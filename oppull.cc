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
void storeExternalDocument(const std::string& id, string suffix, const std::string& content)
{
  string fname = makePathForExternalID(id, "op", suffix, true);
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

bool knownMissing(const std::string& id)
{
  static unordered_set<string> missing{"h-tk-20232024-50-2-n1", "h-tk-20232024-50-3-n1", "h-tk-20232024-50-4-n1",
"h-tk-20232024-50-7-n1", "h-tk-20232024-50-6-n1", "h-tk-20232024-50-9-n1",
"h-tk-20232024-50-8-n1", "h-tk-20232024-50-11-n1", "h-tk-20232024-50-13-n1",
"h-tk-20232024-50-12-n1", "h-tk-20232024-50-16-n1",
"h-tk-20232024-50-18-n1", "h-tk-20232024-50-17-n1",
"h-tk-20232024-50-19-n1", "h-tk-20232024-50-20-n1",
"h-tk-20232024-50-22-n1", "h-tk-20232024-50-25-n1",
"h-tk-20232024-50-23-n1", "h-tk-20232024-50-24-n1",
"h-tk-20232024-50-21-n1", "h-tk-20232024-50-26-n1",
"h-tk-20232024-50-10-n1", "h-tk-20232024-50-15-n1",
"h-tk-20232024-50-14-n1", "h-tk-20232024-50-28-n1",
"h-tk-20232024-50-29-n1", "h-tk-20232024-50-30-n1", "h-tk-20232024-52-3-n1",
"h-tk-20232024-52-4-n1", "h-tk-20232024-52-5-n1", "h-tk-20232024-52-6-n1",
"h-tk-20232024-52-11-n1", "h-tk-20232024-52-7-n1", "h-tk-20232024-52-8-n1",
"h-tk-20232024-52-10-n1", "h-tk-20232024-52-12-n1", "h-tk-20232024-53-2-n1",
"h-tk-20232024-53-4-n1", "h-tk-20232024-53-3-n1", "h-tk-20232024-53-7-n1",
"h-tk-20232024-53-10-n1", "h-tk-20232024-53-9-n1", "h-tk-20232024-53-17-n1",
"h-tk-20232024-53-26-n1", "h-tk-20232024-53-16-n1",
"h-tk-20232024-53-20-n1", "h-tk-20232024-53-15-n1",
"h-tk-20232024-53-18-n1", "h-tk-20232024-53-11-n1",
"h-tk-20232024-53-29-n1", "h-tk-20232024-53-23-n1",
"h-tk-20232024-53-22-n1", "h-tk-20232024-53-13-n1",
"h-tk-20232024-53-14-n1", "h-tk-20232024-53-19-n1",
"h-tk-20232024-53-12-n1", "h-tk-20232024-53-27-n1",
"h-tk-20232024-53-25-n1", "h-tk-20232024-53-28-n1", "h-tk-20232024-53-8-n1",
"h-tk-20232024-53-24-n1", "h-tk-20232024-53-6-n1", "h-tk-20232024-53-21-n1",
"h-tk-20232024-53-31-n1", "h-tk-20232024-53-32-n1", "h-tk-20232024-54-3-n1",
"h-tk-20232024-54-6-n1", "h-tk-20232024-54-5-n1", "h-tk-20232024-54-7-n1",
"h-tk-20232024-54-8-n1", "h-tk-20232024-54-9-n1", "h-tk-20232024-54-10-n1",
"h-tk-20232024-54-11-n1", "h-tk-20232024-55-3-n1", "h-tk-20232024-55-4-n1",
"h-tk-20232024-55-5-n1", "h-tk-20232024-55-6-n1", "h-tk-20232024-55-8-n1",
"h-tk-20232024-55-9-n1", "h-tk-20232024-55-10-n1", "h-tk-20232024-55-11-n1",
"h-tk-20232024-55-12-n1", "h-tk-20232024-55-13-n1", "h-tk-20232024-56-3-n1",
"h-tk-20232024-56-2-n1", "h-tk-20232024-56-4-n1", "h-tk-20232024-56-7-n1",
"h-tk-20232024-56-8-n1", "h-tk-20232024-56-10-n1", "h-tk-20232024-56-11-n1",
"h-tk-20232024-56-14-n1", "h-tk-20232024-56-13-n1",
"h-tk-20232024-56-17-n1", "h-tk-20232024-56-18-n1",
"h-tk-20232024-56-15-n1", "h-tk-20232024-56-16-n1",
"h-tk-20232024-56-19-n1", "h-tk-20232024-56-20-n1", "h-tk-20232024-56-6-n1",
"h-tk-20232024-56-12-n1", "h-tk-20232024-56-9-n1", "h-tk-20232024-56-22-n1",
"h-tk-20232024-56-23-n1", "h-tk-20232024-56-24-n1",
"h-tk-20232024-56-25-n1", "h-tk-20232024-56-26-n1", "h-tk-20232024-57-3-n1",
"h-tk-20232024-57-4-n1", "h-tk-20232024-57-5-n1", "h-tk-20232024-57-6-n1",
"h-tk-20232024-57-8-n1", "h-tk-20232024-57-10-n1", "h-tk-20232024-57-11-n1",
"h-tk-20232024-57-12-n1", "h-tk-20232024-57-13-n1", "h-tk-20232024-58-4-n1",
"h-tk-20232024-58-3-n1", "h-tk-20232024-58-5-n1", "h-tk-20232024-58-8-n1",
"h-tk-20232024-58-7-n1", "h-tk-20232024-58-9-n1"};
    return missing.count(id);
}

int main(int argc, char** argv)
{
  SQLiteWriter sqlw("tk.sqlite3");
  string ftype="odt";
  if(argc > 1)
    ftype=argv[1];
  string suffix ="."+ftype;
  string limit="2023-01-01";
  if(argc > 2)
    limit = argv[2];
  auto wantDocs = sqlw.queryT("select externeidentifier from Document,DocumentVersie where documentid=document.id and document.datum > ? and externeidentifier != ''", {limit});
  cout<<"Got "<<wantDocs.size()<<" external identifiers to look at"<<endl;
  
  int present=0, retrieved=0, error=0;

  int numalready=erase_if(wantDocs, [&suffix](auto &wd) {
    string eid = get<string>(wd["externeidentifier"]);
    return haveExternalIdFile(eid, "op", suffix) || knownMissing(eid);
  });
  fmt::print("Already had {} external documents, {} left to look at\n", numalready, wantDocs.size());
  for(auto& wd : wantDocs) {
    
    string eid = get<string>(wd["externeidentifier"]);

    string url="https://repository.officiele-overheidspublicaties.nl";
      //    string url="https://zoek.officielebekendmakingen.nl";
    httplib::Client cli(url);
    cli.set_connection_timeout(10, 0); 
    cli.set_read_timeout(10, 0); 
    cli.set_write_timeout(10, 0);
    cli.set_follow_location(true);
    url += "/officielepublicaties/"+eid+"/"+eid+suffix;
    fmt::print("Retrieving from {}  ", url);
    cout.flush();
    auto res = cli.Get(url);
    
    if(!res) {
      auto err = res.error();
      fmt::print("Oops retrieving from {} -> {}\n", url, httplib::to_string(err));
      error++;
      continue;
    }
    if(res->status != 200) {
      fmt::print("Wrong status code {} for url {}, not storing\n",
		 res->status, url);
      error++;
      continue;
    }
    if(res->location.find("fout404") != string::npos) {
      fmt::print("Fake 404 detected, skipping\n");
      continue;
    }

    storeExternalDocument(eid, "."+ftype, res->body);
    retrieved++;
    fmt::print("Got {} bytes ({}/{}) \n", res->body.size(), retrieved+error+present, wantDocs.size());
    usleep(10000);
  }
  fmt::print("Retrieved {} documents, {} were present already, {} errors\n", retrieved, present, error);
}
