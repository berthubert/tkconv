#include <fmt/format.h>
#include <fmt/printf.h>
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
  string fname = makePathForId(id);
  string command;

  if(isDocx(fname))
    command = fmt::format("pandoc -s -f docx -t html '{}'",
			  fname);
  else if(isDoc(fname))
    command = fmt::format("echo '<pre>' ; catdoc < '{}'; echo '</pre>'",
			  fname);
  else
    command = fmt::format("pdftohtml -s {} -dataurls -stdout",fname);

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


int main(int argc, char** argv)
{
  SQLiteWriter unlockedsqlw("tk.sqlite3");
  std::mutex sqwlock;
  LockedSqw sqlw{unlockedsqlw, sqwlock};

  httplib::Server svr;

  svr.Get("/getdoc/:nummer", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.path_params.at("nummer"); // 2023D41173
    cout<<"nummer: "<<nummer<<endl;

    auto ret=sqlw.query("select * from Document where nummer=? order by rowid desc limit 1", {nummer});
    if(ret.empty()) {
      res.set_content("Found nothing!!", "text/plain");
      return;
    }
    string id = get<string>(ret[0]["id"]);
    fmt::print("'{}'\n", id);
    string content = getHtmlForDocument(id);
    res.set_content(content, "text/html");
  });

  // hoe je de fractienaam krijgt bij een persoonId:
  //  select f.afkorting from FractieZetelPersoon fzp,FractieZetel fz,Fractie f, Persoon p where fzp.fractieZetelId = fz.id and fzp.persoonId = p.id and p.id = ?
  
  svr.Get("/getraw/:nummer", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.path_params.at("nummer"); // 2023D41173
    cout<<"nummer: "<<nummer<<endl;

    auto ret=sqlw.query("select * from Document where nummer=? order by rowid desc limit 1", {nummer});
    if(ret.empty()) {
      res.set_content("Found nothing!!", "text/plain");
      return;
    }
    string id = get<string>(ret[0]["id"]);
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

  
  svr.Get("/ksd/:nummer", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    int nummer=atoi(req.path_params.at("nummer").c_str()); // 36228
    auto docs = sqlw.query("select document.nummer docnummer,* from Document,Kamerstukdossier where kamerstukdossier.nummer=? and Document.kamerstukdossierid = kamerstukdossier.id order by volgnummer desc", {nummer});
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} docs for kamerstuk {}\n", docs.size(), nummer);
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

      auto zactors = sqlw.query("select * from ZaakActor where zaakId=?", {zaakId});
      if(!zactors.empty()) {
	content+="<p>Zaak-gerelateerde data: <ul>";
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
    
    auto docs = sqlw.query("select kamerstukdossier.nummer, max(document.datum) mdatum,kamerstukdossier.titel,hoogsteVolgnummer from kamerstukdossier,document where document.kamerstukdossierid=kamerstukdossier.id and document.datum > '2023-09-09' group by kamerstukdossier.id order by 2 desc");
    // XXX hardcoded date
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} kamerstukdossiers\n", docs.size());
  });



  
  
  svr.Get("/recent-docs", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    
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

  
  svr.Get("/unplanned-activities", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    auto docs = sqlw.query("select * from Activiteit where datum='' order by updated desc"); 
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} unplanned activities\n", docs.size());
  });


  svr.Get("/future-besluiten", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    auto docs = sqlw.query("select activiteit.datum, activiteit.nummer anummer, zaak.nummer znummer, agendapuntZaakBesluitVolgorde volg, besluit.status,agendapunt.onderwerp aonderwerp, zaak.onderwerp zonderwerp, naam indiener from besluit,agendapunt,activiteit,zaak left join zaakactor on zaakactor.zaakid = zaak.id and relatie='Indiener' where besluit.agendapuntid = agendapunt.id and activiteit.id = agendapunt.activiteitid and zaak.id = besluit.zaakid and datum > '2024-09-10' and datum < '2024-09-17' order by datum asc,agendapuntZaakBesluitVolgorde asc"); // XX hardcoded date
    
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} besluiten\n", docs.size());
  });


  
  svr.Get("/future-activities", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    auto docs = sqlw.query("select Activiteit.datum datum, activiteit.bijgewerkt bijgewerkt, activiteit.nummer nummer, naam, noot, onderwerp,voortouwAfkorting from Activiteit left join Reservering on reservering.activiteitId=activiteit.id  left join Zaal on zaal.id=reservering.zaalId where datum > '2024-09-10' order by datum asc"); // XX hardcoded date

    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} activities\n", docs.size());
  });


  svr.Post("/search", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string term = req.get_file_value("q").content;
    string twomonths = req.get_file_value("twomonths").content;
    string limit = "";
    if(twomonths=="true")
      limit = "2024-07-10";

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
    auto matches = idx.queryT("SELECT uuid,meta.Document.onderwerp, meta.Document.bijgewerkt, meta.Document.titel, nummer, datum, snippet(docsearch,-1, '<b>', '</b>', '...', 20) as snip FROM docsearch,meta.Document WHERE docsearch MATCH ? and docsearch.uuid = Document.id and datum > ? order by bm25(docsearch) limit 80", {term, limit});
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

  svr.set_mount_point("/", "./html/");
  int port = 8089;
  if(argc > 1)
    port = atoi(argv[1]);
  svr.listen("0.0.0.0", port);
}
