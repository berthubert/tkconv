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
    "Activiteit", "Agendapunt", "Besluit", "Commissie",
    "Document", "DocumentActor", "Kamerstukdossier", "Persoon", "PersoonGeschenk",
    "Stemming", "Toezegging", "Vergadering", "Verslag", "Zaak", "ZaakActor"};

  if(argc > 1) {
    categories.clear();
    for(int n = 1 ; n < argc; ++n)
      categories.push_back(argv[n]);
  }
  SQLiteWriter sqlw("tk.sqlite3");
  SQLiteWriter xmlstore("xml.sqlite3");

  for(const auto& category: categories) {
    int skiptoken = -1;
    try {
      auto ret = sqlw.queryT("select skiptoken from "+category+" order by rowid desc limit 1");
      if(!ret.empty()) { 
	skiptoken = get<int64_t>(ret[0]["skiptoken"]);
      }
    }
    catch(std::exception& e) {
      fmt::print("Could not get a 'skiptoken' from database for category {}, starting from scratch\n", category);
    }
    int numentries=0;  

    auto entries = xmlstore.queryT("select * from "+category+" where skiptoken > ?", {skiptoken});

    for(auto& exml : entries) {
      pugi::xml_document pnode;
      if (!pnode.load_string( (get<string>(exml["xml"])).c_str())) {
	cout<<"Could not load"<<endl;
	return -1;
      }
      pugi::xml_node node = pnode.child("entry");
      string id = node.child("title").child_value();
      string updated = node.child("updated").child_value();
      string enclosure;
      numentries++;
      for (auto link : node.children("link")) {
	if(link.attribute("rel").value() == string("enclosure")) {
	  enclosure = link.attribute("href").value();
	}
	else if(link.attribute("rel").value() == string("next")) {
	  if(auto href = link.attribute("href")) {
	    string next = href.value();
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

      string bijgewerkt=node.child("content").begin()->attribute("tk:bijgewerkt").value();
	
      if(auto child = node.child("content").child("activiteit")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
	  continue;
	}
	  
	string datum = child.child("datum").child_value();
	string onderwerp = child.child("onderwerp").child_value();
	string soort = child.child("soort").child_value();
	string vrsNummer = child.child("vrsNummer").child_value();
	string voortouwnaam = child.child("voortouwnaam").child_value();
	string nummer = child.child("nummer").child_value();
	cout<<datum<<" / "<<updated<< " -> "<<onderwerp<<" ("<<nummer<<")\n";
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"nummer", nummer}, {"soort", soort}, {"onderwerp", onderwerp}, {"datum", datum}, {"vrsNummer", vrsNummer}, {"voortouwNaam", voortouwnaam} , {"updated", updated}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt}}, category);
	  
      }
      else if(auto child = node.child("content").child("document")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}}, category);
	  continue;
	}
	string datum = child.child("datum").child_value();
	cout<<datum<<" / "<<updated<<": (";
	string soort = child.child("soort").child_value();
	cout<<soort<<") ";
	string onderwerp = child.child("onderwerp").child_value();
	cout<< onderwerp;
	string titel = child.child("titel").child_value();
	cout<< " " <<titel;

	string contentType = child.attribute("tk:contentType").value();
	int64_t contentLength = atoi(child.attribute("tk:contentLength").value());;
	string vergaderjaar = child.child("vergaderjaar").child_value();
	string aanhangselnummer = child.child("aanhangselnummer").child_value();

	  
	string documentNummer = child.child("documentNummer").child_value();
	cout<<" ("<<documentNummer<<") "<<enclosure<<"\n";

	string citeerTitel = child.child("citeerTitel").child_value();
	string zaakId = child.child("zaak").attribute("ref").value();
	string huidigeDocumentVersieId = child.child("huidigeDocumentVersie").attribute("ref").value();
	string bronDocument = child.child("bronDocument").attribute("ref").value();
	string activiteitId = child.child("activiteit").attribute("ref").value();
	string kamerstukdossierId = child.child("kamerstukdossier").attribute("ref").value();
	int64_t volgnummer = atoi(child.child("volgnummer").child_value());;

	sqlw.addValue({{"id", id},  {"skiptoken", skiptoken}, {"nummer", documentNummer}, {"zaakId", zaakId}, {"activiteitId", activiteitId}, {"soort", soort}, {"onderwerp", onderwerp}, {"datum", datum}, {"enclosure", enclosure}, {"bronDocument", bronDocument}, {"updated", updated}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt}, {"kamerstukdossierId", kamerstukdossierId}, {"volgnummer", volgnummer}, {"titel", titel}, {"citeerTitel", citeerTitel}, {"contentLength", contentLength}, {"contentType", contentType}, {"huidigeDocumentVersieId", huidigeDocumentVersieId}, {"vergaderjaar", vergaderjaar}, {"aanhangselnummer", aanhangselnummer}}, category);
      }
      else if(auto child = node.child("content").child("zaak")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
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
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"nummer", zaakNummer}, {"kamerstukdossierId", kamerstukdossierId}, {"titel", titel}, {"onderwerp", onderwerp}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},{"gestartOp", gestartOp}, {"updated", updated}}, category);
	  
      }
      else if(auto child = node.child("content").child("kamerstukdossier")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
	  continue;
	}
	  
	string titel = child.child("titel").child_value();
	int nummer = atoi(child.child("nummer").child_value());
	cout<< nummer<< " " <<titel << endl;
	  
	string afgesloten = child.child("afgesloten").child_value();
	int hoogsteVolgnummer = atoi(child.child("hoogsteVolgnummer").child_value());
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"nummer", nummer}, {"titel", titel}, {"afgesloten", afgesloten}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},{"hoogsteVolgnummer", hoogsteVolgnummer}, {"updated", updated}}, category);
      }
      else if(auto child = node.child("content").child("ns1:toezegging")) {
	if(child.attribute("ns1:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
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
	  
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"nummer", nummer}, {"tekst", tekst},
		       {"kamerbriefNakoming", kamerbriefNakoming}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
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
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
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
	  
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
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
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
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
	  
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"updated", updated}, {"soort", fields["ns1:soort"]},
		       {"status", fields["ns1:status"]},
		       {"enclosure", enclosure},
		       {"vergaderingId", vergaderingId}},
	  category);
      }
      else if(auto child = node.child("content").child("persoon")) { 
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
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
	  
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
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
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
	  continue;
	}
	  
	auto child2 = *node.child("content").begin();
	map<string,string> fields;
	for(const auto&c : child2) {
	  fields[c.name()]= c.child_value();
	}
	  
	/*
	  agendapunt='' stemmingsSoort='' besluitSoort='V.k.a. - voor kennisgeving aannemen (commissie)' besluitTekst='Voor kennisgeving aannemen ' opmerking='De vaste commissie voor Defensie heeft de staatssecretaris van Defensie gevraagd om een reactie. De commissie voor de Rijksuitgaven wacht het antwoord met belangstelling af.' status='Besluit' agendapuntZaakBesluitVolgorde='1' zaak='' 
	*/

	string agendapuntId = child2.child("agendapunt").attribute("ref").value();
	string zaakId = child2.child("zaak").attribute("ref").value();
	  
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
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
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
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
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
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
      else if(auto child = node.child("content").child("documentActor")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
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
	string documentId = child.child("document").attribute("ref").value();
	string persoonId = child.child("persoon").attribute("ref").value();
	string fractieId = child.child("fractie").attribute("ref").value();
	string commissieId = child.child("commissie").attribute("ref").value();
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"updated", updated}, {"naam", fields["actorNaam"]},
		       {"fractie", fields["actorFractie"]},
		       {"functie", fields["functie"]},
		       {"relatie", fields["relatie"]},
		       {"documentId", documentId},
		       {"commissieId", commissieId},
		       {"persoonId", persoonId},
		       {"fractieId", fractieId}},
	  category);

      }
      else if(auto child = node.child("content").child("zaakActor")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
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
	string zaakId = child.child("zaak").attribute("ref").value();
	string persoonId = child.child("persoon").attribute("ref").value();
	string fractieId = child.child("fractie").attribute("ref").value();
	string commissieId = child.child("commissie").attribute("ref").value();
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"updated", updated}, {"naam", fields["actorNaam"]},
		       {"fractie", fields["actorFractie"]},
		       {"functie", fields["functie"]},
		       {"relatie", fields["relatie"]},
		       {"afkorting", fields["actorAfkorting"]},
		       {"zaakId", zaakId},
		       {"persoonId", persoonId},
		       {"commissieId", commissieId},
		       {"fractieId", fractieId}},
	  category);

      }

      else if(auto child = node.child("content").child("agendapunt")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
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
	  
	sqlw.addValue({{"id", id}, {"skiptoken",skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
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
      else if(auto child = node.child("content").child("persoonGeschenk")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
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
	string persoonId = child.child("persoon").attribute("ref").value();

	sqlw.addValue({{"id", id}, {"skiptoken",skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"updated", updated}, {"omschrijving", fields["omschrijving"]},
		       {"datum", fields["datum"]},
		       {"gewicht", atoi(fields["gewicht"].c_str())},
		       {"persoonId", persoonId}
	  },
	  category);

      }
      else if(auto child = node.child("content").child("commissie")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
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

	// nummer='62750' soort='Dienst Commissieondersteuning Internationaal en Ruimtelijk' afkorting='AM' naamNl='Vaste commissie voor Asiel en Migratie' naamEn='Asylum and Migration' naamWebNl='Asiel en Migratie' naamWebEn='Asylum and Migration' inhoudsopgave='Vaste commissies' datumActief='2024-07-02' datumInactief='' 
	sqlw.addValue({{"id", id}, {"skiptoken",skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"updated", updated},
		       {"nummer", atoi(fields["nummer"].c_str())},
		       {"soort", fields["soort"]},
		       {"afkorting", fields["afkorting"]},
		       {"naam", fields["naamNl"]},
		       {"naamEn", fields["naamEn"]},
		       {"webNaam", fields["naamWebNl"]},
		       {"inhoudsopgave", fields["inhoudsopgave"]},
		       {"datumActief", fields["dactumActief"]},
		       {"datumInactief", fields["dactumInactief"]}
	  },
	  category);
      }
    }
    cout<<"Done - saw "<<numentries<<" new entries"<<endl;
  }
}
