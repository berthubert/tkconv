#include "qparser.hh"

#include "peglib.h"
#include <fmt/core.h>
#include <vector>

using std::string;

static string quote(const string& in) {
  return "\"" + in + "\"";
}

string transformQuery(const string& in,
  std::function<string(const string&)> bareWord,
  std::function<string(const string&)> quotedWord)
{
  peg::parser p;
  p.set_logger([](size_t line, size_t col, const string& msg, const string &rule) {
    fmt::print("line {}, col {}: {}\n", line, col,msg, rule);
  }); // gets us some helpful errors if the grammar is wrong

  // the BareWord is like that because of UTF-8
  auto ret = p.load_grammar(R"a(
    Root <- (Paren / BareWord / QuotedWord)+
    Paren <- ('(' / ')')
    BareWord      <- < [^" ()]+ >
    QuotedWord <-  '"' < [^"]* > '"'
    %whitespace <- [\t ]*
    )a");
  if(!ret)
    throw std::runtime_error("cpp-peglib grammar did not compile");

  p["BareWord"] = [bareWord](const peg::SemanticValues &vs) {
    return bareWord(vs.token_to_string());
  };

  p["QuotedWord"] = [quotedWord](const peg::SemanticValues &vs) {
    return quote(quotedWord(vs.token_to_string()));
  };

  p["Paren"] = [](const peg::SemanticValues &vs) {
    return vs.token_to_string();
  };

  p["Root"] = [](const peg::SemanticValues &vs) {
    return vs.transform<string>();
  };

  std::vector<string> result;
  int rc = p.parse(in, result);

  if(!rc)
    return in; // we tried

  string retval;
  for(const auto& r : result) {
    if(!retval.empty())
      retval.append(1, ' ');
    retval += r;
  }
  return retval;
}

/*
  SQLite FTS5 has some oddities where you can't search for Fox-IT as a bare word,
  because of the dash you must do "Fox-IT".
*/
string convertToSQLiteFTS5(const string& in)
{
  return transformQuery(in, [](const string& s) {
    /*
      As an FTS5 bareword that is not "AND", "OR" or "NOT" (case sensitive). An FTS5 bareword is a string of one or more consecutive characters that are all either:

    Non-ASCII range characters (i.e. unicode codepoints greater than 127), or
    One of the 52 upper and lower case ASCII characters, or
    One of the 10 decimal digit ASCII characters, or
    The underscore character (unicode codepoint 95).
    The substitute character (unicode codepoint 26).
    XXX this is NOT quite what we do!
     */
    if(auto pos = s.find_first_of(",.-[];"); pos != string::npos)
      return quote(s);
    else
      return s;
  });
}
