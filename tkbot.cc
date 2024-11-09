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

using namespace std;

struct ScannerHit
{
  string identifier;
  string date;
  string kind;
};

template<typename T>
string eget(const T& cont, const std::string& fname)
{
  string ret;
  auto iter = cont.find(fname);
  if(iter == cont.end() || !std::get_if<string>(&iter->second))
    return ret;

  return std::get<string>(iter->second);  
}

struct Scanner
{
  virtual string describe(SQLiteWriter& sqlw) = 0;
  virtual vector<ScannerHit> get(SQLiteWriter& sqlw) = 0;
  auto getRow(SQLiteWriter& sqlw, int id)
  {
    auto res = sqlw.queryT("select * from scanners where rowid=?", {id});
    if(res.empty())
      throw runtime_error("No such ID");
    d_cutoff = eget(res[0], "cutoff");
    d_id = id;
    d_userid = eget(res[0], "userid");
    d_soort = eget(res[0], "soort");
    return res[0];
  }
  auto sqlToScannerHits(std::vector<std::unordered_map<std::string,MiniSQLite::outvar_t>>& hits)
  {
    vector<ScannerHit> ret;
    ret.reserve(hits.size());
    for(const auto& h : hits) {
      ScannerHit sh;
      sh.identifier = eget(h, "nummer");
      sh.date = eget(h, "datum");
      sh.kind = "Document";
      ret.push_back(sh);
    }
    return ret;
  }
  string d_cutoff, d_userid, d_soort;
  unsigned int d_id;
};


struct CommissieScanner : Scanner
{
  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, int id) 
  {  
    CommissieScanner s;
    auto row = s.getRow(sqlw, id);  
    s.d_commissieid = eget(row, "commissieId");
    return std::make_unique<CommissieScanner>(s);
  }
  
  vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    auto hits = sqlw.queryT("select datum,document.nummer,relatie from ZaakActor,Zaak,link,document where commissieId=? and ZaakId=zaak.id and naar=zaak.id and document.id=van and datum >= ? and category='Document' and relatie='Voortouwcommissie' order by datum", {d_commissieid, d_cutoff});
    return sqlToScannerHits(hits);
  }
  
  string describe(SQLiteWriter& sqlw) override
  {
    auto orow = sqlw.queryT("select naam from commissie where id=?", {d_commissieid});
    string naam;
    if(!orow.empty()) {
      naam = eget(orow[0], "naam");
    }
    return naam;
  }
  string d_commissieid;
};



 
struct PersoonScanner : Scanner
{
  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, int id) 
  {  
    PersoonScanner zs;
    auto row = zs.getRow(sqlw, id);  
    zs.d_nummer = eget(row, "nummer");
    return std::make_unique<PersoonScanner>(zs);
  }
  
  vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    auto hits = sqlw.queryT("select relatie,document.nummer, document.datum from Document,DocumentActor,Persoon where persoon.nummer=? and documentactor.documentid=document.id and persoon.id=persoonid and datum >= ?", {d_nummer, d_cutoff});

    auto vhits = sqlw.queryT("select vergaderingid as nummer, datum from Vergaderingspreker,vergadering,persoon where vergadering.id=vergaderingid and persoon.nummer=? and persoon.id = persoonid and datum >= ?", {d_nummer, d_cutoff});

    for(const auto& vh: vhits)
      hits.push_back(vh);
    
    return sqlToScannerHits(hits);
  }
  
  string describe(SQLiteWriter& sqlw) override
  {
    auto orow = sqlw.queryT("select roepnaam,tussenvoegsel,achternaam from Persoon where nummer=?", {d_nummer});
    string naam;
    if(!orow.empty()) {
      naam = eget(orow[0], "roepnaam");
      string tv =eget(orow[0], "tussenvoegsel");
      if(!tv.empty()) {
	naam += " " +tv;
      }
      naam += " " +eget(orow[0], "achternaam");
    }
    return "Persoon " + naam;
  }
  string d_nummer;
};


struct ActiviteitScanner : Scanner
{
  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, int id) 
  {  
    ActiviteitScanner s;
    auto row = s.getRow(sqlw, id);  
    s.d_nummer = eget(row, "nummer");
    return std::make_unique<ActiviteitScanner>(s);
  }
  
  vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    auto hits = sqlw.queryT("select Document.nummer, document.datum from link,Document,activiteit where linkSoort='Activiteit' and link.naar=activiteit.id and activiteit.nummer=? and Document.id=link.van and document.datum >= ?", {d_nummer, d_cutoff});
    return sqlToScannerHits(hits);
  }
  
  string describe(SQLiteWriter& sqlw) override
  {
    auto orow = sqlw.queryT("select onderwerp from Activiteit where nummer=?", {d_nummer});
    string onderwerp;
    if(!orow.empty()) {
      onderwerp = eget(orow[0], "onderwerp");
    }
    return "Activiteit " + d_nummer+": "+ onderwerp;
  }
  string d_nummer;
};

