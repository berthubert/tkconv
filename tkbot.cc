#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <fmt/chrono.h>
#include <iostream>
#include "httplib.h"
#include "sqlwriter.hh"
#include "inja.hpp"
#include "pugixml.hpp"
#include "thingpool.hh"
#include "support.hh"
#include "scanmon.hh"
using namespace std;

bool emitIfNeeded(SQLiteWriter& sqlw, const ScannerHit& sh, const Scanner& sc)
{
  try {
    auto h = sqlw.query("select identifier from sentNotification where userid=? and identifier=?",
		      {sc.d_userid, sh.identifier});
    if(!h.empty())
      return false;
  }
  catch(...) {
  }
  return true;
}

void logEmission(SQLiteWriter& sqlw, const ScannerHit&sh, const Scanner& sc)
{
  string when = fmt::format("{:%Y-%m-%dT%H:%M:%S}", fmt::localtime(time(0)));
  sqlw.addValue({{"identifier", sh.identifier}, {"userid", sc.d_userid}, {"soort", sc.d_soort}, {"timestamp", when}, {"scannerId", sc.d_id}}, "sentNotification");

}

void updateScannerDate(SQLiteWriter& sqlw, const Scanner& sc)
{
  string cutoff = getTodayDBFormat();
  sqlw.queryT("update scanners set cutoff=? where rowid=?", {cutoff, sc.d_id});
}

string getDocDescription(SQLiteWriter& sqlw, const std::string& nummer)
{
  auto res = sqlw.queryT("select onderwerp,titel from Document where nummer=?",
			 {nummer});
  if(res.empty()) {
    res = sqlw.queryT("select titel from Vergadering where id=?",
			 {nummer});
    if(res.empty()) {
      res = sqlw.queryT("select soort||' '||onderwerp as onderwerp,datum from Activiteit where nummer=?",
			 {nummer});
      if(res.empty())
	return "";
      string resp = eget(res[0], "onderwerp");
      string datum = eget(res[0], "datum");
      if(!datum.empty()) {
	datum[10]= ' ';
	resp += " (" +datum+")";
      }
      else resp += " (nog geen datum)";
      return resp;
    }
    return eget(res[0], "titel");
  }
  return eget(res[0], "onderwerp");
}

string getEmailForUserId(SQLiteWriter& sqlw, const std::string& userid)
{
  auto res = sqlw.queryT("select email from users where user=?", {userid});
  if(res.empty())
    throw runtime_error("No email for userid '"+userid+"'");
  return eget(res[0], "email");
}

int main(int argc, char** argv)
{
  SQLiteWriter sconfig("user.sqlite3");  

  auto toscan=sconfig.queryT("select rowid,* from scanners");
  vector<unique_ptr<Scanner>> scanners;
  for(auto& ts: toscan) {
    if(auto iter = g_scanmakers.find(eget(ts,"soort")); iter != g_scanmakers.end()) {
      scanners.push_back(iter->second(sconfig, get<string>(ts["id"])));
    }
  }
      
  ThingPool<SQLiteWriter> tp("tk.sqlite3");
  //   user       doc         scanner
  map<string, map<string, set<Scanner*>>> all;
  for(auto& scanner : scanners) {
    fmt::print("{}\n", scanner->describe(tp.getLease().get()));
    try {
      auto ds = scanner->get(tp.getLease().get());
      for(const auto& d: ds) {
	if(emitIfNeeded(sconfig, d, *scanner.get())) {
	  fmt::print("\tNummer {}\n", d.identifier);
	  all[scanner->d_userid][d.identifier].insert(scanner.get());
	  logEmission(sconfig, d, *scanner.get());
	}
	else
	  fmt::print("\t(skip Nummer {})\n", d.identifier);
	
      }
    }
    catch(std::exception& e) {
      fmt::print("Scanner {} failed: {}\n", scanner->describe(tp.getLease().get()),
		 e.what());
    }
  }
  for(auto& [user, content] : all) {
    map<set<Scanner*>, set<string>> grpd;
    set<Scanner*> allscanners;
    
    for(auto& [doc, lescanners] : content) {
      grpd[lescanners].insert(doc);
      for(auto& ls : lescanners)
	allscanners.insert(ls);
    }
    nlohmann::json data;
    for(auto& [grp, docs] : grpd) {
      nlohmann::json scannernames=nlohmann::json::array();
      for(auto& g : grp)
	scannernames.push_back(g->describe(tp.getLease().get()));

      nlohmann::json docdescs=nlohmann::json::array();
      for(auto& d : docs) {
	nlohmann::json ddesc;
	if(d.length() > 11) {
	  // 76423359-0db5-4503-8e41-b8440ab71faf
	  ddesc["dispnummer"] = d.substr(0, 8);
	}
	else ddesc["dispnummer"]=d;
	
	ddesc["nummer"]= d;
	ddesc["description"] = getDocDescription(tp.getLease().get(), d);
	docdescs.push_back(ddesc);
      }
      nlohmann::json stanza;
      stanza["scannernames"]=scannernames;
      stanza["hits"]=docdescs;
      data["payload"].push_back(stanza);
    }
    cout << data.dump() <<endl;
    inja::Environment e;
    string msg = e.render_file("./partials/email.txt", data);
    string subject;
    for(auto& sc : allscanners) {
      if(!subject.empty())
	subject+=", ";
      subject += sc->describe(tp.getLease().get());
    }
    subject = "[opentk alert] "+subject;

    inja::Environment e2;
    e2.set_html_autoescape(true);
    string html = e2.render_file("./partials/email.html", data);
    
    sendEmail("10.0.0.2",
			"opentk@hubertnet.nl",
	      getEmailForUserId(sconfig, user),
	      subject , msg, html);
  }
  for(auto& sc : scanners)
    updateScannerDate(sconfig, *sc);


  /*
  atomic<size_t> ctr = 0;
  auto worker = [&]() {
    for(size_t n = ctr++; n < sconfigs.size(); n = ctr++) {
      auto& sc = sconfigs[n];
      auto sqlw = tp.getLease();
      auto hits = g_scanners[sc.kind](sqlw.get(), sc.param1, sc.cutoff, sc.param2);
      fmt::print("Scanner {} param {} cutoff {} user {}\n",
		 sc.kind, sc.param1, sc.cutoff, sc.userid);
      for(const auto& h : hits) {
	if(emitIfNeeded(sqlw.get(), h, sc)) {
	  sendAsciiEmailAsync("10.0.0.2", "bert@hubertnet.nl", "bert@hubertnet.nl",
			      "opentk alert", fmt::format("Nieuw document {}\n", h.identifier));
	  fmt::print("\tNummer {}\n", h.identifier);
	}
	else
	  fmt::print("\t(skip Nummer {})\n", h.identifier);
      }
    }
  };
  
  
  vector<thread> workers;
  for(int n=0; n < 16; ++n)  // number of threads
    workers.emplace_back(worker);
  
  for(auto& w : workers)
    w.join();
  cout << (unsigned int) tp.d_maxout <<endl;
*/
}
