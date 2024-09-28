#include <fmt/format.h>
#include <fmt/printf.h>
#include <iostream>
#include <sstream>
#include <set>
#include "support.hh"
#include "sqlwriter.hh"
#include "pugixml.hpp"

using namespace std;

vector<string> split_string(const string& input)
{
  istringstream iss(input);
  vector<string> tokens;
  string token;
  while (iss >> token)
    tokens.push_back(token);
  return tokens;
}

struct simple_walker: pugi::xml_tree_walker
{
  virtual bool for_each(pugi::xml_node& node)
  {
    //    for (int i = 0; i < depth(); ++i) std::cout << "  "; // indentation
    
    //    std::cout << "name='" << node.name() << "', value='" << node.value() << "'\n";
    if(string(node.name()) == "sprekers") {
      found = node;
      return false;
    }
    return true; // continue traversal
  }
  pugi::xml_node found;
};


int main(int argc, char** argv)
{
  SQLiteWriter sqlw("tk.sqlite3");

  string limit="2023-01-01";
  auto alleVerslagen = sqlw.queryT("select Verslag.id as id, vergadering.id as vergaderingid,datum, vergadering.titel as onderwerp, '' as titel, 'Verslag' as category from Verslag,Vergadering where status!= 'Casco' and Verslag.vergaderingId=Vergadering.id and datum > ? order by datum desc, verslag.updated desc", {limit});

  set<string> seenvergadering;
  decltype(alleVerslagen) wantVerslagen;
  for(auto& v: alleVerslagen) {
    string vid = get<string>(v["vergaderingid"]);
    if(seenvergadering.count(vid)) 
      continue;
    wantVerslagen.push_back(v);
    seenvergadering.insert(vid);
  }

  for(auto& v : wantVerslagen) {
    pugi::xml_document pnode;
    string fname = get<string>(v["id"]);
    if (!pnode.load_file(makePathForId(fname).c_str())) {
      cout<<"Could not load "<<fname<<endl;
      return -1;
    }
    simple_walker walker;
    pnode.traverse(walker);
    
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
    cout <<"'"<<voornaam << "' '" << achternaam << "' - "<<functie<<endl;

    if(achternaam=="Ram")
      achternaam="Ram ";
    else if(achternaam=="Boomsma" && voornaam == "Diederik")
      voornaam += " ";
    else if(achternaam=="Patijn" && voornaam == "Mariëtte")
      voornaam += " ";
    else if(achternaam=="Soepboer" && voornaam == "Aant Jelle")
      voornaam += " ";
    else if(achternaam=="Welzijn" && voornaam == "Merlien")
      voornaam += " ";
    else if(achternaam=="Eppink" && voornaam == "Derk Jan")
      voornaam += " ";
    else if(achternaam=="El Abassi" && voornaam == "Ismail")
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
	  cout<<n<<"'"<<stukjes[n]<<"'"<<endl;
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
	fmt::print("Poging 2 achternaam '{}', tussenvoegsel '{}'\n",
		   nachternaam, tussenvoegsel);
	p = sqlw.queryT("select * from Persoon where achternaam=? and tussenvoegsel=? and roepnaam=?",
			{
			  nachternaam, tussenvoegsel, voornaam
			});
	if(!p.empty())
	  break;
      }
      
    }
    cout<<" "<<p.size()<<" matches\n";
    if(p.size()==1) {
      cout<<sprekerid<< " -> "<< get<string>(p[0]["id"])<<endl;
      sqlw.addValue({{"vergaderingId", get<string>(v["vergaderingid"])},
	    {"verslagId", get<string>(v["vergaderingid"])},
	    {"persoonId", get<string>(p[0]["id"])},
	    {"sprekerId", sprekerid}}, "VergaderingSpreker");
	  
    }
    else
      cout<<"Oops!\n"<<endl;
  }

  }
}
