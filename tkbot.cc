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
#include "argparse/argparse.hpp"

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

void logEmission(SQLiteWriter& sqlw, const ScannerHit&sh, const Scanner& sc, const std::string& emissionId)
{
  string when = getNowDBFormat();
  sqlw.addValue({{"identifier", sh.identifier}, {"userid", sc.d_userid}, {"soort", sc.d_soort}, {"timestamp", when}, {"scannerId", sc.d_id}, {"emissionId", emissionId}}, "sentNotification");

}

void updateScannerDate(SQLiteWriter& sqlw, const Scanner& sc)
{
  string cutoff = getTodayDBFormat();
  sqlw.queryT("update scanners set cutoff=? where id=?", {cutoff, sc.d_id});
}

string getDescription(SQLiteWriter& sqlw, const std::string& nummer, const std::string& category)
{
  if(category=="Document") {
    auto res = sqlw.queryT("select onderwerp,titel from Document where nummer=?",
			   {nummer});
    if(!res.empty())
      return eget(res[0], "onderwerp");
    else return "";
  }
  else if(category=="Verslag") {
    auto res = sqlw.queryT("select titel from Vergadering where id=?",
			 {nummer});
    if(res.empty()) {
      cout<<"Could not find Verslag "<<nummer<<endl;
      return "";
    }
    return eget(res[0], "titel");
  }
  else if(category=="Activiteit") {
    auto res = sqlw.queryT("select soort||' '||onderwerp as onderwerp,datum from Activiteit where nummer=?",
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
  else if(category=="PersoonGeschenk") {
    auto res = sqlw.queryT("select omschrijving,roepnaam,tussenvoegsel,achternaam from PersoonGeschenk,Persoon where Persoon.id = PersoonID and persoongeschenk.id =?", {nummer});
    if(res.empty())
      return "";
    return "Geschenk aan " + eget(res[0], "roepnaam") + " " + eget(res[0], "tussenvoegsel") + " " +eget(res[0], "achternaam")+": "+eget(res[0], "omschrijving");
  }
  else if(category=="Toezegging") {
    auto res = sqlw.queryT("select * from toezegging where nummer=?", {nummer});
    if(res.empty())
      return "";
    return "Toezegging van "+eget(res[0], "naamToezegger") + " ("+eget(res[0], "ministerie")+"): "+eget(res[0], "tekst");
  }
  else throw(runtime_error("Unknown category "+category+" in getDescription"));
  return "";
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
  argparse::ArgumentParser args("tkbot", "0.0");
  args.add_argument("--smtp-server").help("IP address of SMTP smart host. If empty, no mail will get sent").default_value("");
  args.add_argument("--sender-email").help("From address of email we send").default_value("");
    
  try {
    args.parse_args(argc, argv);
  }
  catch (const std::runtime_error& err) {
    std::cout << err.what() << std::endl << args;
    std::exit(1);
  }
  string smtpserver = args.get<string>("--smtp-server");
  string fromaddr = args.get<string>("--sender-email");
  if(!smtpserver.empty() && fromaddr.empty()) {
    fmt::print("An smtp server has been defined, but no sender email address (--sender-email)\n");
    std::exit(1);
  }
  
  SQLiteWriter userdb("user.sqlite3");  

  auto toscan=userdb.queryT("select rowid,* from scanners");
  vector<unique_ptr<Scanner>> scanners;
  for(auto& ts: toscan) {
    if(auto iter = g_scanmakers.find(eget(ts,"soort")); iter != g_scanmakers.end()) {
      scanners.push_back(iter->second(userdb, get<string>(ts["id"])));
    }
  }
      
  ThingPool<SQLiteWriter> tp("tk.sqlite3", SQLWFlag::ReadOnly);
  //   user         id          scanners

  
  map<string, map<ScannerHit, set<Scanner*>>> all;

  atomic<size_t> ctr = 0;
  std::mutex mlock; // for all & userdb

  map<std::string, std::string> emissionId;
  
  auto worker = [&ctr, &all, &scanners, &userdb,
		 &emissionId, &mlock]() {
    unique_ptr<SQLiteWriter> own;
    {
      std::lock_guard<std::mutex> l(mlock); // sqlite gets unhappy if you all try to open the same db at the same time
      own = make_unique<SQLiteWriter>("tkindex-small.sqlite3", SQLWFlag::ReadOnly);
      own->query("ATTACH DATABASE 'tk.sqlite3' as meta");
    }
    
    for(size_t n = ctr++; n < scanners.size(); n = ctr++) {
      auto& scanner = scanners[n];
    
      fmt::print("{}\n", scanner->describe(*own));
      try {
	auto ds = scanner->get(*own); // this does the actual work

	for(const auto& d: ds) {
	  // for userdb and all and emissionid
	  std::lock_guard<std::mutex> l(mlock);   
  
	  if(emitIfNeeded(userdb, d, *scanner.get())) {
	    fmt::print("\tNummer {} {}\n", d.identifier, d.kind);
	    
	    all[scanner->d_userid][d].insert(scanner.get());

	    if(!emissionId.count(scanner->d_userid))
	      emissionId[scanner->d_userid] = getLargeId();
	    logEmission(userdb, d, *scanner.get(), emissionId[scanner->d_userid]);
	  }
	  else
	    fmt::print("\t(skip Nummer {} {})\n", d.identifier, d.kind);
	}
      }
      catch(std::exception& e) {
	fmt::print("Scanner {} failed: {}\n", scanner->describe(*own),
		   e.what());
      }
    }
  };

  vector<thread> workers;
  for(int n=0; n < 4; ++n) {  // number of threads
    workers.emplace_back(worker);
  }
  // go BRRRRR!
  for(auto& w : workers)
    w.join();
  // wait for everyone to be done - all is now filled
  
  for(auto& [user, content] : all) {
    map<set<Scanner*>, set<ScannerHit>> grpd;
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
	if(d.identifier.length() > 11) {
	  // 76423359-0db5-4503-8e41-b8440ab71faf
	  ddesc["dispnummer"] = d.identifier.substr(0, 8);
	}
	else ddesc["dispnummer"]=d.identifier;
	
	ddesc["nummer"]= d.identifier;
	ddesc["category"] = d.kind;
	ddesc["relurl"] = d.relurl;
	ddesc["description"] = getDescription(tp.getLease().get(), d.identifier,
					      d.kind);
	docdescs.push_back(ddesc);
      }
      nlohmann::json stanza;
      stanza["scannernames"]=scannernames;
      stanza["hits"]=docdescs;
      data["payload"].push_back(stanza);
    }
    cout << getEmailForUserId(userdb, user)<<": "<<data.dump() <<endl;
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
    userdb.addValue({{"user", user},
		    {"email", getEmailForUserId(userdb, user)},
		    {"id", emissionId[user]},
		    {"timestamp", getNowDBFormat()},
		    {"subject", subject}
      }, "emissions");

    if(!smtpserver.empty()) {
      //cout<<"Would send email through "<<smtpserver<<endl;
      sendEmail(smtpserver,
		fromaddr,
		getEmailForUserId(userdb, user),
		subject , msg, html);
    }
    else {
      cout<<"Not sending out email, no smtp-server configured\n";
      cout<<"Would have sent: "<<msg<<endl;
    }
  }
  for(auto& sc : scanners)
    updateScannerDate(userdb, *sc);
}
