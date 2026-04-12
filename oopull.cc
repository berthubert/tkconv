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
#include <openssl/evp.h>
#include "argparse/argparse.hpp"
using namespace std;

static string getSHA1HashString(const std::string& in)
{
  unsigned char digest[EVP_MAX_MD_SIZE];
  size_t digest_len = 0;
  
  if(!EVP_Q_digest(nullptr, "SHA1", nullptr,
	       in.c_str(), in.size(),
		   digest, &digest_len))
    throw runtime_error("Calculating SHA1 failed");
  
  string ret;
  for (size_t i = 0; i < digest_len; i++) {
    ret += fmt::sprintf("%02x", digest[i]);
  }
  return ret;
}


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

void doFSCK(const auto& wantDocs)
{
  std::atomic<int> missing=0, wrongHash=0, wrongSize=0, ok=0;
  std::atomic<int> ctr=0;
  std::mutex m;

  ofstream broken("broken.csv");
  broken<<"id,error,dbhash,filehash,dbsize,filesize"<<endl;
  
  auto doCheck=[&]() {
    for(unsigned int n = ctr++; n < wantDocs.size(); n = ctr++) {
      if(!(n % 256)) {
	fmt::print("\rChecked {} OODocuments, {} were mising, {} had the wrong size, {} the wrong hash, {} were ok",
		   n, (int)missing, (int)wrongSize, (int)wrongHash, (int)ok);
	cout.flush();
      }
      const auto& wd = wantDocs[n];
      string reason;
      size_t fsiz=0;
      string hash;
      
      if(!haveExternalIdFile(eget(wd, "id"), "oo", ".pdf", &fsiz)) {
	reason="missing";
	missing++;
      }
      else if(!haveExternalIdFileRightSize(eget(wd, "id"), iget(wd, "grootte"), "oo", ".pdf")) {
	reason="wrongsize";
	wrongSize++;
      }
      else {
	string fname = makePathForExternalID(eget(wd, "id"), "oo", ".pdf");
	string contents = getContentsOfFile(fname);
	hash = getSHA1HashString(contents);
	if(hash == eget(wd, "hash"))
	  ok++;
	else {
	  reason="wronghash";
	  wrongHash++;
	}
      }
      if(!reason.empty()) {
	lock_guard<mutex> p(m);
	broken<<eget(wd, "id")<<","<<reason<<","<<eget(wd, "hash")<<","<<hash<<","<<iget(wd,"grootte")<<","<<fsiz<<endl;
      }
    }
  };

  vector<thread> workers;
  for(int n=0; n < 16; ++n)  // number of threads, go brrr
    workers.emplace_back(doCheck);
  
  for(auto& w : workers)
    w.join();


  fmt::print("\rChecked {} OODocuments, {} were mising, {} had the wrong size, {} wrong hash, {} were ok\n\n",
	     wantDocs.size(), (int)missing, (int)wrongSize, (int)wrongHash, (int)ok);
}

int main(int argc, char** argv)
{
  argparse::ArgumentParser args("oopull", "0.0");

  args.add_argument("--fsck").default_value(false)
    .implicit_value(true).help("Check file sizes and hashes");

  args.add_argument("--begin")
    .help("Begin date of indexing, 2024-12-05 format").default_value("2025-01-01");

  
  try {
    args.parse_args(argc, argv);
  }
  catch (const std::runtime_error& err) {
    std::cout << err.what() << std::endl << args;
    std::exit(1);
  }
  
  SQLiteWriter sqlw("oo.sqlite3", SQLWFlag::ReadOnly);

  string limit = args.get<string>("begin");

  auto wantDocs = sqlw.queryT("select id,bestandsId,grootte,hash,titel from OODocument where openbaarmakingsdatum > ? and verantwoordelijke != 'Tweede Kamer'", {limit});
  cout<<"Got "<<wantDocs.size()<<" documents we need to ensure we have (correctly)"<<endl;
  int error=0, retrieved=0, present=0, wrongSize=0;


  if (args["--fsck"] == true) {
    doFSCK(wantDocs);
    std::exit(0);
  }
  
  int numalready=erase_if(wantDocs, [](auto &wd) {
    return haveExternalIdFile(eget(wd, "id"), "oo", ".pdf");
    //    return haveExternalIdFileRightSize(eget(wd, "id"), iget(wd, "grootte"), "oo", ".pdf");
  });

  set<string> knownbad{"2e094d39-8dde-4ac3-b7d6-bbdf94936359", "2edb5bf0-2cf0-40c8-9c03-3302d144504c", "a296fe12-cedb-4b1f-bec5-3a5ee1198b9b"};
  int numknownbad=erase_if(wantDocs, [&knownbad](auto &wd) {
    return knownbad.count(eget(wd, "id"));
  });

  fmt::print("Already had {} external documents, {} known bad, {} left to look at\n",
	     numalready,
	     numknownbad,
	     wantDocs.size());
  
  int counter=1;
  for(const auto& wd: wantDocs) {
    cout << counter << "/" << wantDocs.size() <<": "<<eget(wd, "id")<<" " << eget(wd, "titel") << " bestandsid " << eget(wd, "bestandsid") << " " << iget(wd, "grootte")<<"\n";
    counter++;
      
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

    if(res->body.size() != (unsigned size_t)iget(wd,"grootte")) {
      cout<<" !!! FILESIZE WRONG!"<<endl;
      wrongSize++;
    }

    if(getSHA1HashString(res->body) != eget(wd,"hash")) {
      cout<<" !!! HASH WRONG"<<endl;
    }
    if(res->body.empty()) {
      fmt::print("Empty response for url {}, not storing\n",
		 url);
      error++;
      continue;
    }
    storeExternalDocument(eget(wd, "id"), ".pdf", res->body);
    retrieved++;
    fmt::print("\n");
    //    fmt::print("Got {} bytes ({}/{}) \n", res->body.size(), retrieved+error+present, wantDocs.size());
    sleep(1);
  }
  fmt::print("Had {} OODocuments to look at, stored {}, {} with wrong file size. {} retrieval failures.\n",
	     wantDocs.size(), retrieved, wrongSize, error);
}
