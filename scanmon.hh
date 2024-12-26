#pragma once
#include <string>
#include "sqlwriter.hh"
#include "support.hh"

struct ScannerHit
{
  std::string identifier;
  std::string date;
  std::string kind;
};


struct Scanner
{
  virtual std::string describe(SQLiteWriter& sqlw) = 0;
  virtual std::string getType() = 0;
  virtual std::vector<ScannerHit> get(SQLiteWriter& sqlw) = 0;
  auto getRow(SQLiteWriter& sqlw, const std::string& id)
  {
    auto res = sqlw.queryT("select * from scanners where id=?", {id});
    if(res.empty())
      throw std::runtime_error("No such ID");
    d_cutoff = eget(res[0], "cutoff");
    d_id = id;
    d_userid = eget(res[0], "userid");
    d_soort = eget(res[0], "soort");
    return res[0];
  }
  auto sqlToScannerHits(std::vector<std::unordered_map<std::string,MiniSQLite::outvar_t>>& hits)
  {
    std::vector<ScannerHit> ret;
    ret.reserve(hits.size());
    for(const auto& h : hits) {
      ScannerHit sh;
      sh.identifier = eget(h, "nummer");
      sh.date = eget(h, "datum");
      sh.kind = "Document";
      ret.push_back(sh);
    }
    return ret;
  }
  std::string d_cutoff, d_userid, d_soort;
  std::string d_id;
};


struct CommissieScanner : Scanner
{
  std::string getType() override
  {
    return "Commissie";
  }
  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, const std::string& id) 
  {  
    CommissieScanner s;
    auto row = s.getRow(sqlw, id);  
    s.d_commissieid = eget(row, "commissieId");
    return std::make_unique<CommissieScanner>(s);
  }
  
  std::vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    auto hits = sqlw.queryT("select datum,document.nummer,relatie from ZaakActor,Zaak,link,document where commissieId=? and ZaakId=zaak.id and naar=zaak.id and document.id=van and datum >= ? and category='Document' and relatie='Voortouwcommissie' order by datum", {d_commissieid, d_cutoff});
    return sqlToScannerHits(hits);
  }
  
  std::string describe(SQLiteWriter& sqlw) override
  {
    auto orow = sqlw.queryT("select naam from commissie where id=?", {d_commissieid});
    std::string naam;
    if(!orow.empty()) {
      naam = eget(orow[0], "naam");
    }
    return naam;
  }
  std::string d_commissieid;
};



 
struct PersoonScanner : Scanner
{
  std::string getType() override
  {
    return "Persoon";
  }

  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, const std::string& id) 
  {  
    PersoonScanner zs;
    auto row = zs.getRow(sqlw, id);  
    zs.d_nummer = eget(row, "nummer");
    return std::make_unique<PersoonScanner>(zs);
  }
  
  std::vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    auto hits = sqlw.queryT("select relatie,document.nummer, document.datum from Document,DocumentActor,Persoon where persoon.nummer=? and documentactor.documentid=document.id and persoon.id=persoonid and datum >= ?", {d_nummer, d_cutoff});

    auto vhits = sqlw.queryT("select vergaderingid as nummer, datum from Vergaderingspreker,vergadering,persoon where vergadering.id=vergaderingid and persoon.nummer=? and spreekSeconden > 0 and persoon.id = persoonid and datum >= ?", {d_nummer, d_cutoff});

    for(const auto& vh: vhits)
      hits.push_back(vh);
    
    return sqlToScannerHits(hits);
  }
  
  std::string describe(SQLiteWriter& sqlw) override
  {
    auto orow = sqlw.queryT("select roepnaam,tussenvoegsel,achternaam from Persoon where nummer=?", {d_nummer});
    std::string naam;
    if(!orow.empty()) {
      naam = eget(orow[0], "roepnaam");
      std::string tv =eget(orow[0], "tussenvoegsel");
      if(!tv.empty()) {
	naam += " " +tv;
      }
      naam += " " +eget(orow[0], "achternaam");
    }
    return "Persoon " + naam;
  }
  std::string d_nummer;
};


struct ActiviteitScanner : Scanner
{
  std::string getType() override
  {
    return "Activiteit";
  }

  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, const std::string& id) 
  {  
    ActiviteitScanner s;
    auto row = s.getRow(sqlw, id);  
    s.d_nummer = eget(row, "nummer");
    return std::make_unique<ActiviteitScanner>(s);
  }
  
  std::vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    auto hits = sqlw.queryT("select Document.nummer, document.datum from link,Document,activiteit where linkSoort='Activiteit' and link.naar=activiteit.id and activiteit.nummer=? and Document.id=link.van and document.datum >= ?", {d_nummer, d_cutoff});
    return sqlToScannerHits(hits);
  }
  
  std::string describe(SQLiteWriter& sqlw) override
  {
    auto orow = sqlw.queryT("select onderwerp from Activiteit where nummer=?", {d_nummer});
    std::string onderwerp;
    if(!orow.empty()) {
      onderwerp = eget(orow[0], "onderwerp");
    }
    return "Activiteit " + d_nummer+": "+ onderwerp;
  }
  std::string d_nummer;
};

