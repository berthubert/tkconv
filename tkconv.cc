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
    "Document", "DocumentActor", "DocumentVersie", "Kamerstukdossier", "Persoon", "PersoonGeschenk",
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

	// relaties inkomend, ActiviteitActor, AgendaPunt
	// twee-weg: Zichzelf (VoortgezetVanuit, VoortgezetIn, VervangenVanuit, VervangenDoor)
	
	string datum = child.child("datum").child_value();
	string onderwerp = child.child("onderwerp").child_value();
	string noot = child.child("noot").child_value();
	string soort = child.child("soort").child_value();
	string aanvangstijd = child.child("aanvangstijd").child_value();
	string eindtijd = child.child("eindtijd").child_value();
	string vrsNummer = child.child("vrsNummer").child_value();
	string voortouwnaam = child.child("voortouwnaam").child_value();
	string besloten = child.child("besloten").child_value();

	string voortouwafkorting = child.child("voortouwafkorting").child_value();
	string nummer = child.child("nummer").child_value();
	auto repl = [&](string name) {
	  string lname = name;
	  lname[0] = tolower(lname[0]);
	  for(auto& a : child.children(lname.c_str())) {
	    sqlw.addValue({{"skiptoken", skiptoken}, {"category", category}, {"van", id}, {"naar", a.attribute("ref").value()}, {"linkSoort", name}}, "link");
	  }
	};
	repl("vervangenVanuit");
	repl("voortgezetVanuit");
	
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"nummer", nummer}, {"soort", soort}, {"onderwerp", onderwerp},
		       {"aanvangstijd", aanvangstijd}, {"eindtijd", eindtijd}, {"besloten", besloten},
		       {"datum", datum}, {"vrsNummer", vrsNummer}, {"voortouwNaam", voortouwnaam} , {"voortouwAfkorting", voortouwafkorting}, {"noot", noot}, {"updated", updated}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt}}, category);
	  
      }
      else if(auto child = node.child("content").child("document")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}}, category);
	  continue;
	}
	string datum = child.child("datum").child_value();
	string soort = child.child("soort").child_value();
	string onderwerp = child.child("onderwerp").child_value();
	string titel = child.child("titel").child_value();
	string contentType = child.attribute("tk:contentType").value();
	int64_t contentLength = atoi(child.attribute("tk:contentLength").value());
	string vergaderjaar = child.child("vergaderjaar").child_value();
	string aanhangselnummer = child.child("aanhangselnummer").child_value();
	string documentNummer = child.child("documentNummer").child_value();
	string citeerTitel = child.child("citeerTitel").child_value();

	string huidigeDocumentVersieId = child.child("huidigeDocumentVersie").attribute("ref").value();
	string bronDocument = child.child("bronDocument").attribute("ref").value();
	string agendapuntId = child.child("agendapunt").attribute("ref").value();
	string kamerstukdossierId = child.child("kamerstukdossier").attribute("ref").value();
	int64_t volgnummer = atoi(child.child("volgnummer").child_value());;

	auto repl = [&](string name) {
	  string lname = name;
	  lname[0] = tolower(lname[0]);
	  for(auto& activiteit : child.children(lname.c_str())) {
	    sqlw.addValue({{"skiptoken", skiptoken}, {"category", category}, {"van", id}, {"naar", activiteit.attribute("ref").value()}, {"linkSoort", name}}, "link");
	  }
	};

	repl("Activiteit");
	repl("Zaak");
	
	sqlw.addValue({{"id", id},  {"skiptoken", skiptoken}, {"nummer", documentNummer}, {"agendapuntId", agendapuntId}, {"soort", soort}, {"onderwerp", onderwerp}, {"datum", datum}, {"enclosure", enclosure}, {"bronDocument", bronDocument}, {"updated", updated}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt}, {"kamerstukdossierId", kamerstukdossierId}, {"volgnummer", volgnummer}, {"titel", titel}, {"citeerTitel", citeerTitel}, {"contentLength", contentLength}, {"contentType", contentType}, {"huidigeDocumentVersieId", huidigeDocumentVersieId}, {"vergaderjaar", vergaderjaar}, {"aanhangselnummer", aanhangselnummer}}, category);
      }
      else if(auto child = node.child("content").child("zaak")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
	  continue;
	}
	  
	string gestartOp = child.child("gestartOp").child_value();
	string onderwerp = child.child("onderwerp").child_value();
	string titel = child.child("titel").child_value();
	string zaakNummer = child.child("nummer").child_value();

	string kamerstukdossierId = child.child("kamerstukdossier").attribute("ref").value();
	/*
	               x              x              
	  Multi: {"activiteit", "agendapunt", "gerelateerdVanuit", "vervangenVanuit"}
	  Hasref: {"activiteit", "agendapunt", "gerelateerdVanuit", "kamerstukdossier", "vervangenVanuit"}
	                   x          x                                  x
	*/

	auto repl = [&](string name) {
	  string lname = name;
	  lname[0] = tolower(lname[0]);
	  for(auto& a : child.children(lname.c_str())) {
	    sqlw.addValue({{"skiptoken", skiptoken}, {"category", category}, {"van", id}, {"naar", a.attribute("ref").value()}, {"linkSoort", name}}, "link");
	  }
	};
	repl("Activiteit");
	repl("gerelateerdVanuit");
	repl("vervangenVanuit");
	repl("Agendapunt");
	
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"nummer", zaakNummer}, {"kamerstukdossierId", kamerstukdossierId}, {"titel", titel}, {"onderwerp", onderwerp}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},{"gestartOp", gestartOp}, {"updated", updated}}, category);
	  
      }
      else if(auto child = node.child("content").child("kamerstukdossier")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
	  continue;
	}
	  
	string titel = child.child("titel").child_value();
	int nummer = atoi(child.child("nummer").child_value());
	  
	string afgesloten = child.child("afgesloten").child_value();
	int hoogsteVolgnummer = atoi(child.child("hoogsteVolgnummer").child_value());
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"nummer", nummer}, {"titel", titel}, {"afgesloten", afgesloten}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},{"hoogsteVolgnummer", hoogsteVolgnummer}, {"updated", updated}}, category);
      }
      else if(auto child = node.child("content").child("ns1:toezegging")) {
	if(child.attribute("ns1:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
	  continue;
	}
	/*
{"ns1:aanmaakdatum": 100, "ns1:achternaam": 100, "ns1:achtervoegsel": 10,
"ns1:activiteitNummer": 100, "ns1:datumNakoming": 53, "ns1:functie": 100,
"ns1:initialen": 100, "ns1:kamerbriefNakoming": 53, "ns1:ministerie": 100,
"ns1:naam": 100, "ns1:nummer": 100, "ns1:status": 100, "ns1:tekst": 100,
"ns1:titulatuur": 100, "ns1:tussenvoegsel": 27, "ns1:voornaam": 100}

Multi: {"ns1:isAanvullingOp", "ns1:isWijzigingVan"}
Hasref: {"ns1:activiteit", "ns1:isAanvullingOp", "ns1:isHerhalingVan", "ns1:isWijzigingVan", "ns1:toegezegdAanFractie", "ns1:toegezegdAanPersoon"}
	*/

	auto repl = [&](string xname, string name) {
	  for(auto& a : child.children(xname.c_str())) {
	    sqlw.addValue({{"skiptoken", skiptoken}, {"category", category}, {"van", id}, {"naar", a.attribute("ref").value()}, {"linkSoort", name}}, "link");
	  }
	};
	repl("ns1:isAanvullingOp", "aanvullingOp");
	repl("ns1:isHerhalingVan", "herhalingVan");
	repl("ns1:isWijzigingVan", "wijzigingVan");

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
	string persoonId = child.child("ns1:toegezegdAanPersoon").attribute("ref").value();
	//	Multi: {"ns1:isAanvullingOp", "ns1:isWijzigingVan"} XXX

	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"nummer", nummer}, {"tekst", tekst},
		       {"kamerbriefNakoming", kamerbriefNakoming}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"datum", datum}, {"ministerie", ministerie},
		       {"status", status},
		       {"datumNakoming", datumNakoming},
		       {"activiteitId", activiteitId},
		       {"fractieId", fractieId},
		       {"persoonId", persoonId},
		       {"naamToezegger", naamToezegger},
		       {"updated", updated}}, category);
      }
      else if(auto child = node.child("content").child("ns1:vergadering")) {
	if(child.attribute("ns1:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
	  continue;
	}
	  
	auto child2 = *node.child("content").begin();
	map<string,string> fields;
	for(const auto&c : child2) {
	  fields[c.name()]= c.child_value();
	}
	  
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
	map<string,string> fields;
	for(const auto&c : child2) {
	  fields[c.name()]= c.child_value();
	}
	string vergaderingId = child2.child("ns1:vergadering").attribute("ref").value();
	int64_t contentLength = atoi(child.attribute("ns1:contentLength").value());;
	string contentType = child.attribute("ns1:contentType").value();
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"updated", updated}, {"soort", fields["ns1:soort"]},
		       {"status", fields["ns1:status"]}, {"contentLength", contentLength},
		       {"contentType", contentType},
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
	map<string,string> fields;
	for(const auto&c : child2) {
	  fields[c.name()]= c.child_value();
	}
	  
	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"updated", updated}, {"functie", fields["functie"]},
		       {"initialen", fields["initialen"]},
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
		       {"stemmingSoort", fields["stemmingSoort"]},
		       {"tekst", fields["besluitTekst"]},
		       {"opmerking", fields["opmerking"]},
		       {"agendapuntZaakBesluitVolgorde", atoi(fields["agendapuntZaakBesluitVolgorde"].c_str())},
		       {"status", fields["status"]},
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
	map<string,string> fields;
	for(const auto&c : child2) {
	  fields[c.name()]= c.child_value();
	}
	  
	string besluitId = child2.child("besluit").attribute("ref").value();
	string fractieId = child2.child("fractie").attribute("ref").value();
	string persoonId = child2.child("persoon").attribute("ref").value();

	sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"updated", updated}, {"soort", fields["soort"]},
		       {"actorNaam", fields["actorNaam"]},
		       {"actorFractie", fields["actorFractie"]},
		       {"fractieGrootte", atoi(fields["fractieGrootte"].c_str())},
		       {"besluitId", besluitId},
		       {"fractieId", fractieId},
		       {"persoonId", persoonId},
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
	map<string,string> fields;
	for(const auto&c : child2) {
	  fields[c.name()]= c.child_value();
	}

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

	map<string,string> fields;
	for(const auto&c : child2) {

	  fields[c.name()]= c.child_value();
	}

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
	map<string,string> fields;
	for(const auto&c : child2) {
	  fields[c.name()]= c.child_value();
	}
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
	map<string,string> fields;
	for(const auto&c : child2) {
	  fields[c.name()]= c.child_value();
	}
	string persoonId = child.child("persoon").attribute("ref").value();

	sqlw.addValue({{"id", id}, {"skiptoken",skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"updated", updated}, {"omschrijving", fields["omschrijving"]},
		       {"datum", fields["datum"]},
		       {"gewicht", atoi(fields["gewicht"].c_str())},
		       {"persoonId", persoonId}
	  },
	  category);

      }
      else if(auto child = node.child("content").child("documentVersie")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
	  continue;
	}

	auto child2 = *node.child("content").begin();
	map<string,string> fields;
	for(const auto&c : child2) {
	  fields[c.name()]= c.child_value();
	}
	string documentId = child.child("document").attribute("ref").value();

	sqlw.addValue({{"id", id}, {"skiptoken",skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"updated", updated}, 
		       {"datum", fields["datum"]},
		       {"extensie", fields["extensie"]},
		       {"externeidentifier", fields["externeidentifier"]},
		       {"status", fields["status"]},
		       {"versienummer", atoi(fields["versienummer"].c_str())},
		       {"bestandsgrootte", atoi(fields["bestandsgrootte"].c_str())},
		       {"documentId", documentId}
	  },
	  category);

      }

      else if(auto child = node.child("content").child("activiteitActor")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
	  continue;
	}
	
	auto child2 = *node.child("content").begin();
	map<string,string> fields;
	for(const auto&c : child2) {
	  fields[c.name()]= c.child_value();
	}

	string persoonId = child.child("persoon").attribute("ref").value();
	string activiteitId = child.child("activiteit").attribute("ref").value();
	string fractieId = child.child("fractie").attribute("ref").value();
	string commissieId = child.child("commissie").attribute("ref").value();

	sqlw.addValue({{"id", id}, {"skiptoken",skiptoken}, {"verwijderd", false}, {"bijgewerkt", bijgewerkt},
		       {"updated", updated},
		       {"functie", fields["functie"]},
		       {"relatie", fields["relatie"]},
		       {"naam", fields["actorNaam"]},
		       {"fractie", fields["actorFractie"]},
		       {"spreektijd", fields["spreektijd"]},
		       {"volgorde", atoi(fields["volgorde"].c_str())},
		       {"activiteitId", activiteitId},
		       {"persoonId", persoonId},
		       {"fractieId", fractieId},
		       {"commissieId", commissieId}
	  },
	  category);

      }
      else if(auto child = node.child("content").child("commissie")) {
	if(child.attribute("tk:verwijderd").value() == string("true")) {
	  sqlw.addValue({{"id", id}, {"skiptoken", skiptoken}, {"verwijderd", true}, {"updated", updated}}, category);
	  continue;
	}
	  
	auto child2 = *node.child("content").begin();
	map<string,string> fields;
	for(const auto&c : child2) {
	  fields[c.name()]= c.child_value();
	}

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
    cout<<"Cleanup.."<<endl;
    sqlw.query(fmt::format("delete from {} where skiptoken in (select skiptoken from {} v,(select id,max(skiptoken) as mskiptoken,count(1) as c from {} group by id having c > 1) as d where v.id = d.id and v.skiptoken < d.mskiptoken)", category, category, category));
    sqlw.query(fmt::format("delete from {} where verwijderd=1", category));
    cout<<"Done with "<<category <<" - saw "<<numentries<<" new entries"<<endl;
  }
  cout<<"Link cleanup.."<<endl;
  sqlw.query("delete from Link where rowid in(select Link.rowid from Link,(select van,naar,linkSoort,max(rowid) as mrowid, count(1) c from Link group by 1,2,3 having c > 1) as t where Link.van=t.van and Link.naar=t.naar and link.linkSoort=t.linkSoort and Link.rowid < mrowid)");

}
