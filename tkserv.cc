#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/os.h>
#include <fmt/ranges.h>
#include <iostream>
#include "httplib.h"
#include "sqlwriter.hh"
#include "jsonhelper.hh"
#include "support.hh"
#include "pugixml.hpp"

using namespace std;
static void replaceSubstring(std::string &originalString, const std::string &searchString, const std::string &replaceString) {
  size_t pos = originalString.find(searchString);
  
  while (pos != std::string::npos) {
    originalString.replace(pos, searchString.length(), replaceString);
    pos = originalString.find(searchString, pos + replaceString.length());
  }
}

static string htmlEscape(const std::string& str)
{
  vector<pair<string,string>> rep{{"&", "&amp;"}, {"<", "&lt;"}, {">", "&gt;"}, {"\"", "&quot;"}, {"'", "&#39;"}};
  string ret=str;
  for(auto&& [from,to] : rep)
    replaceSubstring(ret, from, to);
  return ret;
}

static string getHtmlForDocument(const std::string& id)
{
  if(isPresentNonEmpty(id, "doccache", ".html")) {
    string fname = makePathForId(id, "doccache", ".html");
    FILE* pfp = fopen(fname.c_str(), "r");
    if(!pfp)
      throw runtime_error("Unable to get cached document "+id+": "+string(strerror(errno)));
    
    shared_ptr<FILE> fp(pfp, fclose);
    char buffer[4096];
    string ret;
    for(;;) {
      int len = fread(buffer, 1, sizeof(buffer), fp.get());
      if(!len)
	break;
      ret.append(buffer, len);
    }
    if(!ferror(fp.get())) {
      fmt::print("Had a cache hit for {} HTML\n", id);
      return ret;
    }
    // otherwise fall back to normal process
  }
  
  string fname = makePathForId(id);
  string command;

  if(isDocx(fname))
    command = fmt::format("pandoc -s -f docx   --embed-resources  --variable maxwidth=72em -t html '{}'",
			  fname);
  else if(isDoc(fname))
    command = fmt::format("echo '<pre>' ; catdoc < '{}'; echo '</pre>'",
			  fname);
  else if(isXML(fname))
    command = fmt::format("xmlstarlet tr tk.xslt < '{}'",
			  fname);

  else
    command = fmt::format("pdftohtml -s {} -dataurls -stdout",fname);

  fmt::print("Command: {} {} \n", command, isXML(fname));
  FILE* pfp = popen(command.c_str(), "r");
  if(!pfp)
    throw runtime_error("Unable to perform conversion for '"+command+"': "+string(strerror(errno)));
  
  shared_ptr<FILE> fp(pfp, pclose);
  char buffer[4096];
  string ret;
  for(;;) {
    int len = fread(buffer, 1, sizeof(buffer), fp.get());
    if(!len)
      break;
    ret.append(buffer, len);
  }
  if(ferror(fp.get()))
    throw runtime_error("Unable to perform pdftotext: "+string(strerror(errno)));

  string rsuffix ="."+to_string(getRandom64());
  string oname = makePathForId(id, "doccache", "", true);
  {
    auto out = fmt::output_file(oname+rsuffix);
    out.print("{}", ret);
  }
  if(rename((oname+rsuffix).c_str(), (oname+".html").c_str()) < 0) {
    unlink((oname+rsuffix).c_str());
    fmt::print("Rename of cached HTML failed\n");
  }

  
  return ret;
}

