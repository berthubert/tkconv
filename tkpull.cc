#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <mutex>
#include <iostream>
#include "sqlwriter.hh"
#include <atomic>

#include "httplib.h"
#include <set>
#include "support.hh"

using namespace std;
void storeDocument(const std::string& id, const std::string& content)
{
  string fname=makePathForId(id);
  FILE* t = fopen((fname+".tmp").c_str(), "w");
  if(!t)
    throw runtime_error("Unable to open file "+fname+": "+string(strerror(errno)));
  
  shared_ptr<FILE> fp(t, fclose);
  if(fwrite(content.c_str(), 1, content.size(), fp.get()) != content.size()) {
    unlink(fname.c_str());
    throw runtime_error("Partial write to file "+fname);
  }
  fp.reset();
  
  if(rename((fname+".tmp").c_str(), fname.c_str()) < 0) {
    int e = errno;
    unlink((fname+".tmp").c_str());
    throw runtime_error("Unable to rename saved file "+fname+".tmp - "+strerror(e));
  }
}

int main(int argc, char** argv)
{
  SQLiteWriter sqlw("tk.sqlite3");

  int sizlim = 12500000;
  string limit="2008-01-01";
  auto wantDocs = sqlw.queryT("select id,enclosure,contentLength from Document where datum > ? and contentLength < ?", {limit, sizlim});
  limit="2021-11-01";
  auto alleVerslagen = sqlw.queryT("select Verslag.id as id, vergadering.id as vergaderingid,enclosure,contentLength,datum from Verslag,Vergadering where Verslag.vergaderingId=Vergadering.id and datum > ? order by datum desc, verslag.updated desc", {limit});

  
  set<string> seenvergadering;
  decltype(alleVerslagen) wantVerslagen, todelete;
  for(auto& v: alleVerslagen) {
    string vid = get<string>(v["vergaderingid"]);
    if(seenvergadering.count(vid)) {
      todelete.push_back(v);
      continue;
    }
    wantVerslagen.push_back(v);
    seenvergadering.insert(vid);
  }
  fmt::print("We desire {} documents and {} verslagen, and found {} older verslagen\n", wantDocs.size(), wantVerslagen.size(), todelete.size());

  int unlinked=0;
  for(auto& td : todelete) {
    string verslagid=get<string>(td["id"]);
    string fname=makePathForId(verslagid);
    int rc = unlink(fname.c_str());
    if(!rc)
      unlinked++;
    else {
      if(errno != ENOENT)
	fmt::print("Error removing file {}: {}\n", fname, strerror(errno));
    }
  }
  fmt::print("{} niet-nieuwste versies van verslagen gewist\n", unlinked);

  struct RetStore
  {
    string id;
    string enclosure;
    int64_t contentLength;
    bool operator<(const RetStore& rhs) const
    {
      return id < rhs.id;
    }
  };

  
  for(auto store : {&wantDocs, &wantVerslagen}) {
    int present=0;
    int toolarge=0, retrieved=0;

    cout<<"Starting from a store, got "<<store->size()<<" docs to go"<<endl;
    set<RetStore> toRetrieve;
    for(auto& d : *store) {
      if(isPresentRightSize(get<string>(d["id"]), get<int64_t>(d["contentLength"]))  )
	present++;
      else {
	auto contentLength = get_if<int64_t>(&d["contentLength"]);
	toRetrieve.insert({get<string>(d["id"]), get<string>(d["enclosure"]),
	    contentLength ? *contentLength : 0});
      }
    }
    fmt::print("We have {} files to retrieve, {} are already present\n", toRetrieve.size(), present);

    for(const auto& need : toRetrieve) {
      if(!need.contentLength || need.contentLength > sizlim) {
	toolarge++;
	fmt::print("Skipping {}, too large for server ({}) or unknown\n",
		   need.id, need.contentLength);
	continue;
      }
      
      httplib::Client cli("https://gegevensmagazijn.tweedekamer.nl");
      cli.set_connection_timeout(10, 0); 
      cli.set_read_timeout(10, 0); 
      cli.set_write_timeout(10, 0); 
      
      fmt::print("Retrieving from {} (expect {} bytes).. ", need.enclosure, need.contentLength);
      cout.flush();
      auto res = cli.Get(need.enclosure);
      
      if(!res) {
	auto err = res.error();
	throw runtime_error("Oops retrieving from "+ need.enclosure +" -> "+httplib::to_string(err));
      }
      fmt::print("Got {} bytes\n", res->body.size());
      storeDocument(need.id, res->body);
      retrieved++;
      usleep(100000);
    }
    fmt::print("Retrieved {} documents, {} were too large\n", retrieved, toolarge);
  }
}