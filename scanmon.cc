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
    if(m.verantwoordelijke.find("gemeente") != 0) // temporary filter
      ret.emplace_back(m.nummer, m.datum, m.categorie, m.relurl, m.snippet);
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


std::vector<ScannerHit> OODocumentVerantwoordelijkeScanner::get(SQLiteWriter& sqlw) 
{
  auto hits = sqlw.queryT("select id,openbaarmakingsdatum from oo.OODocument where openbaarmakingsdatum >= ? and verantwoordelijke=?",
			  {d_cutoff,
			   d_verantwoordelijke
			  });
  
  std::vector<ScannerHit> ret;
  for(auto& h : hits) {
    ScannerHit sh{.identifier=eget(h, "id"),
		  .date =eget(h, "openbaarmakingsdatum"),
		  .kind = "OODocument",
		  .relurl = "oo.html?nummer="+eget(h, "id"),
    };
    ret.push_back(sh);
  }
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
      {"toezeggingen", ToezeggingenScanner::make},
      {"OODocumentVerantwoordelijke", OODocumentVerantwoordelijkeScanner::make}
    };