static string getPDFForDocx(const std::string& id)
{
  if(isPresentNonEmpty(id, "doccache", ".pdf")) {
    string fname = makePathForId(id, "doccache", ".pdf");
    FILE* pfp = fopen(fname.c_str(), "r");
    if(!pfp)
      throw runtime_error("Unable to get cached document "+id+": "+string(strerror(errno)));
    
    shared_ptr<FILE> fp(pfp, fclose);
    char buffer[4096];
    string ret;
    for(;;) {
      int len = fread(buffer, 1, sizeof(buffer), fp.get());
      if(!len)
	break;
      ret.append(buffer, len);
    }
    if(!ferror(fp.get())) {
      fmt::print("Had a cache hit for {} PDF\n", id);
      return ret;
    }
    // otherwise fall back to normal process
  }
  // 
  string fname = makePathForId(id);
  string command = fmt::format("pandoc -s --metadata \"margin-left:1cm\" --metadata \"margin-right:1cm\" -V fontfamily=\"dejavu\"  --variable mainfont=\"DejaVu Serif\" --variable sansfont=Arial --pdf-engine=xelatex -f docx -t pdf '{}'",
			  fname);
  FILE* pfp = popen(command.c_str(), "r");
  if(!pfp)
    throw runtime_error("Unable to perform conversion for '"+command+"': "+string(strerror(errno)));
  
  shared_ptr<FILE> fp(pfp, pclose);
  char buffer[4096];
  string ret;
  for(;;) {
    int len = fread(buffer, 1, sizeof(buffer), fp.get());
    if(!len)
      break;
    ret.append(buffer, len);
  }
  if(ferror(fp.get()))
    throw runtime_error("Unable to perform pandoc: "+string(strerror(errno)));

  string rsuffix ="."+to_string(getRandom64());
  string oname = makePathForId(id, "doccache", "", true);
  {
    auto out = fmt::output_file(oname+rsuffix);
    out.print("{}", ret);
  }
  if(rename((oname+rsuffix).c_str(), (oname+".pdf").c_str()) < 0) {
    unlink((oname+rsuffix).c_str());
    fmt::print("Rename of cached PDF failed\n");
  }
  
  return ret;
}



static string getRawDocument(const std::string& id)
{
  string fname = makePathForId(id);
  FILE* pfp = fopen(fname.c_str(), "r");
  if(!pfp)
    throw runtime_error("Unable to get raw document "+id+": "+string(strerror(errno)));

  shared_ptr<FILE> fp(pfp, fclose);
  char buffer[4096];
  string ret;
  for(;;) {
    int len = fread(buffer, 1, sizeof(buffer), fp.get());
    if(!len)
      break;
    ret.append(buffer, len);
  }
  if(ferror(fp.get()))
    throw runtime_error("Unable to perform pdftotext: "+string(strerror(errno)));
  return ret;
}

struct VoteResult
{
  set<string> voorpartij, tegenpartij, nietdeelgenomenpartij;
  int voorstemmen=0, tegenstemmen=0, nietdeelgenomen=0;
  
};
bool getVoteDetail(LockedSqw& sqlw, const std::string& besluitId, VoteResult& vr)
{
  auto votes = sqlw.query("select * from Stemming where besluitId=?", {besluitId});
  if(votes.empty())
    return false;
  cout<<"Got "<<votes.size()<<" votes\n";
  if(!get<string>(votes[0]["persoonId"]).empty()) {
    fmt::print("Hoofdelijke stemming, lukt niet\n");
    return false; // we kunnen nog niks met hoofdelijke stemmingen
  }
  
  for(auto& v : votes) {
    string soort = get<string>(v["soort"]);
    string partij = get<string>(v["actorFractie"]);
    int zetels = get<int64_t>(v["fractieGrootte"]);
    if(soort == "Voor") {
      vr.voorstemmen += zetels;
      vr.voorpartij.insert(partij);
    }
    else if(soort == "Tegen") {
      vr.tegenstemmen += zetels;
      vr.tegenpartij.insert(partij);
    }
    else if(soort=="Niet deelgenomen") {
      vr.nietdeelgenomen+= zetels;
      vr.nietdeelgenomenpartij.insert(partij);
    }
  }
  return true;
}

