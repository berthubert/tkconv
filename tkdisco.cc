#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
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

  if(argc > 1) {
    categories.clear();
    for(int n = 1 ; n < argc; ++n)
      categories.push_back(argv[n]);
  }
  SQLiteWriter xmlstore("xml.sqlite3");
  
  for(const auto& category: categories) {
    int skiptoken = -1;
    int numentries=0;  

    auto entries = xmlstore.queryT("select * from "+category+" where skiptoken > ?", {skiptoken});
    map<string, int> fields;
    set<string> multis, hasref;
    for(auto& exml : entries) {
      pugi::xml_document pnode;
      if (!pnode.load_string( (get<string>(exml["xml"])).c_str())) {
	cout<<"Could not load"<<endl;
	return -1;
      }
      pugi::xml_node node = pnode.child("entry");
      string id = node.child("title").child_value();
      auto child2 = *node.child("content").begin();
      numentries++;
      //      cout << child2.name() << endl;
      map<string, int> perentry;
      for(const auto&c : child2) {
	//	cout << c.name() <<"='"<<c.child_value()<<"' ";
	if(!string(c.child_value()).empty())
	  fields[c.name()]++;
	perentry[c.name()]++;
	string ref = c.attribute("ref").value();
	if(!ref.empty())
	  hasref.insert(c.name());
      }
      for(const auto& p : perentry) {
	if(p.second > 1)
	  multis.insert(p.first);
      }
    }
    for(auto& f : fields)
      f.second = 100.0*f.second/numentries;
    fmt::print("{}: \n{}\n", category, fields);
    fmt::print("Multi: {}\n", multis);
    fmt::print("Hasref: {}\n\n", hasref);
  }
}
