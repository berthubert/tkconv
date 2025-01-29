#include "search.hh"
#include "support.hh"
using namespace std;


std::vector<SearchHelper::Result> SearchHelper::search(const std::string& query, const std::set<string>& categories, const std::string& cutoff, unsigned int mseclimit, unsigned int itemlimit)
{
  std::vector<SearchHelper::Result> ret;
  // nummer, category, relurl, score, date, snippet, title

  string categoriesstr;
  for(const auto& c : categories) {
    if(!categoriesstr.empty())
      categoriesstr +=  ", ";
    if(c.find_first_of("'\"") != string::npos)
      continue;
    categoriesstr+= "'" + c + "'";
  }
  auto matches = d_sqw.queryT("SELECT uuid, datum, snippet(docsearch,-1, '<b>', '</b>', '...', 20) as snip,  category, bm25(docsearch) as score FROM docsearch WHERE docsearch match ? and (datum >= ? or datum='') and (? or category in ("+categoriesstr+")) order by rank"
			      + (itemlimit ? " limit "+to_string(itemlimit) : ""),
			      {query, cutoff, categories.empty()}, mseclimit);

  for(auto& m : matches) {
    Result r;

    string category = eget(m, "category");
    string id = eget(m, "uuid");
    r.categorie = category;
    r.snippet = eget(m, "snip");
    r.score = get<double>(m["score"]);
    r.datum = eget(m, "datum");
    if(category == "Document") {
      auto doc =  d_sqw.queryT("select onderwerp, soort, bijgewerkt, titel, nummer, datum FROM meta.Document where id=?",   {id});
      if(doc.empty())
	continue;
      r.nummer = eget(doc[0], "nummer");
      r.onderwerp = eget(doc[0], "onderwerp");
      r.titel = eget(doc[0], "titel");
      r.bijgewerkt = eget(doc[0], "bijgewerkt");
      r.soort = eget(doc[0], "soort");
      r.relurl = "get/"+r.nummer;
      //      cout << "Got Document "<< eget(doc[0], "nummer") <<", onderwerp "<<eget(doc[0],"onderwerp")<<endl;
    }
    else if(category == "Activiteit") {
      auto act = d_sqw.queryT("select * from Activiteit where id=?", {id});
      if(act.empty()) {
	fmt::print("Weird, could not find activity {} in tk", id);
	continue;
      }
      r.nummer = eget(act[0], "nummer");
      r.relurl = "activiteit.html?nummer="+ r.nummer;
      r.onderwerp = eget(act[0], "onderwerp");
      r.titel = eget(act[0], "titel");
      r.bijgewerkt = eget(act[0], "bijgewerkt");
      r.soort = eget(act[0], "soort");
      /*
      CREATE TABLE Activiteit ('id' TEXT PRIMARY KEY, 'skiptoken' INT, "nummer" TEXT, "soort" TEXT, "onderwerp" TEXT, "aanvangstijd" TEXT, "eindtijd" TEXT, "besloten" TEXT, "datum" TEXT, "vrsNummer" TEXT, "voortouwNaam" TEXT, "voortouwAfkorting" TEXT, "noot" TEXT, "updated" TEXT, "bijgewerkt" TEXT) STRICT;
      */
      //      cout << "Got Activiteit "<< eget(act[0], "nummer")<<", onderwerp "<<eget(act[0],"onderwerp") << endl;
    }
    else if(category == "Verslag") {
      auto verslag = d_sqw.queryT("SELECT titel as onderwerp, Vergadering.id as vergaderingId, Verslag.updated as bijgewerkt, '' as titel, Vergadering.datum FROM meta.Verslag, meta.Vergadering WHERE Verslag.id = ? and Vergadering.id = Verslag.vergaderingId", {id});
      
      if(verslag.empty()) {
	fmt::print("Weird, could not find verslag {} in tk", id);
	continue;
      }
      // cout << "Got Verslag from vergadering "<< eget(verslag[0], "vergaderingId")<<", onderwerp "<<eget(verslag[0],"onderwerp") << endl;
      // we let users find the right Verslag by the Vergaderingid
      r.nummer = eget(verslag[0], "vergaderingId");
      r.relurl = "verslag.html?vergaderingid="+r.nummer;
      r.onderwerp = eget(verslag[0], "onderwerp");
      r.titel = eget(verslag[0], "titel");
      r.bijgewerkt = eget(verslag[0], "bijgewerkt");
    }
    else if(category == "Toezegging") {
      auto toez = d_sqw.queryT("SELECT tekst, nummer, bijgewerkt FROM meta.Toezegging WHERE id = ?", {id});
      
      if(toez.empty()) {
	fmt::print("Weird, could not find toezegging {} in tk", id);
	continue;
      }
      // cout << "Got Verslag from vergadering "<< eget(verslag[0], "vergaderingId")<<", onderwerp "<<eget(verslag[0],"onderwerp") << endl;
      r.nummer = eget(toez[0], "nummer");
      r.relurl = "toezegging.html?nummer=" + r.nummer;
      r.titel = "Toezegging van " +eget(toez[0], "naamToezegger") + " ("+eget(toez[0], "ministerie") +")";
      r.onderwerp = eget(toez[0], "tekst");
      r.bijgewerkt = eget(toez[0], "bijgewerkt");
    }
    else if(category == "PersoonGeschenk") {
      auto gesch = d_sqw.queryT("SELECT datum, persoongeschenk.bijgewerkt, omschrijving, roepnaam, tussenvoegsel, achternaam, persoon.nummer nummer from PersoonGeschenk, Persoon where PersoonGeschenk.id = ? and Persoon.id = Persoonid", {id});
      
      if(gesch.empty()) {
	fmt::print("Weird, could not find geschenk {} in tk", id);
	continue;
      }
      r.nummer = id;
      r.relurl = "persoon.html?nummer=" + to_string(get<int64_t>(gesch[0]["nummer"])) +"#" + id;
      r.titel = eget(gesch[0], "achternaam");
      r.onderwerp = "Geschenk aan " + eget(gesch[0], "roepnaam") 
	+ " " + eget(gesch[0], "tussenvoegsel") + " " + eget(gesch[0], "achternaam")+": " +
	eget(gesch[0], "omschrijving");
      r.bijgewerkt = eget(gesch[0], "bijgewerkt");
      r.persoonnummer = std::get<int64_t>(gesch[0]["nummer"]);
    }

    else {
      cout<<"Unknown category '"<<category<<"'\n";
    }
    //    cout<<"\t"<<eget(m, "snip")<<"\n";
    ret.push_back(r);
  }
  return ret;
}

set<pair<string,string>> getZakenFromDocument(const std::string& id)
{
  SQLiteWriter own("tkindex-small.sqlite3", SQLWFlag::ReadOnly);
  own.query("ATTACH database 'tk.sqlite3' as meta");
  string q = "\""+id+"\"";

  set<pair<string, string>> ret;
  auto rows = own.queryT("select uuid, tekst from docsearch where docsearch match ?", {q});
  if(rows.empty())
    return ret;

  std::regex zmatch("(20[0123][0-9]Z[0-9][0-9][0-9][0-9][0-9])");
  auto tekst = eget(rows[0], "tekst");
  auto words_begin = 
        std::sregex_iterator(tekst.begin(), tekst.end(), zmatch);
  auto words_end = std::sregex_iterator();

  // 8064a13e-3827-4f30-a657-f0ff85f5a344
  for (auto i = words_begin; i != words_end; ++i) {
    string nummer = i->str();

    auto znummer = own.queryT("select id from Zaak where nummer=?", {nummer});
    if(znummer.size() == 1) {
      cout<<"Got: '"<<nummer<<" -> id "<<eget(znummer[0], "id")<<endl;
      ret.insert({i->str(), eget(znummer[0], "id")});
    }
  }
  return ret;

}