struct ZaakScanner : Scanner
{
  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, int id) 
  {  
    ZaakScanner zs;
    auto row = zs.getRow(sqlw, id);  
    zs.d_nummer = eget(row, "nummer");
    return std::make_unique<ZaakScanner>(zs);
  }
  
  vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    auto hits = sqlw.queryT("select document.nummer, document.datum from Zaak,Link,Document where zaak.nummer=? and zaak.id=link.naar and document.id = link.van and datum >= ?", {d_nummer, d_cutoff});
    return sqlToScannerHits(hits);
  }
  
  string describe(SQLiteWriter& sqlw) override
  {
    auto orow = sqlw.queryT("select onderwerp from Zaak where nummer=?", {d_nummer});
    string onderwerp;
    if(!orow.empty()) {
      onderwerp = eget(orow[0], "onderwerp");
    }
    return "Zaak " + d_nummer+": "+ onderwerp;
  }
  string d_nummer;
};


struct KsdScanner : Scanner
{
  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, int id) 
  {  
    KsdScanner s;
    auto row = s.getRow(sqlw, id);  
    s.d_nummer = eget(row, "nummer");
    s.d_toevoeging = eget(row, "toevoeging");
    return std::make_unique<KsdScanner>(s);
  }
  
  vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    auto hits = sqlw.queryT("select document.nummer,datum from document,kamerstukdossier where kamerstukdossierid=kamerstukdossier.id and kamerstukdossier.nummer=? and toevoeging=? and datum >= ?", {d_nummer, d_toevoeging, d_cutoff});
    return sqlToScannerHits(hits);
  }
  
  string describe(SQLiteWriter& sqlw) override
  {
    auto orow = sqlw.queryT("select titel from kamerstukdossier where nummer=? and toevoeging=?", {d_nummer, d_toevoeging});
    string onderwerp;
    if(!orow.empty()) {
      onderwerp = eget(orow[0], "titel");
    }
    return "Kamerstukdossier " + d_nummer+ " " +d_toevoeging+": "+ onderwerp;
  }
  string d_nummer, d_toevoeging;
};



struct ZoekScanner : Scanner
{
  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, int id) 
  {  
    ZoekScanner s;
    auto row = s.getRow(sqlw, id);  
    s.d_query = eget(row, "query");
    s.d_categorie = eget(row, "categorie");
    return std::make_unique<ZoekScanner>(s);
  }
  
  vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    struct Local
    {
      Local() : idx("tkindex.sqlite3", SQLWFlag::ReadOnly)
      {
	idx.query("ATTACH DATABASE 'tk.sqlite3' as meta");
      }
      SQLiteWriter idx;
      
    };
    static Local l;

    auto matches = l.idx.queryT("SELECT uuid, snippet(docsearch,-1, '<b>', '</b>', '...', 20) as snip,  category FROM docsearch WHERE docsearch match ? and (datum > ? or datum='') and (? or category=?)", {d_query, d_cutoff, d_categorie.empty(), d_categorie});

    cout<<"d_categorie: '"<<d_categorie<<"'\n";
    vector<ScannerHit> ret;
    for(auto& m : matches) {
      auto doc =  l.idx.queryT("select onderwerp, bijgewerkt, titel, nummer, datum FROM meta.Document where id=?",   {eget(m, "uuid")});
      if(doc.empty())
	continue;
      
      for(const auto& f : doc[0]) {
	m[f.first]=f.second;
      }
      ret.emplace_back(eget(m, "nummer"),
		       eget(m, "datum"),
		       "Document");

    }

    for(auto& m : matches) {
      if(m.count("nummer"))
	continue;
      if(eget(m,"category") != "Activiteit")
	continue;
      auto act =  l.idx.queryT("select nummer, datum FROM meta.Activiteit where id=?",   {eget(m, "uuid")});
      if(act.empty())
	continue;
      ret.emplace_back(eget(act[0], "nummer"),
		       eget(act[0], "datum"),
		       "Activiteit");
    }
    
    return ret;
  }
  
  string describe(SQLiteWriter& sqlw) override
  {
    string ret = "Zoekopdracht " + d_query;
    if(!d_categorie.empty())
      ret += " (categorie "+d_categorie+")";
    return ret;
  }
  string d_query;
  string d_categorie;
};



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
  sqlw.addValue({{"identifier", sh.identifier}, {"userid", sc.d_userid}, {"soort", sc.d_soort}, {"timestamp", when}, {"scannnerId", sc.d_id}}, "sentNotification");

}

