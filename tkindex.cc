#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <mutex>
#include <iostream>
#include "sqlwriter.hh"
#include <atomic>
#include "support.hh"
#include <unordered_set>

using namespace std;

string textFromFile(const std::string& fname)
{
  string command;
  bool ispdf = false;
  if(isPDF(fname)) {
    command = string("pdftotext -q -nopgbrk - < '") + fname + "' -";
    ispdf = true;
  }
  else if(isDocx(fname)) {
    command = string("pandoc -f docx '"+fname+"' -t plain");
  }
  else if(isXML(fname)) {
    command = string("xmlstarlet tr tk.xslt < '"+fname+"' | sed 's:<[^>]*>: :g'");
  }
  else if(isDoc(fname))
    command = "catdoc - < '" + fname +"'";
  else
    return "";
  string ret;
  for(int tries = 0; tries < 1; ++tries) {
    FILE* pfp = popen(command.c_str(), "r");
    if(!pfp)
      throw runtime_error("Unable to perform pdftotext: "+string(strerror(errno)));
    shared_ptr<FILE> fp(pfp, pclose);

    char buffer[4096];

    for(;;) {
      int len = fread(buffer, 1, sizeof(buffer), fp.get());
      if(!len)
	break;
      ret.append(buffer, len);
    }
    if(ferror(fp.get()))
      throw runtime_error("Unable to perform pdftotext: "+string(strerror(errno)));
    if(!ispdf || !ret.empty())
      break;
    fmt::print("Doing attempt to OCR this PDF {}\n", fname);
    command = fmt::format("ocrmypdf -f -l nld+eng -j 1 {} - | pdftotext - -", fname);
  }
  
  return ret;
}



int main(int argc, char** argv)
{
  SQLiteWriter todo("tk.sqlite3");
  string limit="2008-01-01";
  auto wantDocs = todo.queryT("select id,titel,onderwerp,datum,'Document' as category from Document where datum > ?", {limit});

  fmt::print("There are {} documents we'd like to index\n", wantDocs.size());

  // query voor verslagen is ingewikkeld want we willen alleen de nieuwste versie indexeren
  // en sterker nog alle oude versies wissen
  
  auto alleVerslagen = todo.queryT("select Verslag.id as id, vergadering.id as vergaderingid,datum, vergadering.titel as onderwerp, '' as titel, 'Verslag' as category from Verslag,Vergadering where Verslag.vergaderingId=Vergadering.id and datum > ? order by datum desc, verslag.updated desc", {limit});

  set<string> seenvergadering;
  decltype(alleVerslagen) wantVerslagen;
  for(auto& v: alleVerslagen) {
    string vid = get<string>(v["vergaderingid"]);
    if(seenvergadering.count(vid)) 
      continue;
    wantVerslagen.push_back(v);
    seenvergadering.insert(vid);
  }
  fmt::print("Would like to index {} most recent verslagen\n", wantVerslagen.size());

  string idxfname = argc<2 ? "tkindex.sqlite3" : argv[1];
  SQLiteWriter sqlw(idxfname);

  sqlw.queryT(R"(
CREATE VIRTUAL TABLE IF NOT EXISTS docsearch USING fts5(onderwerp, titel, tekst, uuid UNINDEXED, datum UNINDEXED, category UNINDEXED,  tokenize="unicode61 tokenchars '_'")
)");

  auto already = sqlw.queryT("select uuid from docsearch");
  unordered_set<string> skipids;
  for(auto& a : already)
    skipids.insert(get<string>(a["uuid"]));

  unordered_set<string> dropids;
  
  for(const auto& si : skipids) {
    if(!isPresentNonEmpty(si)) {
      fmt::print("We miss document enclosure for indexed document with id {}\n", si);
      dropids.insert(si);
    }
  }
  fmt::print("{} entries that are indexed have no file enclosure present\n", dropids.size());
  // remove from index?
  fmt::print("{} documents are already indexed & will be skipped\n",
	     skipids.size());

  decltype(wantDocs) wantAll = wantDocs;

  for(const auto& wv : wantVerslagen)
    wantAll.push_back(wv);
  
  atomic<size_t> ctr = 0;

  std::mutex m;
  atomic<int> skipped=0, notpresent=0, wrong=0, indexed=0;
  auto worker = [&]() {
    for(unsigned int n = ctr++; n < wantAll.size(); n = ctr++) {
      string id = get<string>(wantAll[n]["id"]);
      if(skipids.count(id)) {
	//	fmt::print("{} indexed already, skipping\n", id);
	skipped++;
	continue;
      }
      string fname = makePathForId(id);
      if(!isPresentNonEmpty(id)) {
	//	fmt::print("{} is not present\n", id);
	notpresent++;
	continue;
      }
      string text = textFromFile(fname);
      
      if(text.empty()) {
	fmt::print("{} is not a file we can deal with\n", fname);
	wrong++;
	continue;
      }

      lock_guard<mutex> p(m);
      string titel;
      try {
	titel = 	  get<string>(wantAll[n]["titel"]);
      } catch(...){}
      sqlw.queryT("insert into docsearch values (?,?,?,?,?,?)", {
	  get<string>(wantAll[n]["onderwerp"]),
	  titel,
	  text, id, get<string>(wantAll[n]["datum"]), get<string>(wantAll[n]["category"])  });
      indexed++;
    }
  };

  vector<thread> workers;
  for(int n=0; n < 8; ++n)  // number of threads
    workers.emplace_back(worker);
  
  for(auto& w : workers)
    w.join();

  fmt::print("Indexed {} new documents. {} weren't present, {} of unsupported type, {} were indexed already\n",
	     (int)indexed, (int)notpresent, (int)wrong, (int)skipped);
}
