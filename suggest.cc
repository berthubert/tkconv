#include "suggest.hh"

using std::nullptr_t;
using std::string;

#include "fstlib.h"
#include "peglib.h"
#include "sqlwriter.hh"

string Suggester::spell(string q) {
  string best_term = q;
  size_t best_docs = 0;

  if(!q.empty() && !bytes.empty()) {
    fst::map<uint64_t> dictionary(bytes.data(), bytes.size());

    if(!dictionary.contains(q)) {
      // of words within edit distance, return the one that appears in most
      // documents (and thus the one most likely to give results)
      for(const auto &[term, docs] : dictionary.edit_distance_search(q, 2)) {
        if(docs >= best_docs) {
          best_term = term;
          best_docs = docs;
        }
      }
    }
  }

  return best_term;
}

string Suggester::correct_query(string in) {
  peg::parser p;

  // the BareWord is like that because of UTF-8
  auto ret = p.load_grammar(R"a(
    Root <- (Paren / BareWord / QuotedWord)+
    Paren <- ('(' / ')')
    BareWord      <- < [^" ()]+ > 
    QuotedWord <-  '"'  < [^"]* >  '"' 
    %whitespace <- [\t ]*
    )a");
  if(!ret)
    throw std::runtime_error("bad grammar");

  p["BareWord"] = [this](const peg::SemanticValues &vs) {
    return spell(vs.token_to_string());
  };

  p["QuotedWord"] = [this](const peg::SemanticValues &vs) {
    return "\"" + spell(vs.token_to_string()) + "\"";
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

Suggester suggester_from_pairs(const std::vector<std::pair<string, uint64_t>> &pairs) {
  std::stringstream out;

  auto [result, _] = fst::compile<uint64_t>(pairs, out, true);

  if(result != fst::Result::Success)
    throw std::runtime_error("Suggester could not build FST (not sorted?)");

  return {out.str()};
}

Suggester suggester_from_table(SQLiteWriter *sql) {
  std::vector<std::pair<string, uint64_t>> pairs;

  auto rows = sql->queryT("select term as term, doc as doc from lexicon order by term");

  pairs.reserve(rows.size());

  for(const auto& row : rows) {
    string term = std::get<string>(row.at("term"));
    uint64_t doc = std::get<int64_t>(row.at("doc"));
    pairs.push_back({term, doc});
  }

  return suggester_from_pairs(pairs);
}
