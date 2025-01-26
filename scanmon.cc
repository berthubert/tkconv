#include "scanmon.hh"
#include "search.hh"

std::string ZoekScanner::getType() 
{
  return "Zoekopdracht";
}

std::unique_ptr<Scanner> ZoekScanner::make(SQLiteWriter& sqlw, const std::string& id) 
{  
  ZoekScanner s;
  auto row = s.getRow(sqlw, id);  
  s.d_query = eget(row, "query");
  s.d_categorie = eget(row, "categorie");
  return std::make_unique<ZoekScanner>(s);
}

// needs sqlw with tk.sqlite3 (as meta.*) AND tkindex
std::vector<ScannerHit> ZoekScanner::get(SQLiteWriter& sqlw) 
{
  SearchHelper sh(sqlw);
  
  auto matches = sh.search(d_query, {}, d_cutoff);

  // nummer/identifier, datum, category
  std::vector<ScannerHit> ret;
  for(auto& m : matches) {
    ret.emplace_back(m.nummer, m.datum, m.categorie, m.relurl);
  }
  return ret;
    
}

std::string ZoekScanner::describe(SQLiteWriter& sqlw) 
{
  std::string ret = "Zoekopdracht " + d_query;
  if(!d_categorie.empty())
    ret += " (categorie "+d_categorie+")";
  return ret;
}


std::map<std::string, decltype(&ZaakScanner::make)> g_scanmakers  =
    {
      {"activiteit", ActiviteitScanner::make},
      {"zaak", ZaakScanner::make},
      {"ksd", KsdScanner::make},
      {"zoek", ZoekScanner::make},
      {"commissie", CommissieScanner::make},
      {"persoon", PersoonScanner::make},
    };
