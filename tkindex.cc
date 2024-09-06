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



string textFromFile(const std::string& fname, bool pdf)
{
  string command;
  if(pdf)
    command = string("pdftotext -q -nopgbrk - < '") + fname + "' -";
  else
    command = string("pandoc -f docx '"+fname+"' -t plain");

  shared_ptr<FILE> fp(popen(command.c_str(), "r"), pclose);
  if(!fp)
    throw runtime_error("Unable to perform pdftotext: "+string(strerror(errno)));
  char buffer[4096];
  string ret;
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
  string limit="2019-10-01";
  auto wantDocs = todo.queryT("select id,titel,onderwerp from Document where datum > ?", {limit});

  fmt::print("There are {} documents we'd like to index\n", wantDocs.size());
  
  SQLiteWriter sqlw("tkindex.sqlite3");

  sqlw.queryT(R"(
CREATE VIRTUAL TABLE IF NOT EXISTS docsearch USING fts5(onderwerp, titel, tekst, uuid UNINDEXED, tokenize="unicode61 tokenchars '_'")
)");

  auto already = sqlw.queryT("select uuid from docsearch");
  unordered_set<string> skipids;
  for(auto& a : already)
    skipids.insert(get<string>(a["uuid"]));

  fmt::print("{} documents are already indexed & will be skipped\n",
	     skipids.size());
  
  atomic<size_t> ctr = 0;

  std::mutex m;
  atomic<int> skipped=0, notpresent=0, wrong=0, indexed=0;
  auto worker = [&]() {
    for(unsigned int n = ctr++; n < wantDocs.size(); n = ctr++) {
      string id = get<string>(wantDocs[n]["id"]);
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
      string text;
      
      if(isPDF(fname)) {
	text = textFromFile(fname, true);
      }
      else if(isDocx(fname)) {
	text = textFromFile(fname, false);
      }
      else {
	fmt::print("{} is not a file we can deal with\n", fname);
	wrong++;
	continue;
      }

      lock_guard<mutex> p(m);
      string titel;
      try {
	titel = 	  get<string>(wantDocs[n]["titel"]);
      } catch(...){}
      sqlw.queryT("insert into docsearch values (?,?,?,?)", {
	  get<string>(wantDocs[n]["onderwerp"]),
	  titel,
	  text, id});
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
