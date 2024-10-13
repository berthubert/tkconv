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

static string textFromFile(const std::string& fname)
{
  string command;
  if(isPDF(fname)) {
    command = string("pdftotext -q -nopgbrk - < '") + fname + "' -";
  }
  else if(isDocx(fname)) {
    command = string("pandoc -f docx '"+fname+"' -t plain");
  }
  else if(isXML(fname)) {
    command = string("xmlstarlet tr tk.xslt < '"+fname+"' | sed 's:<[^>]*>: :g'");
  }
  else if(isDoc(fname))
    command = "catdoc - < '" + fname +"'";
  else if(isRtf(fname))
    command = string("pandoc -f rtf '"+fname+"' -t plain");
  else
    return "";
  
  string ret;
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
  return ret;
}


int main(int argc, char** argv)
{
  SQLiteWriter todo("tk.sqlite3");
  string limit="2008-01-01";
  fmt::print("Getting docs since {}\n", limit);
  auto wantDocs = todo.queryT("select id,titel,onderwerp,datum,'Document' as category, contentLength from Document where datum > ?", {limit});

  fmt::print("There are {} documents we'd like to index\n", wantDocs.size());

  // query voor verslagen is ingewikkeld want we willen alleen de nieuwste versie indexeren
  // en sterker nog alle oude versies wissen
  fmt::print("Getting verslagen since {}\n", limit);
  auto alleVerslagen = todo.queryT("select Verslag.id as id, vergadering.id as vergaderingid,datum, vergadering.titel as onderwerp, '' as titel, 'Verslag' as category, contentLength from Verslag,Vergadering where Verslag.vergaderingId=Vergadering.id and datum > ? order by datum desc, verslag.updated desc", {limit});

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
CREATE VIRTUAL TABLE IF NOT EXISTS docsearch USING fts5(onderwerp, titel, tekst, contentLength UNINDEXED, uuid UNINDEXED, datum UNINDEXED, category UNINDEXED,  tokenize="unicode61 tokenchars '_'")
)");

  fmt::print("Retrieving already indexed document uuids\n");
  auto already = sqlw.queryT("select uuid,contentLength from docsearch");
  unordered_map<string, int64_t> skipids;
  for(auto& a : already) {
    skipids[get<string>(a["uuid"])] = get<int64_t>(a["contentLength"]);
  }

  unordered_set<string> dropids, reindex;
  
  for(const auto& si : skipids) {
    if(!isPresentNonEmpty(si.first)) {
      fmt::print("We miss document enclosure for indexed document with id {}\n", si.first);
      dropids.insert(si.first); 
    }
    else if(!isPresentRightSize(si.first, si.second)) {
      fmt::print("Document enclosure for indexed document with id {} is wrong size, reindexing\n", si.first);
      reindex.insert(si.first); 
    }
  }
  fmt::print("{} entries that are indexed have no file enclosure present\n", dropids.size());
  fmt::print("{} entries that are indexed have incorrectly sized enclosure, reindexing\n", reindex.size());

  for(const auto& di : dropids) {
    fmt::print("Removing absent {} from index\n", di);
    sqlw.queryT("delete from docsearch where uuid=?", {di});
  }
  for(const auto& di : reindex) {
    fmt::print("Removing wrongly sized {} from index\n", di);
    sqlw.queryT("delete from docsearch where uuid=?", {di});
    skipids.erase(di);
  }

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
	if(isPresentNonEmpty(id, "improvdocs")) {
	  string impfname = makePathForId(id, "improvdocs");
	  
	  text = textFromFile(impfname);
	  if(!text.empty()) {
	    fmt::print("{} did work using improvdocs overlay!\n", id);
	  }
	  else {
	    fmt::print("{} is not a file we can deal with {}\n", fname, isPDF(impfname) ? "PDF" : "");
	    wrong++;
	    continue;
	  }
	}
	else {
	  fmt::print("{} is not a file we can deal with {}\n", fname, isPDF(fname) ? "PDF" : "");
	  wrong++;
	  continue;
	}
      }

      lock_guard<mutex> p(m);
      string titel;
      try {
	titel = 	  get<string>(wantAll[n]["titel"]);
      } catch(...){}
      sqlw.queryT("insert into docsearch values (?,?,?,?,?,?,?)", {
	  get<string>(wantAll[n]["onderwerp"]),
	  titel,
	  text,
	  get<int64_t>(wantAll[n]["contentLength"]),
	  id, get<string>(wantAll[n]["datum"]), get<string>(wantAll[n]["category"])  });
      indexed++;
    }
  };

  vector<thread> workers;
  for(int n=0; n < 8; ++n)  // number of threads
    workers.emplace_back(worker);
  
  for(auto& w : workers)
    w.join();

  fmt::print("Indexed {} new documents, of which {} were reindexes. {} weren't present, {} of unsupported type, {} were indexed already\n",
	     (int)indexed, reindex.size(), (int)notpresent, (int)wrong, (int)skipped);
}
