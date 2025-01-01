#include "support.hh"
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/chrono.h>
#include <sys/stat.h>
#include <vector>
#include <random>
#include "siphash.h"
#include <sclasses.hh>
#include "base64.hpp"
#include "peglib.h"
using namespace std;

static bool fileStartsWith(const std::string& fname, const std::string& start)
{
  vector<char> buf(start.size());
  FILE* fp = fopen(fname.c_str(), "r");
  if(!fp)
    throw runtime_error("Can't check if "+fname+" is a PDF file, "+string(strerror(errno)));
  shared_ptr<FILE> tfp(fp, fclose);
  if(fread(&buf[0], 1, buf.size(), tfp.get()) != buf.size())
    throw runtime_error("Can't read from "+fname+", "+string(strerror(errno)));
  return (memcmp(&buf[0], start.c_str(), start.size())==0);
}


bool isPDF(const std::string& fname)
{
  return fileStartsWith(fname, "%PDF");
}

bool isDocx(const std::string& fname)
{
  return fileStartsWith(fname, "PK");
}
bool isXML(const std::string& fname)
{
  return fileStartsWith(fname, "<?xml") || fileStartsWith(fname, "\xef\xbb\xbf<?xml");
}

bool isDoc(const std::string& fname)
{
  return fileStartsWith(fname, "\xd0\xcf\x11\xe0");
}

bool isRtf(const std::string& fname)
{
  return fileStartsWith(fname, "{\\rtf1");
}

// we add the slash to prefix for you, you need to put the . in the suffix (if you want one)
string makePathForId(const std::string& id, const std::string& prefix, const std::string& suffix, bool makepath)
{
  if(id.size() < 10)
    throw runtime_error("Incomplete ID requested");
  // 92c78e5c-0bc0-4d3e-b1df-d43a100124bb
  if(id.find_first_not_of("0123456789-abcdef") != string::npos)
    throw runtime_error("Invalid ID");
  
  string a = id.substr(0,2);
  string b = id.substr(2,2);

  if(makepath) {
    string dir = prefix;
    int err = mkdir(dir.c_str(), 0770);
    if(err < 0 && errno != EEXIST)
      throw runtime_error("Could not mkdir prefix dir "+dir+": "+strerror(errno));
    dir += "/"+a;
    err = mkdir(dir.c_str(), 0770);
    if(err < 0 && errno != EEXIST)
      throw runtime_error("Could not mkdir storage dir "+dir);
    dir += "/"+b;
    err = mkdir(dir.c_str(), 0770);
    if(err < 0 && errno != EEXIST)
      throw runtime_error("Could not mkdir storage dir "+dir);
  }
  
  return fmt::sprintf("%s/%s/%s/%s%s", prefix,a, b, id, suffix);
}

bool isPresentNonEmpty(const std::string& id, const std::string& prefix, const std::string& suffix)
{
  struct stat sb;
  string fname = makePathForId(id, prefix, suffix);
  int ret = stat(fname.c_str(), &sb);
  if(ret < 0 || sb.st_size == 0)
    return false;
  return true;
}

bool isPresentRightSize(const std::string& id, int64_t size, const std::string& prefix)
{
  struct stat sb;
  string fname = makePathForId(id, prefix);
  int ret = stat(fname.c_str(), &sb);
  if(ret < 0 || sb.st_size != size)
    return false;
  return true;
}

bool cacheIsNewer(const std::string& id, const std::string& cacheprefix, const std::string& suffix, const std::string& docprefix)
{
  string cachename = makePathForId(id, cacheprefix, suffix);
  string origname = makePathForId(id, docprefix);

  struct stat sbcache, sborig;
  int ret = stat(cachename.c_str(), &sbcache);
  if(ret < 0 || sbcache.st_size == 0) // not newer then
    return false;

  ret = stat(origname.c_str(), &sborig);
  if(ret < 0 || sborig.st_size == 0) // if orig is gone, cache is newer!
    return true;

  return sborig.st_mtime < sbcache.st_mtime;
}


uint64_t getRandom64()
{
  static std::random_device rd; // 32 bits at a time. At least on recent Linux and gcc this does not block
  return ((uint64_t)rd() << 32) | rd();
}

string getLargeId()
{
  uint64_t id = getRandom64();
  string ret = base64::to_base64(std::string((char*)&id, sizeof(id)));
  ret.resize(ret.size()-1); // this base64url implementation pads, somehow
  id = getRandom64();
  ret += base64::to_base64(std::string((char*)&id, sizeof(id)));
  ret.resize(ret.size()-1); // this base64url implementation pads, somehow

  for(auto& c : ret) {
    if(c == '/')
      c = '_';
    else if(c == '+')
      c = '-';
  }
  return ret;
}


