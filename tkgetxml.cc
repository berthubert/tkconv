#include <fmt/format.h>
#include <fmt/printf.h>
#include <iostream>
#include "httplib.h"
#include "sqlwriter.hh"
#include "pugixml.hpp"

using namespace std;
int main(int argc, char** argv)
{
  vector<string> categories=
    {"Activiteit", "ActiviteitActor", "Agendapunt", "Besluit", "Commissie",
"CommissieContactinformatie", "CommissieZetel", "CommissieZetelVastPersoon",
"CommissieZetelVastVacature", "CommissieZetelVervangerPersoon",
"CommissieZetelVervangerVacature", "Document", "DocumentActor",
"DocumentVersie", "Fractie", "FractieZetel", "FractieZetelPersoon",
"FractieZetelVacature", "Kamerstukdossier", "Persoon",
"PersoonContactinformatie", "PersoonGeschenk", "PersoonLoopbaan",
"PersoonNevenfunctie", "PersoonNevenfunctieInkomsten", "PersoonOnderwijs",
"PersoonReis", "Reservering", "Stemming", "Toezegging", "Vergadering",
"Verslag", "Zaak", "ZaakActor", "Zaal"};

  signal(SIGPIPE, SIG_IGN); // every TCP application needs this
  
  if(argc > 1) {
    categories.clear();
    for(int n = 1 ; n < argc; ++n)
      categories.push_back(argv[n]);
  }
  SQLiteWriter sqlw("xml.sqlite3");

  sqlw.query("create table if not exists meta (source TEXT, name TEXT, value INT) STRICT");
  sqlw.query("create unique index if not exists metaindex on meta(source, name)");

  
  for(const auto& category: categories) {
    sqlw.query("create table if not exists "+category+" (category TEXT, id TEXT, skiptoken INT, enclosure TEXT, updated TEXT, xml TEXT)");
    sqlw.query("create index if not exists "+category+"skipidx on "+category+"(skiptoken)");
    string next="https://gegevensmagazijn.tweedekamer.nl/SyncFeed/2.0/Feed?category=" + category;
    int skiptoken = -1;
    try {
      auto ret = sqlw.queryT("select skiptoken from "+category+" order by rowid desc limit 1");
      if(!ret.empty()) { 
	skiptoken = get<int64_t>(ret[0]["skiptoken"]);
        next = fmt::format("https://gegevensmagazijn.tweedekamer.nl/SyncFeed/2.0/Feed?skiptoken={}&category={}", skiptoken, category);
      }
      else {
	fmt::print("Could not get a skiptoken for category {}. First run?\n", category);
      }
    }
    catch(std::exception& e) {
      fmt::print("Could not get a 'next' from database for category {}, starting from scratch\n", category);
    }
    int catentries=0;  

    while(!next.empty()) {
      int entries = 0;
      httplib::Client cli("https://gegevensmagazijn.tweedekamer.nl");
      cli.set_connection_timeout(10, 0); 
      cli.set_read_timeout(10, 0); 
      cli.set_write_timeout(10, 0); 
      
      fmt::print("Retrieving from {}.. ", next);
      cout.flush();
      auto res = cli.Get(next);

      if(!res) {
	auto err = res.error();
	throw runtime_error("Oops retrieving from "+next+" -> "+httplib::to_string(err));
      }
      
      next.clear();
      pugi::xml_document doc;
      if (!doc.load_string(res->body.c_str())) {
	cout<<"Could not load XML"<<endl;
	return -1;
      }
      
      auto feed = doc.child("feed");
      if(!feed) {
	cout<<"No feed in XML at "<<next<<"\n";
	return -1;
      }
      
      for(const auto& node : feed.children("entry")) {
	string id = node.child("title").child_value();
	string updated = node.child("updated").child_value();
	string enclosure;
	entries++;
	catentries++;
	for (auto link : node.children("link")) {
	  if(link.attribute("rel").value() == string("enclosure")) {
	    enclosure = link.attribute("href").value();
	  }
	  else if(link.attribute("rel").value() == string("next")) {
	    if(auto href = link.attribute("href")) {
	      next = href.value();
	      // https://gegevensmagazijn.tweedekamer.nl/SyncFeed/2.0/Feed?skiptoken=20127222&category=Document
	      if(next.find("https://gegevensmagazijn.tweedekamer.nl/SyncFeed/2.0/Feed"))
		throw std::runtime_error("Unexpected next URL format "+next);
	      if(auto pos = next.find("skiptoken="); pos ==string::npos)
		throw std::runtime_error("Could not find skiptoken in "+next);
	      else {
		skiptoken = atoi(next.substr(pos+10).c_str());
	      }
	    }
	  }
	}
	ostringstream xml;
	node.print(xml, "\t", pugi::format_raw);
	sqlw.addValue({{"category", category},{"id", id}, {"skiptoken", skiptoken}, {"enclosure", enclosure}, {"updated", updated}, {"xml", xml.str()}}, category);
      }
      fmt::print("got {} entries\n", entries);
      usleep(100000);
    }
    cout<<"Done - saw "<<catentries<<" new entries for category "<< category<<endl;
  }
  
  sqlw.addOrReplaceValue({{"source", "tkgetxml"}, {"name", "lastupdate"}, {"value", time(0)}}, "meta");
}
