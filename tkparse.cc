#include <fmt/format.h>
#include <fmt/printf.h>
#include <iostream>
#include <sstream>
#include <set>
#include "support.hh"
#include "sqlwriter.hh"
#include "pugixml.hpp"

using namespace std;

static vector<string> split_string(const string& input)
{
  istringstream iss(input);
  vector<string> tokens;
  string token;
  while (iss >> token)
    tokens.push_back(token);
  return tokens;
}

struct sprekers_walker: pugi::xml_tree_walker
{
  virtual bool for_each(pugi::xml_node& node)
  {
    if(string(node.name()) == "sprekers") {
      found = node;
      return false;
    }
    return true; // continue traversal
  }
  pugi::xml_node found;
};


struct vragen_walker: pugi::xml_tree_walker
{
  virtual bool for_each(pugi::xml_node& node)
  {
    if(string(node.name()) == "vraag") {
      vragen++;
    }
    return true; // continue traversal
  }
  unsigned int vragen=0;
};


struct tekst_walker : pugi::xml_tree_walker
{
  virtual bool for_each(pugi::xml_node& node)
  {
    string nv= node.value();
    if(!tekst.empty() && *tekst.rbegin()!=' ' && !nv.empty() && nv[0]!=' ')
      tekst += " ";
    tekst += nv;
    return true;
  }
  string tekst;
};

struct spreektijd_walker: pugi::xml_tree_walker
{
  virtual bool for_each(pugi::xml_node& node)
  {
    string name = node.name();
    if(name == "woordvoerder" || name=="interrumpant") {
      auto spreker = node.child("spreker");
      auto begintijd = node.child("markeertijdbegin").child_value();
      auto eindtijd = node.child("markeertijdeind").child_value();
      bool isVoorzitter = string(node.child("isvoorzitter").child_value())=="true";
      auto d = getTstamp(eindtijd) - getTstamp(begintijd);
      string sprekerId = spreker.attribute("objectid").value();
      // || sprekerId=="c7aa0e9d-3370-4c56-a461-682e83bcab4f"
      if(d > 900 || d < 0 ) {
	fmt::print("{} ({}): {} - {} {} {} seconds\n",
		   spreker.child("achternaam").child_value(),
		   sprekerId,
		   begintijd,
		   eindtijd, name, d);
      }
      TextTime tt;

      tt.seconds = d;

      if(isVoorzitter)  {
	seconds[sprekerId].voorzitterSeconds += d;
	tt.type="voorzitter";
      }
      else if(name=="woordvoerder") {
	seconds[sprekerId].woordvoerderSeconds += d;
	tt.type = name;
      }
      else if(name=="interrumpant") {
	seconds[sprekerId].interrumpantSeconds += d;
	tt.type = name;
      }
      else
	throw std::runtime_error("Impossible spreker type "+name);

      tekst_walker tw;
      node.child("tekst").traverse(tw);
      tt.tekst = tw.tekst;
      //      cout<<"tekst: '"<< tw.tekst <<"'\n";
      
      seconds[sprekerId].texts.push_back({begintijd, tt});
	
    }
    return true; // continue traversal
  }
  struct TextTime
  {
    string type;
    string tekst;
    int seconds = 0;
  };

  struct Times
  {
    int woordvoerderSeconds = 0;
    int interrumpantSeconds = 0;
    int voorzitterSeconds = 0;
    int sum() {
      return woordvoerderSeconds + interrumpantSeconds + voorzitterSeconds;
    }
    vector<pair<string,TextTime>> texts;
  };

  std::unordered_map<string, Times> seconds;
};


