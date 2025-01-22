#include "search.hh"
#include "support.hh"
using namespace std;

/*
  Need to do categories too
  And specify cutoff
 */

std::vector<SearchHelper::Result> SearchHelper::search(const std::string& query, const std::set<string>& categories, const std::string& cutoff)
{
  std::vector<SearchHelper::Result> ret;
  // nummer, relurl, category, date, snippet, title

  string categoriesstr;
  for(const auto& c : categories) {
    if(!categoriesstr.empty())
      categoriesstr +=  ", ";
    if(c.find_first_of("'\"") != string::npos)
      continue;
    categoriesstr+= "'" + c + "'";
  }
  auto matches = d_sqw.queryT("SELECT uuid, datum, snippet(docsearch,-1, '<b>', '</b>', '...', 20) as snip,  category FROM docsearch WHERE docsearch match ? and (datum >= ? or datum='') and (? or category in ("+categoriesstr+"))", {query, cutoff, categories.empty()});

  for(auto& m : matches) {
    Result r;

    string category = eget(m, "category");
    string id = eget(m, "uuid");
    r.categorie = category;
    r.snippet = eget(m, "snip");
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
      //      cout << "Got Document "<< eget(doc[0], "nummer") <<", onderwerp "<<eget(doc[0],"onderwerp")<<endl;
    }
    else if(category == "Activiteit") {
      auto act = d_sqw.queryT("select * from Activiteit where id=?", {id});
      if(act.empty()) {
	fmt::print("Weird, could not find activity {} in tk", id);
	continue;
      }
      r.nummer = eget(act[0], "nummer");
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
      r.nummer = id;
      r.onderwerp = eget(verslag[0], "onderwerp");
      r.titel = eget(verslag[0], "titel");
      r.bijgewerkt = eget(verslag[0], "bijgewerkt");
    }
    else {
      cout<<"Unknown category '"<<category<<"'\n";
    }
    //    cout<<"\t"<<eget(m, "snip")<<"\n";
    ret.push_back(r);
  }
  return ret;
}