int main(int argc, char** argv)
{
  SQLiteWriter unlockedsqlw("tk.sqlite3");
  std::mutex sqwlock;
  LockedSqw sqlw{unlockedsqlw, sqwlock};
  signal(SIGPIPE, SIG_IGN); // every TCP application needs this
  httplib::Server svr;

  svr.Get("/getdoc/:nummer", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.path_params.at("nummer"); // 2023D41173
    cout<<"nummer: "<<nummer<<endl;

    string id, contentType;
    if(nummer.length()==36 && nummer.find_first_not_of("0123456789abcdef-") == string::npos) {
      id = nummer;
      auto ret = sqlw.query("select contentType from Document where id=?", {id});
      if(!ret.empty()) {
	contentType = get<string>(ret[0]["contentType"]);
      }
      else {
	ret = sqlw.query("select contentType from Verslag where id=?", {id});
	if(ret.empty()) {
	  fmt::print("Kon {} niet vinden, niet als document, niet als verslag\n", id);
	  res.set_content("Found nothing!!", "text/plain");
	  res.status=404;
	  return;
	}
	contentType = get<string>(ret[0]["contentType"]);
      }
    }
    else {
      auto ret=sqlw.query("select * from Document where nummer=? order by rowid desc limit 1", {nummer});
      if(ret.empty()) {
	res.set_content("Found nothing!!", "text/plain");
	res.status=404;
	return;
      }
      id = get<string>(ret[0]["id"]);
      contentType = get<string>(ret[0]["contentType"]);
      fmt::print("'{}' {}\n", id, contentType);
    }
    // docx to pdf is better for embedded images it appears
    // XXX disabled
    if(0 && contentType == "application/vnd.openxmlformats-officedocument.wordprocessingml.document") {
      string content = getPDFForDocx(id);
      res.set_content(content, "application/pdf");
    }
    else {
      string content = getHtmlForDocument(id);
      res.set_content(content, "text/html; charset=utf-8");
    }
  });

  // hoe je de fractienaam krijgt bij een persoonId:
  //  select f.afkorting from FractieZetelPersoon fzp,FractieZetel fz,Fractie f, Persoon p where fzp.fractieZetelId = fz.id and fzp.persoonId = p.id and p.id = ?
  
  svr.Get("/getraw/:nummer", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.path_params.at("nummer"); // 2023D41173
    cout<<"nummer: "<<nummer<<endl;
    string id;
    auto ret=sqlw.query("select * from Document where nummer=? order by rowid desc limit 1", {nummer});

    if(ret.empty()) {
      ret = sqlw.query("select * from Verslag where id=? order by rowid desc limit 1", {nummer});
      if(ret.empty()) {
	res.set_content("Found nothing!!", "text/plain");
	return;
      }
      id=nummer;
    }
    else
      id = get<string>(ret[0]["id"]);
    fmt::print("'{}'\n", id);
    string content = getRawDocument(id);
    res.set_content(content, get<string>(ret[0]["contentType"]));
  });


  svr.Get("/zaak/:nummer", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string nummer = req.path_params.at("nummer");
    nlohmann::json z = nlohmann::json::object();
    auto zaken = sqlw.query("select * from zaak where nummer=?", {nummer});
    if(zaken.empty()) {
      res.status = 404;
      return;
    }
    z["zaak"] = packResultsJson(zaken)[0];
    string zaakid = z["zaak"]["id"];
    cout<<"Id: '"<<zaakid<<"'\n";
    z["actors"] = sqlw.queryJRet("select * from zaakactor where zaakId=?", {zaakid});

    // Multi: {"activiteit", "agendapunt", "gerelateerdVanuit", "vervangenVanuit"}
    // Hasref: {"activiteit", "agendapunt", "gerelateerdVanuit", "kamerstukdossier", "vervangenVanuit"}


    z["activiteiten"] = sqlw.queryJRet("select * from Activiteit,Link where Link.van=? and Activiteit.id=link.naar", {zaakid});
    z["agendapunten"] = sqlw.queryJRet("select * from Agendapunt,Link where Link.van=? and Agendapunt.id=link.naar", {zaakid});
    for(auto &d : z["agendapunten"]) {
      d["activiteit"] = sqlw.queryJRet("select * from Activiteit where id=?", {(string)d["activiteitId"]})[0];
    }

    z["gerelateerd"] = sqlw.queryJRet("select * from Zaak,Link where Link.van=? and Zaak.id=link.naar and linkSoort='gerelateerdVanuit'", {zaakid});
    z["vervangenVanuit"] = sqlw.queryJRet("select * from Zaak,Link where Link.van=? and Zaak.id=link.naar and linkSoort='vervangenVanuit'", {zaakid});
    z["vervangenDoor"] = sqlw.queryJRet("select * from Zaak,Link where Link.naar=? and Zaak.id=link.van and linkSoort='vervangenVanuit'", {zaakid});
    
    z["docs"] = sqlw.queryJRet("select * from Document,Link where Link.naar=? and Document.id=link.van order by datum desc", {zaakid});
    for(auto &d : z["docs"]) {
      cout<< d["id"] << endl;
      string docid = d["id"];
      d["actors"]=sqlw.queryJRet("select * from DocumentActor where documentId=?", {docid});
    }

    z["kamerstukdossier"]=sqlw.queryJRet("select * from kamerstukdossier where id=?",
					 {(string)z["zaak"]["kamerstukdossierId"]});
    
    // XXX kamerstuk
    // XXX agendapunt multi
    
    res.set_content(z.dump(), "application/json");
  });    


  svr.Get("/persoonplus/:id", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string id = req.path_params.at("id"); // 9e79de98-e914-4dc8-8dc7-6d7cb09b93d7
    cout<<"Lookup for "<<id<<endl;
    auto persoon = sqlw.query("select * from Persoon where id=?", {id});

    if(persoon.empty()) {
      res.status = 404;
      return;
    }

    nlohmann::json j = nlohmann::json::object();
    j["persoon"] = packResultsJson(persoon)[0];

    j["docs"] = sqlw.queryJRet("select datum,nummer,soort,onderwerp from Document,DocumentActor where relatie like '%ondertekenaar%' and DocumentActor.DocumentId = Document.id and persoonId=? order by 1 desc", {id});

    j["moties"] = sqlw.queryJRet("select document.nummer, besluit.id besluitid, zaak.id zaakid, zaak.nummer zaaknummer, Document.id, Document.datum,document.soort,document.onderwerp,document.titel,besluit.soort,besluit.tekst,besluit.opmerking from Document,DocumentActor,Link,Zaak,besluit where Document.soort like 'Motie%' and DocumentId=Document.id and relatie like '%ondertekenaar' and persoonid=? and link.van = document.id and link.linkSoort='Zaak' and zaak.id=link.naar and besluit.zaakid=zaak.id order by datum desc", {id});
    
    for(auto& m : j["moties"]) {
      VoteResult vr;
      if(!getVoteDetail(sqlw, m["besluitid"], vr))
	continue;
      m["voorpartij"] = vr.voorpartij;
      m["tegenpartij"] = vr.tegenpartij;
      m["voorstemmen"] = vr.voorstemmen;
      m["tegenstemmen"] = vr.tegenstemmen;
      m["nietdeelgenomenstemmen"] = vr.nietdeelgenomen;
      m["aangenomen"] = vr.voorstemmen > vr.tegenstemmen;
    }
    // hier kan een mooie URL van gebakken worden
    //  https://www.tweedekamer.nl/kamerstukken/moties/detail?id=2024Z10238&did=2024D24219
    
    res.set_content(j.dump(), "application/json");
  });

  
  svr.Get("/ksd/:nummer/:toevoeging", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    int nummer=atoi(req.path_params.at("nummer").c_str()); // 36228
    string toevoeging=req.path_params.at("toevoeging").c_str();
    auto docs = sqlw.query("select document.nummer docnummer,* from Document,Kamerstukdossier where kamerstukdossier.nummer=? and kamerstukdossier.toevoeging=? and Document.kamerstukdossierid = kamerstukdossier.id order by volgnummer desc", {nummer, toevoeging});
    nlohmann::json r = nlohmann::json::object();
    r["documents"] = packResultsJson(docs);

    auto meta = sqlw.query("select * from kamerstukdossier where nummer=? and toevoeging=?",
			   {nummer, toevoeging});

    if(!meta.empty())
      r["meta"] = packResultsJson(meta)[0];
    
    res.set_content(r.dump(), "application/json");
    fmt::print("Returned {} docs for kamerstuk {} toevoeging {}\n", docs.size(), nummer, toevoeging);
  });

  
  svr.Get("/get/:nummer", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.path_params.at("nummer"); // 2023D41173
        cout<<"nummer: "<<nummer<<endl;

    auto ret=sqlw.query("select * from Document where nummer=? order by rowid desc limit 1", {nummer});
    if(ret.empty()) {
      res.set_content("Found nothing!!", "text/plain");
      return;
    }

    string kamerstukdossierId;
    int kamerstuknummer=0, kamerstukvolgnummer=0;
    string kamerstuktitel;
    try {
      kamerstukdossierId = get<string>(ret[0]["kamerstukdossierId"]);
      auto kamerstuk = sqlw.query("select * from kamerstukdossier where id=? order by rowid desc limit 1", {kamerstukdossierId});
      if(!kamerstuk.empty()) {
	kamerstuknummer = get<int64_t>(kamerstuk[0]["nummer"]);
	kamerstuktitel = get<string>(kamerstuk[0]["titel"]);
	kamerstukvolgnummer = get<int64_t>(ret[0]["volgnummer"]);
      }
    }
    catch(exception& e) {
      fmt::print("Could not get kamerstukdetails: {}\n", e.what());
    }
    
    string bronDocument = std::get<string>(ret[0]["bronDocument"]);
    string soort = std::get<string>(ret[0]["soort"]);
    string titel;
    try {
      if(ret[0].count("titel"))
	titel = std::get<string>(ret[0]["titel"]);
    }
    catch(...) {}
    
    string dir="kamervragen";
    if(soort=="Brief regering")
      dir = "brieven_regering";
    
    string url = fmt::format("https://www.tweedekamer.nl/kamerstukken/{}/detail?id={}&did={}", 	     dir,
			     get<string>(ret[0]["nummer"]),
			     get<string>(ret[0]["nummer"]));

    
    string content = "<!doctype html>\n";
    content += R"(<html><meta charset="utf-8">   <link rel='stylesheet' href='../style.css'>)";

    content += fmt::format(R"(<meta property='og:title' content='{}'>
<meta property='og:description' content='{}'>
<meta property='og:type' content='article'><meta property='og:image' content='{}'>)",
			   htmlEscape(get<string>(ret[0]["onderwerp"])),
			   htmlEscape(get<string>(ret[0]["onderwerp"])),
			   "https://www.tweedekamer.nl/themes/contrib/tk_theme/assets/favicon/favicon.svg"
			   );
    
    content += fmt::format(R"(<body><center><small>[<a href="../search.html">zoekmachine</a>] [<a href="../">documenten</a>] [<a href="../ongeplande-activiteiten.html">ongeplande activiteiten</a>] [<a href="../activiteiten.html">activiteiten</a>] [<a href="../geschenken.html">geschenken</a>] [<a href="../toezeggingen.html">toezeggingen</a>] [<a href="../open-vragen.html">open vragen</a>] [<a href="../kamerstukdossiers.html">kamerstukdossiers</a>] [<a href="https://github.com/berthubert/tkconv?tab=readme-ov-file#tools-om-de-tweede-kamer-open-data-te-gebruiken">wat is dit?</a>]</small></center><h1>{}</h1><h2>{}</h2>
<p>Datum: <b>{}</b>, bijgewerkt: {}, updated: {}.</p>
<p>Nummer: <b>{}</b>, Soort: <b>{}</b>.</p>
<p>
<a href="{}">Directe link naar document</a>, <a href="{}">link naar pagina op de Tweede Kamer site</a>.</p>
)",
			   get<string>(ret[0]["onderwerp"]),
			   titel,
				 get<string>(ret[0]["datum"]),
				 get<string>(ret[0]["bijgewerkt"]),
				 get<string>(ret[0]["updated"]),
				 get<string>(ret[0]["nummer"]),
				 soort,
				 get<string>(ret[0]["enclosure"]),
				 url
				 
				 );

    string documentId=get<string>(ret[0]["id"]);
    try {
      auto actors = sqlw.query("select * from DocumentActor where documentId=?", {documentId});
    if(!actors.empty()) {
      content += "Gerelateerde personen: <ul>";
      for(auto& a: actors)
	content += "<li>" + get<string>(a["relatie"]) +", "+get<string>(a["naam"]) +", " + get<string>(a["functie"]) +"</li>";
      content += "</ul>";
    }
    }catch(exception& e) { cout << e.what() << endl;}

    if(kamerstuknummer> 0) {
      content += fmt::format("<p>Deel van kamerstukdossier '<a href='../ksd.html?ksd={}'>{}, {}</a>-{}</p>",
			     kamerstuknummer, kamerstuktitel, kamerstuknummer, kamerstukvolgnummer);
			     
    }
    
    if(!bronDocument.empty()) {
      auto brondocument = sqlw.query("select * from document where id=? order by rowid desc limit 1", {bronDocument});
      if(!brondocument.empty())
	content += "<p>Dit document is een bijlage bij <a href='"+get<string>(brondocument[0]["nummer"])+"'> " + get<string>(brondocument[0]["nummer"])+ "</a> '"+get<string>(brondocument[0]["onderwerp"])+"'</p>";
    }

    auto bijlagen = sqlw.query("select * from document where bronDocument=?", {documentId});
    if(!bijlagen.empty()) {
      content+="<p>Dit document heeft de volgende bijlagen: <ul>";
      for(auto& b: bijlagen) {
	auto g = [&b](const string& s) {
	  return get<string>(b[s]);
	};
	content += "<li><a href='"+g("nummer")+ "'>'" + g("onderwerp");
	content += "' ("+g("nummer")+")</a></li> ";
	
      }
      content+="</ul></p>";
    }
    auto zlinks = sqlw.query("select distinct(naar) as naar from Link where van=? and category='Document' and linkSoort='Zaak'", {documentId});
    set<string> actids;
    for(auto& zlink : zlinks) {
      string zaakId = get<string>(zlink["naar"]);

      auto zactors = sqlw.query("select * from Zaak,ZaakActor where zaak.id=?  and ZaakActor.zaakId = zaak.id", {zaakId});
      if(!zactors.empty()) {
	content+="<p>Zaak-gerelateerde data (";
	set<string> znummers;
	for(auto& z: zactors) 
	  znummers.insert(get<string>(z["nummer"]));
	for(auto iter = znummers.begin(); iter != znummers.end(); ++iter) {
	  if(iter != znummers.begin())
	    content+= ", ";
	  content += "<a href='../zaak.html?nummer=" + *iter +"'>" + *iter +"</a>";
	}
	content +="): <ul>";
	for(auto& z: zactors) {
	  auto g = [&z](const string& s) {
	    return get<string>(z[s]);
	  };
	  content += "<li>";
	  content += g("relatie") + ": " + g("naam");
	  if(!g("functie").empty())
	    content+=", " +g("functie");
	  content += "</li>";
	  
	}
	auto reldocs = sqlw.query("select * from Document,Link where Link.naar=? and link.van=Document.id", {zaakId});
	for(auto& rd : reldocs) {
	  if(get<string>(rd["id"]) != documentId)
	    content+= "<li>"+get<string>(rd["soort"])+ " <a href='"+get<string>(rd["nummer"])+"'>" +get<string>(rd["onderwerp"]) +" (" + get<string>(rd["nummer"])+")</a></li>";
	}
	content+="</ul></p>";
      }
    
      auto besluiten = sqlw.query("select * from besluit where zaakid=? and verwijderd = 0 order by rowid", {zaakId});
      set<string> agendapuntids;
      for(auto& b: besluiten) {
	agendapuntids.insert(get<string>(b["agendapuntId"]));
      }
      
      for(auto& agendapuntid : agendapuntids) {
	auto agendapunten = sqlw.query("select * from Agendapunt where id = ?", {agendapuntid});
	for(auto& agendapunt: agendapunten)
	  actids.insert(get<string>(agendapunt["activiteitId"]));
      }
    }
    if(!actids.empty()) {
      content += "<p></p>Onderdeel van de volgende activiteiten:\n<ul>";
      for(auto& actid : actids) {
	auto activiteit = sqlw.query("select * from Activiteit where id = ? order by rowid desc limit 1", {actid});
	auto g = [&activiteit](const string& s) {
	  return get<string>(activiteit[0][s]);
	};
	string datum = g("datum");
	datum[10]=' ';
	content += "<li><a href='https://www.tweedekamer.nl/debat_en_vergadering/commissievergaderingen/details?id="+g("nummer")+ "'>"+ g("soort")+" " + datum +" &rarr; '"+g("onderwerp");
	  content += "', "+g("voortouwNaam")+" ("+g("nummer")+")</a></li> ";
      }
      content += "</ul>";
    }
    //     https://gegevensmagazijn.tweedekamer.nl/SyncFeed/2.0/Resources/ceac1329-435f-4235-b5c4-410f135b74cf
    if(get<string>(ret[0]["contentType"])=="application/pdf")
      content += "<iframe width='95%'  height='1024' src='../getraw/"+nummer+"'></iframe>";
    else
      content += "<iframe width='95%'  height='1024' src='../getdoc/"+nummer+"'></iframe>";

    
    content += "</body></html>";
    res.set_content(content, "text/html");
  });


  svr.Get("/open-toezeggingen", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    
    auto docs = sqlw.query("select toezegging.id, tekst, toezegging.nummer, ministerie, status, naamToezegger,activiteit.datum, kamerbriefNakoming, datumNakoming, activiteit.nummer activiteitNummer, initialen, tussenvoegsel, achternaam, functie, fractie.afkorting as fractienaam, voortouwAfkorting from Toezegging,Activiteit left join Persoon on persoon.id = toezegging.persoonId left join Fractie on fractie.id = toezegging.fractieId where  Toezegging.activiteitId = activiteit.id and status != 'Voldaan' order by activiteit.datum desc");
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} docs\n", docs.size());
  });


  // create table openvragen as select Zaak.id, Zaak.gestartOp, zaak.nummer, min(document.nummer) as docunummer, zaak.onderwerp, count(1) filter (where Document.soort="Schriftelijke vragen") as numvragen, count(1) filter (where Document.soort like "Antwoord schriftelijke vragen%" or (Document.soort="Mededeling" and (document.onderwerp like '%ingetrokken%' or document.onderwerp like '%intrekken%'))) as numantwoorden  from Zaak, Link, Document where Zaak.id = Link.naar and Document.id = Link.van and Zaak.gestartOp > '2019-09-09' group by 1, 3 having numvragen > 0 and numantwoorden==0 order by 2 desc

  svr.Get("/open-vragen", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    
    auto docs = sqlw.query("select *, max(functie) filter (where relatie='Gericht aan') as aan, max(naam) filter (where relatie='Indiener') as indiener from openvragen,zaakactor where zaakactor.zaakid = openvragen.id group by openvragen.id order by gestartOp desc");
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} open-vragen\n", docs.size());
  });

  svr.Get("/recent-kamerstukdossiers", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    
    auto docs = sqlw.query("select kamerstukdossier.nummer, max(document.datum) mdatum,kamerstukdossier.titel,kamerstukdossier.toevoeging,hoogsteVolgnummer from kamerstukdossier,document where document.kamerstukdossierid=kamerstukdossier.id and document.datum > '2020-01-01' group by kamerstukdossier.id,toevoeging order by 2 desc");
    // XXX hardcoded date
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} kamerstukdossiers\n", docs.size());
  });



  
  
  svr.Get("/recent-docs", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    // als een document in twee zaken zit komt hij hier dubbel uit! deduppen?
    auto docs = sqlw.query("select Document.datum datum, Document.nummer nummer, Document.onderwerp onderwerp, Document.titel titel, Document.soort soort, Document.bijgewerkt bijgewerkt, ZaakActor.naam naam, ZaakActor.afkorting afkorting from Document left join Link on link.van = document.id left join zaak on zaak.id = link.naar left join  ZaakActor on ZaakActor.zaakId = zaak.id and relatie = 'Voortouwcommissie' where bronDocument='' order by datum desc limit 80");
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} docs\n", docs.size());
  });

  // select * from persoonGeschenk, Persoon where Persoon.id=persoonId order by Datum desc

  svr.Get("/geschenken", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    auto docs = sqlw.query("select datum, omschrijving, functie, initialen, tussenvoegsel, roepnaam, achternaam, gewicht from persoonGeschenk, Persoon where Persoon.id=persoonId and datum > '2019-01-01' order by Datum desc"); 
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} geschenken\n", docs.size());
  });

  /* stemmingen. Poeh. Een stemming is eigenlijk een Stem, en ze zijn allemaal gekoppeld aan een besluit.
     een besluit heeft een Zaak en een Agendapunt
     een agendapunt hoort weer bij een activiteit, en daar vinden we eindelijk een datum

     Er zijn vaak twee besluiten per motie, eentje "ingediend" en dan nog een besluit waarop
     gestemd wordt.
     
  */

  svr.Get("/stemmingen", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    auto besluiten = sqlw.query("select besluit.id as besluitid, besluit.soort as besluitsoort, besluit.tekst as besluittekst, besluit.opmerking as besluitopmerking, activiteit.datum, activiteit.nummer anummer, zaak.nummer znummer, agendapuntZaakBesluitVolgorde volg, besluit.status,agendapunt.onderwerp aonderwerp, zaak.onderwerp zonderwerp, naam indiener from besluit,agendapunt,activiteit,zaak left join zaakactor on zaakactor.zaakid = zaak.id and relatie='Indiener' where besluit.agendapuntid = agendapunt.id and activiteit.id = agendapunt.activiteitid and zaak.id = besluit.zaakid and datum < '2024-09-14' and datum > '2024-08-13' order by datum desc,agendapuntZaakBesluitVolgorde asc"); // XX hardcoded date

    nlohmann::json j = nlohmann::json::array();

    for(auto& b : besluiten) {
      cout<<"Besluit "<<get<string>(b["zonderwerp"])<<endl;
      VoteResult vr;
      if(!getVoteDetail(sqlw, get<string>(b["besluitid"]), vr))
	continue;
      
      fmt::print("{}, voor: {} ({}), tegen: {} ({}), niet deelgenomen: {} ({})\n",
		 get<string>(b["besluitid"]),
		 vr.voorpartij, vr.voorstemmen,
		 vr.tegenpartij, vr.tegenstemmen,
		 vr.nietdeelgenomenpartij, vr.nietdeelgenomen);

      decltype(besluiten) tmp{b};
      nlohmann::json jtmp = packResultsJson(tmp)[0];
      jtmp["voorpartij"] = vr.voorpartij;
      jtmp["tegenpartij"] = vr.tegenpartij;
      jtmp["voorstemmen"] = vr.voorstemmen;
      jtmp["tegenstemmen"] = vr.tegenstemmen;
      jtmp["nietdeelgenomenstemmen"] = vr.nietdeelgenomen;
      j.push_back(jtmp);
    }
    // als document er bij komt kan je deze aantrekkelijke url gebruiken
    // https://www.tweedekamer.nl/kamerstukken/moties/detail?id=2024Z10238&did=2024D24219
    
    res.set_content(j.dump(), "application/json");
    fmt::print("Returned {} besluiten\n", j.size());
  });

  
  svr.Get("/unplanned-activities", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    auto docs = sqlw.query("select * from Activiteit where datum='' order by updated desc"); 
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} unplanned activities\n", docs.size());
  });


  svr.Get("/future-besluiten", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    auto docs = sqlw.query("select activiteit.datum, activiteit.nummer anummer, zaak.nummer znummer, agendapuntZaakBesluitVolgorde volg, besluit.status,agendapunt.onderwerp aonderwerp, zaak.onderwerp zonderwerp, naam indiener from besluit,agendapunt,activiteit,zaak left join zaakactor on zaakactor.zaakid = zaak.id and relatie='Indiener' where besluit.agendapuntid = agendapunt.id and activiteit.id = agendapunt.activiteitid and zaak.id = besluit.zaakid and datum > '2024-09-13' and datum < '2024-09-19' order by datum asc,agendapuntZaakBesluitVolgorde asc"); // XX hardcoded date
    
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} besluiten\n", docs.size());
  });


  
  svr.Get("/future-activities", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    auto docs = sqlw.query("select Activiteit.datum datum, activiteit.bijgewerkt bijgewerkt, activiteit.nummer nummer, naam, noot, onderwerp,voortouwAfkorting from Activiteit left join Reservering on reservering.activiteitId=activiteit.id  left join Zaal on zaal.id=reservering.zaalId where datum > '2024-09-18' order by datum asc"); // XX hardcoded date

    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} activities\n", docs.size());
  });


  svr.Post("/search", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string term = req.get_file_value("q").content;
    string twomonths = req.get_file_value("twomonths").content;
    string limit = "";
    if(twomonths=="true")
      limit = "2024-07-11";

    // turn COVID-19 into "COVID-19" and A.W.R. Hubert into "A.W.R. Hubert"
    if(term.find_first_of(".-") != string::npos  && term.find('"')==string::npos) {
      cout<<"fixing up"<<endl;
      term = "\"" + term + "\"";
    }

    
    SQLiteWriter idx("tkindex.sqlite3");
    idx.query("ATTACH DATABASE 'tk.sqlite3' as meta");
    cout<<"Search: '"<<term<<"'\n";
    DTime dt;
    dt.start();
    auto matches = idx.queryT("SELECT uuid,meta.Document.onderwerp, meta.Document.bijgewerkt, meta.Document.titel, nummer, datum, snippet(docsearch,-1, '<b>', '</b>', '...', 20) as snip, bm25(docsearch) as score FROM docsearch,meta.Document WHERE docsearch MATCH ? and docsearch.uuid = Document.id and datum > ? order by score limit 280", {term, limit});
    auto usec = dt.lapUsec();
    
    fmt::print("Got {} matches\n", matches.size());
    nlohmann::json response=nlohmann::json::object();
    response["results"]= packResultsJson(matches);
    response["milliseconds"] = usec/1000.0;
    res.set_content(response.dump(), "application/json");
  });

  
  svr.set_exception_handler([](const auto& req, auto& res, std::exception_ptr ep) {
    auto fmt = "<h1>Error 500</h1><p>%s</p>";
    char buf[BUFSIZ];
    try {
      std::rethrow_exception(ep);
    } catch (std::exception &e) {
      snprintf(buf, sizeof(buf), fmt, e.what());
    } catch (...) { // See the following NOTE
      snprintf(buf, sizeof(buf), fmt, "Unknown Exception");
    }
    cout<<"Error: '"<<buf<<"'"<<endl;
    res.set_content(buf, "text/html");
    res.status = 500; 
  });

  string root = "./html/";
  if(argc > 2)
    root = argv[2];
  svr.set_mount_point("/", root);
  int port = 8089;
  if(argc > 1)
    port = atoi(argv[1]);
  fmt::print("Listening on port {} serving html from {}\n",
	     port, root);
  svr.listen("0.0.0.0", port);
}