void doSchriftelijkeVragen(SQLiteWriter& sqlw)
{
  sqlw.queryT("create table if not exists SchriftelijkeVraagStat (documentId TEXT PRIMARY KEY, documentNummer TEXT, aantal INT)");

  sqlw.queryT("create index if not exists docnumidx on SchriftelijkeVraagStat(documentNummer)");

  
  auto alreadyRows = sqlw.queryT("select documentId from SchriftelijkeVraagStat");
  set<string> skip;
  for(const auto& ar : alreadyRows)
    skip.insert(eget(ar, "documentId"));
  
  auto potentials = sqlw.queryT("select Document.nummer,DocumentVersie.externeidentifier,Document.id from Document,DocumentVersie where soort='Schriftelijke vragen' and DocumentVersie.documentid = document.id and +externeidentifier!=''");
  fmt::print("Got {} schriftelijke vragen with an external identifier, and {} ids we counted already\n",
	     potentials.size(), skip.size());
  unsigned int totvragen = 0, skipped=0, looked=0;
  for(auto& p : potentials) {
    if(skip.count(eget(p, "id"))) {
      skipped++;
      continue;
    }
    string eid = eget(p, "externeidentifier");
    string fname = makePathForExternalID(eid, "op", ".xml");
    pugi::xml_document pnode;
    if (!pnode.load_file(fname.c_str())) {
      cout<<"Could not load '"<<eid<<"' from "<< fname <<endl;
      continue;
    }
    vragen_walker walker;
    pnode.traverse(walker);
    //    fmt::print("Got {} vragen for {}\n", walker.vragen, eget(p, "nummer"));
    sqlw.addOrReplaceValue({{"documentId", eget(p, "id")}, {"documentNummer", eget(p, "nummer")}, {"aantal", walker.vragen}}, "SchriftelijkeVraagStat");
    totvragen += walker.vragen;
    looked++;
  }
  fmt::print("In total saw {} vragen over {} documents we looked at, skipped {}\n",
	     totvragen, looked, 
	     skipped);
}