// Function to check if a string ends with a particular suffix
bool endsWith(const std::string& str, const std::string& suffix) {
    // Check if the suffix is longer than the string itself
    if (suffix.size() > str.size()) {
        return false;
    }

    // Compare the end of the string with the suffix
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

string makePathForExternalID(const std::string& id, const std::string& prefix, const std::string& suffix, bool makepath)
{
  if(id.find_first_of("./") != string::npos)
    throw runtime_error("id for external file contained bad characters");

  if(makepath) {
    string fname = prefix;
    int err = mkdir(fname.c_str(), 0770);
    if(err < 0 && errno != EEXIST) 
      throw runtime_error("Could not mkdir prefix dir "+fname+": "+strerror(errno));
    
    string subdir=getSubdirForExternalID(id);
    fname += "/" + subdir.substr(0,2);
    
    err= mkdir(fname.c_str(), 0770);
    
    if(err < 0 && errno != EEXIST) 
      throw runtime_error("Could not mkdir prefix dir "+fname+": "+strerror(errno));
    
    fname += "/" + subdir.substr(3);
    err= mkdir(fname.c_str(), 0770);
    if(err < 0 && errno != EEXIST) 
      throw runtime_error("Could not mkdir prefix dir "+fname+": "+strerror(errno));
  }
  
  return prefix + "/" + getSubdirForExternalID(id)+"/" +id + suffix;
}

bool haveExternalIdFile(const std::string& id, const std::string& prefix, const std::string& suffix)
{
  if(id.find_first_of("./") != string::npos)
    return false;
  
  struct stat sb;
  string fname = prefix + "/" + getSubdirForExternalID(id)+"/" +id + suffix;
  int ret = stat(fname.c_str(), &sb);

  if(ret < 0 || sb.st_size == 0)
    return false;
  return true;
}


/*int siphash(const void *in, const size_t inlen, const void *k, uint8_t *out,
            const size_t outlen);

    Computes a SipHash value
    *in: pointer to input data (read-only)
    inlen: input data length in bytes (any size_t value)
    *k: pointer to the key data (read-only), must be 16 bytes 
    *out: pointer to output data (write-only), outlen bytes must be allocated
    outlen: length of the output in bytes, must be 8 or 16
*/

// returns "f3/12", used for external id's 
string getSubdirForExternalID(const std::string& in)
{
  static unsigned char k[16]={1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  unsigned char out[8]={};
  int outlen = sizeof(out);

  siphash((const void*) in.c_str(), in.length(), k, out, outlen);
  return fmt::sprintf("%02x/%02x", out[4], out[6]);
}

time_t getTstamp(const std::string& str)
{
  //  2024-09-17T13:00:00
  //  2024-09-17T13:00:00+0200
  struct tm tm={};
  strptime(str.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
  
  return timelocal(&tm);
}

time_t getTstampUTC(const std::string& str)
{
  //  2024-09-17T13:00:00Z
  struct tm tm={};
  strptime(str.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
  
  return timegm(&tm);
}

// do not put \r in in
string toQuotedPrintable(const std::string& in)
{
  string out;
  string line;

  for(const auto& c: in) {
    if(c=='\n') {
      out += line;
      out += "\r\n"; // really
      line.clear();
      continue;
    }
    string part;
    if(isprint(c) && c != '=')
      part=c;
    else
      part = fmt::sprintf("=%02X", (int)(unsigned char)c);
    
    if(line.length() + part.length() >= 76) {
      out += line;
      out += "=\r\n";
      line.clear();
    }
    line += part;
  }
  out += line;
  return out;
}

void sendEmail(const std::string& server, const std::string& from, const std::string& to, const std::string& subject, const std::string& textBody, const std::string& htmlBody)
{
  const char* allowed="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+-.@=";
  if(from.find_first_not_of(allowed) != string::npos || to.find_first_not_of(allowed) != string::npos) {
    throw std::runtime_error("Illegal character in from or to address");
  }

  ComboAddress mailserver(server, 25);
  Socket s(mailserver.sin4.sin_family, SOCK_STREAM);

  SocketCommunicator sc(s);
  sc.connect(mailserver);
  string line;
  auto sponge= [&](int expected) {
    while(sc.getLine(line)) {
      if(line.size() < 4)
        throw std::runtime_error("Invalid response from SMTP server: '"+line+"'");
      if(stoi(line.substr(0,3)) != expected)
        throw std::runtime_error("Unexpected response from SMTP server: '"+line+"'");
      if(line.at(3) == ' ')
        break;
    }
  };

  sponge(220);
  sc.writen("EHLO outer2.berthub.eu\r\n");
  sponge(250);

  sc.writen("MAIL From:<"+from+">\r\n");
  sponge(250);

  sc.writen("RCPT To:<"+to+">\r\n");
  sponge(250);

  sc.writen("DATA\r\n");
  sponge(354);
  sc.writen("From: "+from+"\r\n");
  sc.writen("To: "+to+"\r\n");

  bool needb64 = false;
  for(const auto& c : subject) {
    if(c < 32 || (unsigned char)c > 127) {
      needb64 = true;
      break;
    }
  }
  string esubject;
  if(needb64)
    esubject = "=?utf-8?B?"+base64::to_base64(subject)+"?=";
  else
    esubject = subject;

  
  sc.writen("Subject: "+esubject+"\r\n");
  

  sc.writen(fmt::format("Message-Id: <{}@opentk.hostname>\r\n", getRandom64()));
  
  //Date: Thu, 28 Dec 2023 14:31:37 +0100 (CET)
  sc.writen(fmt::format("Date: {:%a, %d %b %Y %H:%M:%S %z (%Z)}\r\n", fmt::localtime(time(0))));

  sc.writen("Auto-Submitted: auto-generated\r\nPrecedence: bulk\r\n");

  string sepa="_----------=_MCPart_"+getLargeId();
  if(htmlBody.empty()) {
    sc.writen("Content-Type: text/plain; charset=\"utf-8\"\r\n");
    sc.writen("Content-Transfer-Encoding: quoted-printable\r\n");
  }
  else {
    sc.writen("Content-Type: multipart/alternative; boundary=\""+sepa+"\"\r\n");
    sc.writen("MIME-Version: 1.0\r\n");
  }
  sc.writen("\r\n");


  if(!htmlBody.empty()) {
    sc.writen("This is a multi-part message in MIME format\r\n\r\n");

    sc.writen("--"+sepa+"\r\n");
    sc.writen("Content-Type: text/plain; charset=\"utf-8\"; format=\"fixed\"\r\n");
    sc.writen("Content-Transfer-Encoding: quoted-printable\r\n\r\n");
  }
  string qp = toQuotedPrintable(textBody);

  sc.writen(qp +"\r\n");
  
  if(htmlBody.empty()) {
    sc.writen("\r\n.\r\n");
    sponge(250);
    return;
  }
  sc.writen("--"+sepa+"\r\n");
  sc.writen("Content-Type: text/html; charset=\"utf-8\"\r\n");
  sc.writen("Content-Transfer-Encoding: base64\r\n\r\n");
  int linelen = 76;
  string b64 = base64::to_base64(htmlBody);
  int pos = 0;
  for(pos = 0 ; pos < (int)b64.length() - linelen; pos += linelen) {
    sc.writen(b64.substr(pos, linelen)+"\r\n");
  }
  sc.writen(b64.substr(pos) +"\r\n");
  sc.writen("--"+sepa+"--\r\n.\r\n");
  sponge(250);
  return;
}


void replaceSubstring(std::string &originalString, const std::string &searchString, const std::string &replaceString) {
  size_t pos = originalString.find(searchString);
  
  while (pos != std::string::npos) {
    originalString.replace(pos, searchString.length(), replaceString);
    pos = originalString.find(searchString, pos + replaceString.length());
  }
}

std::string htmlEscape(const std::string& data)
{
  std::string buffer;
  buffer.reserve(1.1*data.size());
  for(size_t pos = 0; pos != data.size(); ++pos) {
    switch(data[pos]) {
    case '&':  buffer.append("&amp;");       break;
    case '\"': buffer.append("&quot;");      break;
    case '\'': buffer.append("&apos;");      break;
    case '<':  buffer.append("&lt;");        break;
    case '>':  buffer.append("&gt;");        break;
    default:   buffer.append(&data[pos], 1); break;
    }
  }
  return buffer;
}

std::string urlEscape(const std::string& data)
{
  std::string buffer;
  buffer.reserve(1.1*data.size());
  for(const auto& c : data) {
    if(!isalnum(c) && (c!= '-' && c != '.' && c !='_' && c != '~'))
      buffer += fmt::sprintf("%%%02x", (unsigned int) c);
    else
      buffer.append(1, c); 
  }
  return buffer;
}


std::string getTimeDBFormat(time_t t)
{
  return fmt::format("{:%Y-%m-%d}", fmt::localtime(t));
}


std::string getTodayDBFormat()
{
  return getTimeDBFormat(time(0));
}

/*
  SQLite FTS5 has some oddities where you can't search for Fox-IT as a bare word,
  because of the dash you must do "Fox-IT".
*/
string convertToSQLiteFTS5(const std::string& in)
{
  peg::parser p;
  p.set_logger([](size_t line, size_t col, const string& msg, const string &rule) {
    fmt::print("line {}, col {}: {}\n", line, col,msg, rule);
  }); // gets us some helpful errors if the grammar is wrong

  // sequence of words AND "words" - add quotes to everything not quoted with a . or - in there

  // the BareWord is like that because of UTF-8
  auto ret = p.load_grammar(R"a(
Root <- (Paren / BareWord / QuotedWord)+
Paren <- ('(' / ')')
BareWord      <- < [^" ()]+ > 
QuotedWord <-  < '"'  [^"]*  '"' > 
%whitespace <- [\t ]*
)a");
  if(!ret)
    throw runtime_error("cpp-peglib grammar did not compile");

  p["BareWord"] = [](const peg::SemanticValues &vs) {
    if(auto pos = vs.token_to_string().find_first_of(",.-"); pos != string::npos) {
      return "\"" + vs.token_to_string() +"\"";
    }
    else
      return vs.token_to_string();
  };

  p["QuotedWord"] = [](const peg::SemanticValues &vs) {
    return vs.token_to_string();
  };

  p["Paren"] = [](const peg::SemanticValues &vs) {
    return vs.token_to_string();
  };

  
  p["Root"] = [](const peg::SemanticValues &vs) {
    return vs.transform<string>();
  };
  vector<string> result;
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
