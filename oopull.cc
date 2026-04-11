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

static void ifExistsAndDifferentThenRename(const std::string& fname, const std::string& content)
{
  struct stat sb;
  if(stat(fname.c_str(), &sb) < 0)
    return;

  if((long unsigned int)sb.st_size == content.size())
    return; // we cheat a bit and don't actually check content
  
  string newname = fmt::sprintf("%s.%d", fname, sb.st_mtime);
  if(rename(fname.c_str(), newname.c_str()) == 0) {
    fmt::print("Already had a file for {}, renamed to {}\n",
		fname, newname);
  }
}

// this knows that the id needs additional hashing
void storeExternalDocument(const std::string& id, string suffix, const std::string& content)
{
  string fname = makePathForExternalID(id, "oo", suffix, true);

  ifExistsAndDifferentThenRename(fname, content);
  
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
  SQLiteWriter sqlw("oo.sqlite3", SQLWFlag::ReadOnly);

  string limit="2026-01-01";
  auto wantDocs = sqlw.queryT("select id,bestandsId,grootte,hash,titel from OODocument where openbaarmakingsdatum > ? and verantwoordelijke != 'Tweede Kamer' and verantwoordelijke not like 'gemeente %'", {limit});
  cout<<"Got "<<wantDocs.size()<<" documents we need to ensure we have (correctly)"<<endl;
  int error=0, retrieved=0, present=0;
  
  int numalready=erase_if(wantDocs, [](auto &wd) {
    return haveExternalIdFileRightSize(eget(wd, "id"), iget(wd, "grootte"), "oo", ".pdf");
  });

  set<string> knownbad{"2e094d39-8dde-4ac3-b7d6-bbdf94936359", "2edb5bf0-2cf0-40c8-9c03-3302d144504c", "a296fe12-cedb-4b1f-bec5-3a5ee1198b9b"};
  int numknownbad=erase_if(wantDocs, [&knownbad](auto &wd) {
    return knownbad.count(eget(wd, "id"));
  });

  fmt::print("Already had {} external documents, {} known bad, {} left to look at\n",
	     numalready,
	     numknownbad,
	     wantDocs.size());
  
  
  for(const auto& wd: wantDocs) {
    cout << eget(wd, "id")<<" " << eget(wd, "titel") << " bestandsid " << eget(wd, "bestandsid") << " " << iget(wd, "grootte")<<"\n";
    
    string url="https://open.overheid.nl";
    httplib::Client cli(url);
    cli.set_connection_timeout(10, 0); 
    cli.set_read_timeout(10, 0); 
    cli.set_write_timeout(10, 0);
    cli.set_follow_location(true);
    url = eget(wd, "bestandsid");
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

    storeExternalDocument(eget(wd, "id"), ".pdf", res->body);
    retrieved++;
    fmt::print("Got {} bytes ({}/{}) \n", res->body.size(), retrieved+error+present, wantDocs.size());
    sleep(1);
  }    
}