int main(int argc, char** argv)
{
  SQLiteWriter sqlw("tk.sqlite3");

  doSchriftelijkeVragen(sqlw);

  // so this is unique on vergaderingId and not on verslagId - we only want one entry per vergadering!
  
  sqlw.queryT("create table if not exists VergaderingSpreker (vergaderingId TEXT, verslagId, persoonId TEXT)");
  sqlw.queryT("create unique index if not exists uniidx on VergaderingSpreker(vergaderingId, persoonId)");

  sqlw.queryT("create table if not exists VergaderingSprekerTekst (vergaderingId TEXT, verslagId, persoonId TEXT, beginTijd TEXT)");
  sqlw.queryT("create unique index if not exists unisidx on VergaderingSprekerTekst(vergaderingId, persoonId, beginTijd)");
  
  string limit="2023-01-01";

  auto alleVerslagen = sqlw.queryT("select Verslag.id as id, vergadering.id as vergaderingid,datum, vergadering.titel as onderwerp, '' as titel, 'Verslag' as category from Verslag,Vergadering where status!= 'Casco' and Verslag.vergaderingId=Vergadering.id and datum > ? order by datum desc, verslag.updated desc", {limit});

  // the model is, if we've seen *something* from a verslag, we won't parse it again
  auto doneVerslagen = sqlw.queryT("select distinct(verslagId) s from VergaderingSpreker");
  unordered_set<string> doneAlready;
  for(auto& dv: doneVerslagen)
    doneAlready.insert(get<string>(dv["s"]));
  
  set<string> seenvergadering;
  decltype(alleVerslagen) wantVerslagen;
  for(auto& v: alleVerslagen) {
    string vid = get<string>(v["vergaderingid"]);
    if(seenvergadering.count(vid)) 
      continue;
    if(!doneAlready.count(get<string>(v["id"])))
      wantVerslagen.push_back(v);
    seenvergadering.insert(vid);
  }

  for(auto& v : wantVerslagen) {
    pugi::xml_document pnode;
    string id = get<string>(v["id"]);
    string fname = makePathForId(id).c_str();
    if (!pnode.load_file(fname.c_str())) {
      cout<<"Could not load '"<<id<<"' from "<< fname <<endl;
      return -1;
    }
    //    fmt::print("Loaded {}\n", fname);
    sprekers_walker walker;
    pnode.traverse(walker);

    spreektijd_walker sw;
    pnode.traverse(sw);

    /*
    <spreker soort="Minister" objectid="6695f235-a643-4413-bc45-9c81ee79fa56">
<aanhef>De heer</aanhef>
<verslagnaam>Brekelmans</verslagnaam>
<weergavenaam>Brekelmans</weergavenaam>
<voornaam>Ruben</voornaam>
<achternaam>Brekelmans</achternaam>
<functie>minister van Defensie</functie>
</spreker>
<spreker soort="Tweede Kamerlid" objectid="dbca4b1a-166e-4a48-8490-a409abbe6b9e">
<fractie>PVV</fractie>
<aanhef>De heer</aanhef>
<verslagnaam>Deen</verslagnaam>
<weergavenaam>Deen</weergavenaam>
<voornaam>Marco</voornaam>
<achternaam>Deen</achternaam>
<functie>lid Tweede Kamer</functie>
</spreker>
  */

    set<string> seen;
    for(auto& spreker : walker.found.children("spreker")) {
      //    spreker.print(cout, "\t", pugi::format_raw);
      string sprekerid=spreker.attribute("objectid").value();
      string achternaam=spreker.child("achternaam").child_value();
      string voornaam=spreker.child("voornaam").child_value();
      string functie=spreker.child("functie").child_value();
      if(functie != "lid Tweede Kamer")
	continue;
      if(seen.count(sprekerid))
	continue;
      seen.insert(sprekerid);
      //      cout <<"'"<<voornaam << "' '" << achternaam << "' - "<<functie<<endl;
      
      if(achternaam=="El Abassi" && voornaam == "Ismail")
	achternaam = "Abassi el";
      else if(achternaam=="Koerhuis" && voornaam == "Daniël")
	voornaam = "Daniel";
      
      auto p = sqlw.queryT("select * from Persoon where achternaam=? and roepnaam=? and tussenvoegsel=''",
			   {achternaam,
			    voornaam});
      
      if(p.empty()) {
	auto stukjes = split_string(achternaam);
	
	//      <achternaam>Nispen van</achternaam>
	
	for(unsigned int snip = 1; snip < stukjes.size(); ++snip) {
	  string nachternaam;
	  string tussenvoegsel;
	  for(unsigned int n = 0 ; n < stukjes.size() ; ++n) {
	    //	    cout<<n<<"'"<<stukjes[n]<<"'"<<endl;
	    if(n < stukjes.size() - snip ) {
	      if(!nachternaam.empty())
		nachternaam+=" ";
	      nachternaam+=stukjes[n];
	    }
	    else {
	      if(!tussenvoegsel.empty())
		tussenvoegsel += " ";
	      tussenvoegsel += stukjes[n];
	    }
	  }
	  //	  fmt::print("Poging 2 achternaam '{}', tussenvoegsel '{}'\n",
	  //		     nachternaam, tussenvoegsel);
	  p = sqlw.queryT("select * from Persoon where achternaam=? and tussenvoegsel=? and roepnaam=?",
			  {
			    nachternaam, tussenvoegsel, voornaam
			  });
	  if(!p.empty())
	    break;
	}
	
      }
      //      cout<<" "<<p.size()<<" matches\n";
      if(p.size()==1) {
	//	cout<<sprekerid<< " -> "<< get<string>(p[0]["id"])<<endl;
	sqlw.addOrReplaceValue({{"vergaderingId", get<string>(v["vergaderingid"])},
				{"verslagId", get<string>(v["id"])},
				{"persoonId", get<string>(p[0]["id"])},
				{"sprekerId", sprekerid},
				{"spreekSeconden", sw.seconds[sprekerid].sum()},
				{"woordvoerderSeconden", sw.seconds[sprekerid].woordvoerderSeconds},
				{"interrumpantSeconden", sw.seconds[sprekerid].interrumpantSeconds},
				{"voorzitterSeconden", sw.seconds[sprekerid].voorzitterSeconds}
				}, "VergaderingSpreker");

	auto& si = sw.seconds[sprekerid];
	for(auto& t : si.texts) {
	  sqlw.addOrReplaceValue({
	      {"vergaderingId", get<string>(v["vergaderingid"])},
	      {"verslagId", get<string>(v["id"])},
	      {"persoonId", get<string>(p[0]["id"])},
	      {"sprekerId", sprekerid},
	      {"beginTijd", t.first},
	      {"tekst", t.second.tekst},
	      {"type", t.second.type},
	      {"seconden", t.second.seconds}
	    }, "VergaderingSprekerTekst");
	}
	
      }
      else
	cout<<"Oops - "<<p.size()<< " '"<<achternaam<<"' '"<<voornaam<<"'"<<endl;
    }
  }
}