void updateScannerDate(SQLiteWriter& sqlw, const Scanner& sc)
{
  string cutoff = fmt::format("{:%Y-%m-%d}", fmt::localtime(time(0)));
  sqlw.queryT("update scanners set cutoff=? where rowid=?", {cutoff, sc.d_id});
}

map<string, decltype(&ZaakScanner::make)> g_makers =
    {
      {"activiteit", ActiviteitScanner::make},
      {"zaak", ZaakScanner::make},
      {"ksd", KsdScanner::make},
      {"zoek", ZoekScanner::make},
      {"commissie", CommissieScanner::make},
      {"persoon", PersoonScanner::make},
    };


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
  auto res = sqlw.queryT("select email from users where id=?", {userid});
  if(res.empty())
    throw runtime_error("No email for userid '"+userid+"'");
  return eget(res[0], "email");
}

int main(int argc, char** argv)
{
  SQLiteWriter sconfig("user.sqlite3", {{"users", {{"id", "PRIMARY KEY"}}}});
  
  if(argc==2 && argv[1]==string("new")) {
    string userid=getLargeId();
    sconfig.addValue({{"id", userid}, {"email", "bert@hubertnet.nl"}}, "users");

    sconfig.addValue({{"userid", userid}, {"cutoff", "2024-11-01"}, {"nummer", "2024Z16445"}, {"soort", "zaak"}}, "scanners");

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-11-01"}, {"commissieId", "807e03a0-df3f-49a1-9618-996f6fb7e24f"}, {"soort", "commissie"}}, "scanners");
    
    sconfig.addValue({{"userid", userid},{"cutoff", "2024-11-01"}, {"commissieId", "177eb895-6307-4268-94f6-a0f80dc3e358"}, {"soort", "commissie"}}, "scanners");

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-11-01"}, {"nummer", "4921"}, {"soort", "persoon"}}, "scanners");

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-11-01"}, {"nummer", "48800"}, {"soort", "persoon"}}, "scanners");

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-11-01"}, {"nummer", "58572"}, {"soort", "persoon"}}, "scanners");

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-11-01"}, {"nummer", "36650"}, {"soort", "ksd"}, {"toevoeging", ""}}, "scanners");

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-11-01"}, {"query", "AIVD OR MIVD"}, {"soort", "zoek"}}, "scanners");

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-11-01"}, {"query", "hubert NOT bruls"}, {"soort", "zoek"}}, "scanners");

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-11-01"}, {"query", "Nootdorp"}, {"soort", "zoek"}}, "scanners");

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-09-01"}, {"query", "CSAM OR CSAR"}, {"categorie", "Activiteit"}, {"soort", "zoek"}}, "scanners");    

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-11-01"}, {"query", "autonomie OR soevereiniteit OR cloud"}, {"categorie", "Activiteit"}, {"soort", "zoek"}}, "scanners");    

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-10-01"}, {"nummer", "2024A07225"}, {"soort", "activiteit"}}, "scanners");    

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-10-01"}, {"nummer", "2024A06698"}, {"soort", "activiteit"}}, "scanners");    

    sconfig.addValue({{"userid", userid},{"cutoff", "2024-10-01"}, {"nummer", "2024A05208"}, {"soort", "activiteit"}}, "scanners");    

    
  }

  auto toscan=sconfig.queryT("select rowid,* from scanners");
  vector<unique_ptr<Scanner>> scanners;
  for(auto& ts: toscan) {
    if(auto iter = g_makers.find(eget(ts,"soort")); iter != g_makers.end()) {
      scanners.push_back(iter->second(sconfig, get<int64_t>(ts["rowid"])));
    }
  }
      

  ThingPool<SQLiteWriter> tp("tk.sqlite3");
  //   user       doc         scanner
  map<string, map<string, set<Scanner*>>> all;
  for(auto& scanner : scanners) {
    fmt::print("{}\n", scanner->describe(tp.getLease().get()));
    auto ds = scanner->get(tp.getLease().get());
    for(const auto& d: ds) {
      if(emitIfNeeded(sconfig, d, *scanner.get())) {
	fmt::print("\tNummer {}\n", d.identifier);
	all[scanner->d_userid][d.identifier].insert(scanner.get());
      }
      else
	fmt::print("\t(skip Nummer {})\n", d.identifier);
      logEmission(sconfig, d, *scanner.get());
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
			"bert@hubertnet.nl",
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
