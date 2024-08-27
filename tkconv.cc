#include <fmt/format.h>
#include <fmt/printf.h>
#include <iostream>
#include "httplib.h"
#include "sqlwriter.hh"
#include "pugixml.hpp"

using namespace std;
int main(int argc, char** argv)
{
  vector<string> categories={
    "Activiteit", "Agendapunt", "Besluit",
    "Document", "Kamerstukdossier", "Persoon",
    "Stemming", "Toezegging", "Vergadering", "Verslag", "Zaak"};

  if(argc > 1) {
    categories.clear();
    for(int n = 1 ; n < argc; ++n)
      categories.push_back(argv[n]);
  }
  SQLiteWriter sqlw("tk.sqlite3");
  ofstream xmls("xmls");
  for(const auto& category: categories) {
    string next="https://gegevensmagazijn.tweedekamer.nl/SyncFeed/2.0/Feed?category=" + category;
    try {
      auto ret = sqlw.query("select next from "+category+" order by rowid desc limit 1");
      if(!ret.empty()) { 
	next = ret[0]["next"];
	//	fmt::print("Got a 'next' url from the database, resuming at {}\n", next);
      }
    }
    catch(std::exception& e) {
      fmt::print("Could not get a 'next' from database for category {}, starting from scratch\n", category);
    }
    int entries=0;  

    while(!next.empty()) {
      httplib::Client cli("https://gegevensmagazijn.tweedekamer.nl");
      cli.set_connection_timeout(10, 0); 
      cli.set_read_timeout(10, 0); 
      cli.set_write_timeout(10, 0); 
      
      fmt::print("Retrieving from {}\n", next);
      auto res = cli.Get(next);

      if(!res) {
	auto err = res.error();
	throw runtime_error("Oops retrieving from "+next+" -> "+httplib::to_string(err));
      }

      
      next.clear();
      
      string xml = res->body;
      
      xmls<<xml<<endl;
      pugi::xml_document doc;
      if (!doc.load_string(xml.c_str())) {
	cout<<"Could not load"<<endl;
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
	for (auto link : node.children("link")) {
	  if(link.attribute("rel").value() == string("enclosure")) {
	    enclosure = link.attribute("href").value();
	  }
	  else if(link.attribute("rel").value() == string("next")) {
	    if(auto href = link.attribute("href")) {
	      next = href.value();
	    }
	  }
	}
	if(auto child = node.child("content").child("activiteit")) {
	  if(child.attribute("tk:verwijderd").value() == string("true")) {
	    sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", true}, {"updated", updated}}, category);
	    continue;
	  }
	  
	  string datum = child.child("datum").child_value();
	  string onderwerp = child.child("onderwerp").child_value();
	  string soort = child.child("soort").child_value();
	  string vrsNummer = child.child("vrsNummer").child_value();
	  string voortouwnaam = child.child("voortouwnaam").child_value();
	  string nummer = child.child("nummer").child_value();
	  cout<<datum<<" / "<<updated<< " -> "<<onderwerp<<" ("<<nummer<<")\n";
	  sqlw.addValue({{"id", id}, {"next", next}, {"activiteitNummer", nummer}, {"soort", soort}, {"onderwerp", onderwerp}, {"datum", datum}, {"vrsNummer", vrsNummer}, {"voortouwNaam", voortouwnaam} , {"updated", updated}, {"verwijderd", false}}, category);
	  
	}
	else if(auto child = node.child("content").child("document")) {
	  if(child.attribute("tk:verwijderd").value() == string("true")) {
	    sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", true}}, category);
	    continue;
	  }
	  string datum = child.child("datum").child_value();
	  cout<<datum<<" / "<<updated<<": (";
	  string soort = child.child("soort").child_value();
	  cout<<soort<<") ";
	  string onderwerp = child.child("onderwerp").child_value();
	  cout<< onderwerp;
	  
	  string documentNummer = child.child("documentNummer").child_value();
	  cout<<" ("<<documentNummer<<") "<<enclosure<<"\n";
	  string zaakId = child.child("zaak").attribute("ref").value();
	  string activiteitId = child.child("activiteit").attribute("ref").value();
	  sqlw.addValue({{"id", id},  {"next", next}, {"nummer", documentNummer}, {"zaakId", zaakId}, {"activiteitId", activiteitId}, {"soort", soort}, {"onderwerp", onderwerp}, {"datum", datum}, {"enclosure", enclosure}, {"updated", updated}, {"verwijderd", false}}, category);
	}
	else if(auto child = node.child("content").child("zaak")) {
	  if(child.attribute("tk:verwijderd").value() == string("true")) {
	    sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", true}, {"updated", updated}}, category);
	    continue;
	  }
	  
	  string gestartOp = child.child("gestartOp").child_value();
	  cout<<gestartOp<<": ";
	  string onderwerp = child.child("onderwerp").child_value();
	  string titel = child.child("titel").child_value();
	  string zaakNummer = child.child("nummer").child_value();
	  cout<< onderwerp <<" ("<< zaakNummer<<")"<< " "<<titel<<endl;
	  
	  
	  string kamerstukdossierId = child.child("kamerstukdossier").attribute("ref").value();
	  // also get agendapunt refs!
	  sqlw.addValue({{"id", id}, {"next", next}, {"nummer", zaakNummer}, {"kamerstukdossierId", kamerstukdossierId}, {"titel", titel}, {"onderwerp", onderwerp}, {"verwijderd", false},{"gestartOp", gestartOp}, {"updated", updated}}, category);
	  
	}
	else if(auto child = node.child("content").child("kamerstukdossier")) {
	  if(child.attribute("tk:verwijderd").value() == string("true")) {
	    sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", true}, {"updated", updated}}, category);
	    continue;
	  }
	  
	  string titel = child.child("titel").child_value();
	  int nummer = atoi(child.child("nummer").child_value());
	  cout<< nummer<< " " <<titel << endl;
	  
	  string afgesloten = child.child("afgesloten").child_value();
	  int hoogsteVolgnummer = atoi(child.child("hoogsteVolgnummer").child_value());
	  sqlw.addValue({{"id", id}, {"next", next}, {"nummer", nummer}, {"titel", titel}, {"afgesloten", afgesloten}, {"verwijderd", false},{"hoogsteVolgnummer", hoogsteVolgnummer}, {"updated", updated}}, category);
	}
	else if(auto child = node.child("content").child("ns1:toezegging")) {
	  if(child.attribute("ns1:verwijderd").value() == string("true")) {
	    sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", true}, {"updated", updated}}, category);
	    continue;
	  }
	  
	  string tekst = child.child("ns1:tekst").child_value();
	  string naamToezegger = child.child("ns1:naam").child_value();
	  string nummer = child.child("ns1:nummer").child_value();
	  string kamerbriefNakoming = child.child("ns1:kamerbriefNakoming").child_value();
	  string ministerie = child.child("ns1:ministerie").child_value();
	  string status = child.child("ns1:status").child_value();
	  string datum = child.child("ns1:aanmaakdatum").child_value();
	  string datumNakoming = child.child("ns1:datumNakoming").child_value();
	  string activiteitId = child.child("ns1:activiteit").attribute("ref").value();
	  string fractieId = child.child("ns1:toegezegdAanFractie").attribute("ref").value();
	  cout<< nummer<< " " <<tekst << endl;
	  
	  sqlw.addValue({{"id", id}, {"next", next}, {"nummer", nummer}, {"tekst", tekst},
			 {"kamerbriefNakoming", kamerbriefNakoming}, {"verwijderd", false},
			 {"datum", datum}, {"ministerie", ministerie},
			 {"status", status},
			 {"datumNakoming", datumNakoming},
			 {"activiteitId", activiteitId},
			 {"fractieId", fractieId},
			 {"naamToezegger", naamToezegger},
			 {"updated", updated}}, category);
	}
	else if(auto child = node.child("content").child("ns1:vergadering")) {
	  if(child.attribute("ns1:verwijderd").value() == string("true")) {
	    sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", true}, {"updated", updated}}, category);
	    continue;
	  }
	  
	  auto child2 = *node.child("content").begin();
	  cout << child2.name() << endl;
	  map<string,string> fields;
	  for(const auto&c : child2) {
	    cout << c.name() <<"='"<<c.child_value()<<"' ";
	    fields[c.name()]= c.child_value();
	  }
	  cout<<endl;
	  
	  sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", false},
			 {"updated", updated}, {"soort", fields["ns1:soort"]},
			 {"titel", fields["ns1:titel"]},
			 {"zaal", fields["ns1:zaal"]},
			 {"vergaderjaar", fields["ns1:vergaderjaar"]},
			 {"nummer", atoi(fields["ns1:vergaderingNummer"].c_str())},
			 {"datum", fields["ns1:datum"]},
			 {"aanvangstijd", fields["ns1:aanvangstijd"]},
			 {"sluiting", fields["ns1:sluiting"]}}, category);
	  
	}
	else if(auto child = node.child("content").child("ns1:verslag")) { 
	  if(child.attribute("ns1:verwijderd").value() == string("true")) {
	    sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", true}, {"updated", updated}}, category);
	    continue;
	  }
	  
	  auto child2 = *node.child("content").begin();
	  cout << child2.name() << endl;
	  map<string,string> fields;
	  for(const auto&c : child2) {
	    cout << c.name() <<"='"<<c.child_value()<<"' ";
	    fields[c.name()]= c.child_value();
	  }
	  cout<<endl;
	  cout<<"enclosure "<<enclosure<<endl;
	  string vergaderingId = child2.child("ns1:vergadering").attribute("ref").value();
	  cout<<"vergaderingId "<<vergaderingId<<endl;
	  
	  sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", false},
			 {"updated", updated}, {"soort", fields["ns1:soort"]},
			 {"status", fields["ns1:status"]},
			 {"enclosure", enclosure},
			 {"vergaderingId", vergaderingId}},
	    category);
	}
	else if(auto child = node.child("content").child("persoon")) { 
	  if(child.attribute("tk:verwijderd").value() == string("true")) {
	    sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", true}, {"updated", updated}}, category);
	    continue;
	  }
	  
	  auto child2 = *node.child("content").begin();
	  cout << child2.name() << endl;
	  map<string,string> fields;
	  for(const auto&c : child2) {
	    cout << c.name() <<"='"<<c.child_value()<<"' ";
	    fields[c.name()]= c.child_value();
	  }
	  cout<<endl;
	  
	  sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", false},
			 {"updated", updated}, {"functie", fields["functie"]},
			 {"intialen", fields["initialen"]},
			 {"tussenvoegsel", fields["tussenvoegsel"]},
			 {"achternaam", fields["achternaam"]},
			 {"voornamen", fields["voornamen"]},
			 {"roepnaam", fields["roepnaam"]},
			 {"geboortedatum", fields["geboortedatum"]},
			 {"geboorteplaats", fields["geboorteplaats"]},
			 {"geboorteland", fields["geboorteland"]},
			 {"overlijdensdatum", fields["overlijdensdatum"]},
			 {"overlijdensplaats", fields["overlijdensplaats"]},
			 
			 {"geslacht", fields["geslacht"]},
			 {"titels", fields["titels"]},
			 {"enclosure", enclosure},
			 {"woonplaats", fields["woonplaats"]},
			 {"land", fields["land"]},
			 {"nummer", atoi(fields["nummer"].c_str())}},
	    
	    category);
	}
	else if(auto child = node.child("content").child("besluit")) { 
	  if(child.attribute("tk:verwijderd").value() == string("true")) {
	    sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", true}, {"updated", updated}}, category);
	    continue;
	  }
	  
	  auto child2 = *node.child("content").begin();
	  cout << child2.name() << endl;
	  map<string,string> fields;
	  for(const auto&c : child2) {
	    cout << c.name() <<"='"<<c.child_value()<<"' ";
	    fields[c.name()]= c.child_value();
	  }
	  cout<<endl;
	  
	  /*
	    agendapunt='' stemmingsSoort='' besluitSoort='V.k.a. - voor kennisgeving aannemen (commissie)' besluitTekst='Voor kennisgeving aannemen ' opmerking='De vaste commissie voor Defensie heeft de staatssecretaris van Defensie gevraagd om een reactie. De commissie voor de Rijksuitgaven wacht het antwoord met belangstelling af.' status='Besluit' agendapuntZaakBesluitVolgorde='1' zaak='' 
	  */

	  string agendapuntId = child2.child("agendapunt").attribute("ref").value();
	  string zaakId = child2.child("zaak").attribute("ref").value();
	  
	  sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", false},
			 {"updated", updated}, {"soort", fields["besluitSoort"]},
			 {"tekst", fields["besluitTekst"]},
			 {"opmerking", fields["opmerking"]},
			 {"status", fields["status"]},
			 {"zaakId", zaakId},
			 {"agendapuntId", agendapuntId},
			 {"zaakId", zaakId}},
	    category);
	  
	  
	}
	else if(auto child = node.child("content").child("stemming")) {
	  if(child.attribute("tk:verwijderd").value() == string("true")) {
	    sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", true}, {"updated", updated}}, category);
	    continue;
	  }
	  
	  auto child2 = *node.child("content").begin();
	  cout << child2.name() << endl;
	  map<string,string> fields;
	  for(const auto&c : child2) {
	    cout << c.name() <<"='"<<c.child_value()<<"' ";
	    fields[c.name()]= c.child_value();
	  }
	  cout<<endl;
	  
	  string besluitId = child2.child("besluit").attribute("ref").value();
	  string persoonId = child2.child("persoon").attribute("ref").value();
	  string fractieId = child2.child("fractie").attribute("ref").value();
	  sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", false},
			 {"updated", updated}, {"soort", fields["soort"]},
			 {"actorNaam", fields["actorNaam"]},
			 {"actorFractie", fields["actorFractie"]},
			 {"fractieGrootte", atoi(fields["fractieGrootte"].c_str())},
			 {"besluitId", besluitId},
			 {"persoonId", persoonId},
			 {"fractieId", fractieId},
			 {"vergissing", fields["vergissing"]}
	    },
	    category);
	  
	  
	}
	else if(auto child = node.child("content").child("agendapunt")) {
	  if(child.attribute("tk:verwijderd").value() == string("true")) {
	    sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", true}, {"updated", updated}}, category);
	    continue;
	  }
	  
	  auto child2 = *node.child("content").begin();
	  cout << child2.name() << endl;
	  map<string,string> fields;
	  for(const auto&c : child2) {
	    cout << c.name() <<"='"<<c.child_value()<<"' ";
	    fields[c.name()]= c.child_value();
	  }
	  cout<<endl;
	  string activiteitId = child.child("activiteit").attribute("ref").value();
	  
	  // activiteit='' nummer='2008P01291' onderwerp='Beantwoording vragen commissie over het evaluatierapport Belastinguitgaven op het terrein van de accijnzen' aanvangstijd='' eindtijd='' volgorde='40' rubriek='Stukken/brieven (als eerste) ondertekend door de staatssecretaris van Financiën' noot='De antwoorden op de door de commissie gestelde vragen zijn ontvangen op 15 juli 2008 (31200-IXB, nr. 35)' status='Vrijgegeven' 
	  
	  sqlw.addValue({{"id", id}, {"next", next}, {"verwijderd", false},
			 {"updated", updated}, {"nummer", fields["nummer"]},
			 {"onderwerp", fields["onderwerp"]},
			 {"aanvangstijd", fields["aanvangstijd"]},
			 {"eindtijd", fields["eindtijd"]},
			 {"volgorde", atoi(fields["volgorde"].c_str())},
			 {"rubriek", fields["rubriek"]},
			 {"noot", fields["noot"]},
			 {"status", fields["status"]},
			 {"activiteitId", activiteitId}
	    },
	    category);
	}
      }
    }
    cout<<"Done - saw "<<entries<<" new entries"<<endl;
  }
}