struct ZaakScanner : Scanner
{
  std::string getType() override
  {
    return "Zaak";
  }

  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, const std::string& id) 
  {  
    ZaakScanner zs;
    auto row = zs.getRow(sqlw, id);  
    zs.d_nummer = eget(row, "nummer");
    return std::make_unique<ZaakScanner>(zs);
  }
  
  std::vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    auto hits = sqlw.queryT("select document.nummer, document.datum from Zaak,Link,Document where zaak.nummer=? and zaak.id=link.naar and document.id = link.van and datum >= ?", {d_nummer, d_cutoff});
    return sqlToScannerHits(hits);
  }
  
  std::string describe(SQLiteWriter& sqlw) override
  {
    auto orow = sqlw.queryT("select onderwerp from Zaak where nummer=?", {d_nummer});
    std::string onderwerp;
    if(!orow.empty()) {
      onderwerp = eget(orow[0], "onderwerp");
    }
    return "Zaak " + d_nummer+": "+ onderwerp;
  }
  std::string d_nummer;
};


struct KsdScanner : Scanner
{
  std::string getType() override
  {
    return "Kamerstukdossier";
  }

  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, const std::string& id) 
  {  
    KsdScanner s;
    auto row = s.getRow(sqlw, id);  
    s.d_nummer = eget(row, "nummer");
    s.d_toevoeging = eget(row, "toevoeging");
    return std::make_unique<KsdScanner>(s);
  }
  
  std::vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    auto hits = sqlw.queryT("select document.nummer,datum from document,kamerstukdossier where kamerstukdossierid=kamerstukdossier.id and kamerstukdossier.nummer=? and toevoeging=? and datum >= ?", {d_nummer, d_toevoeging, d_cutoff});
    return sqlToScannerHits(hits);
  }
  
  std::string describe(SQLiteWriter& sqlw) override
  {
    auto orow = sqlw.queryT("select titel from kamerstukdossier where nummer=? and toevoeging=?", {d_nummer, d_toevoeging});
    std::string onderwerp;
    if(!orow.empty()) {
      onderwerp = eget(orow[0], "titel");
    }
    return "Kamerstukdossier " + d_nummer+ " " +d_toevoeging+": "+ onderwerp;
  }
  std::string d_nummer, d_toevoeging;
};

#if 0
struct GeschenkScanner : Scanner
{
  std::string getType() override
  {
    return "Geschenk";
  }

  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, const std::string& id) 
  {  
    GeschenkScanner s;
    auto row = s.getRow(sqlw, id);  
    return std::make_unique<GeschenkScanner>(s);
  }
  
  std::vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    auto hits = sqlw.queryT("select * from Geschenk where bijgewerkt >= ?", {d_nummer, d_toevoeging, d_cutoff});
    return sqlToScannerHits(hits);
  }
  
  std::string describe(SQLiteWriter& sqlw) override
  {
    return "Geschenken";
  }
};
#endif


struct ZoekScanner : Scanner
{
  std::string getType() override
  {
    return "Zoekopdracht";
  }

  static std::unique_ptr<Scanner> make(SQLiteWriter& sqlw, const std::string& id) 
  {  
    ZoekScanner s;
    auto row = s.getRow(sqlw, id);  
    s.d_query = eget(row, "query");
    s.d_categorie = eget(row, "categorie");
    return std::make_unique<ZoekScanner>(s);
  }
  
  std::vector<ScannerHit> get(SQLiteWriter& sqlw) override
  {
    struct Local
    {
      Local() : idx("tkindex.sqlite3", SQLWFlag::ReadOnly)
      {
	idx.query("ATTACH DATABASE 'tk.sqlite3' as meta");
      }
      SQLiteWriter idx;
      
    };
    static Local l;

    auto matches = l.idx.queryT("SELECT uuid, snippet(docsearch,-1, '<b>', '</b>', '...', 20) as snip,  category FROM docsearch WHERE docsearch match ? and (datum >= ? or datum='') and (? or category=?)", {d_query, d_cutoff, d_categorie.empty(), d_categorie});

    //    cout<<"d_categorie: '"<<d_categorie<<"'\n";
    //    std::cout<<"Query: "<<d_query<<"\n";;
    //    std::cout<<"Got "<<matches.size()<<" matches\n";
    std::vector<ScannerHit> ret;
    for(auto& m : matches) {
      auto doc =  l.idx.queryT("select onderwerp, bijgewerkt, titel, nummer, datum FROM meta.Document where id=?",   {eget(m, "uuid")});
      if(doc.empty())
	continue;
      
      for(const auto& f : doc[0]) {
	m[f.first]=f.second;
      }
      ret.emplace_back(eget(m, "nummer"),
		       eget(m, "datum"),
		       "Document");

    }

    for(auto& m : matches) {
      if(m.count("nummer"))
	continue;
      if(eget(m,"category") != "Activiteit")
	continue;
      auto act =  l.idx.queryT("select nummer, datum FROM meta.Activiteit where id=?",   {eget(m, "uuid")});
      if(act.empty())
	continue;
      ret.emplace_back(eget(act[0], "nummer"),
		       eget(act[0], "datum"),
		       "Activiteit");
    }
    
    return ret;
  }
  
  std::string describe(SQLiteWriter& sqlw) override
  {
    std::string ret = "Zoekopdracht " + d_query;
    if(!d_categorie.empty())
      ret += " (categorie "+d_categorie+")";
    return ret;
  }
  std::string d_query;
  std::string d_categorie;
};

extern std::map<std::string, decltype(&ZaakScanner::make)> g_scanmakers;
