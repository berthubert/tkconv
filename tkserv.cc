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
  else
    command = fmt::format("pdftohtml -s {} -dataurls -stdout",fname);

  shared_ptr<FILE> fp(popen(command.c_str(), "r"), pclose);
  if(!fp)
    throw runtime_error("Unable to perform pdftotext: "+string(strerror(errno)));
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
  SQLiteWriter sqlw("tk.sqlite3");
  httplib::Server svr;

  svr.Get("/getdoc/:nummer", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.path_params.at("nummer"); // 2023D41173
    cout<<"nummer: "<<nummer<<endl;

    auto ret=sqlw.queryT("select * from Document where nummer=? order by rowid desc limit 1", {nummer});
    if(ret.empty()) {
      res.set_content("Found nothing!!", "text/plain");
      return;
    }
    string id = get<string>(ret[0]["id"]);
    fmt::print("'{}'\n", id);
    string content = getHtmlForDocument(id);
    res.set_content(content, "text/html");
  });
  
  svr.Get("/get/:nummer", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.path_params.at("nummer"); // 2023D41173
        cout<<"nummer: "<<nummer<<endl;

    auto ret=sqlw.queryT("select * from Document where nummer=? order by rowid desc limit 1", {nummer});
    if(ret.empty()) {
      res.set_content("Found nothing!!", "text/plain");
      return;
    }

    string kamerstukdossierId;
    int kamerstuknummer=0, kamerstukvolgnummer=0;
    string kamerstuktitel;
    try {
      kamerstukdossierId = get<string>(ret[0]["kamerstukdossierId"]);
      auto kamerstuk = sqlw.queryT("select * from kamerstukdossier where id=? order by rowid desc limit 1", {kamerstukdossierId});
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

    
    string content = R"(<html><meta charset="utf-8">)";

    content += fmt::format(R"(<meta property='og:title' content='{}'>
<meta property='og:description' content='{}'>
<meta property='og:type' content='article'><meta property='og:image' content='{}'>)",
			   htmlEscape(get<string>(ret[0]["onderwerp"])),
			   htmlEscape(get<string>(ret[0]["onderwerp"])),
			   "https://www.tweedekamer.nl/themes/contrib/tk_theme/assets/favicon/favicon.svg"
			   );
    
    content += fmt::format(R"(<body><h1>{}</h1><h2>{}</h2>
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
      auto actors = sqlw.queryT("select * from DocumentActor where documentId=?", {documentId});
    if(!actors.empty()) {
      content += "Gerelateerde personen: <ul>";
      for(auto& a: actors)
	content += "<li>" + get<string>(a["relatie"]) +", "+get<string>(a["naam"]) +", " + get<string>(a["functie"]) +"</li>";
      content += "</ul>";
    }
    }catch(exception& e) { cout << e.what() << endl;}

    if(kamerstuknummer> 0) {
      content += fmt::format("<p>Deel van kamerstukdossier '{}', {}-{}</p>",
			     kamerstuktitel, kamerstuknummer, kamerstukvolgnummer);
			     
    }
    
    if(!bronDocument.empty()) {
      auto brondocument = sqlw.queryT("select * from document where id=? order by rowid desc limit 1", {bronDocument});
      if(!brondocument.empty())
	content += "<p>Dit document is een bijlage bij <a href='"+get<string>(brondocument[0]["nummer"])+"'> " + get<string>(brondocument[0]["nummer"])+ "</a> '"+get<string>(brondocument[0]["onderwerp"])+"'</p>";
    }

    auto bijlagen = sqlw.queryT("select * from document where bronDocument=?", {documentId});
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
    auto zlinks = sqlw.queryT("select distinct(naar) as naar from Link where van=? and category='Document' and linkSoort='Zaak'", {documentId});
    set<string> actids;
    for(auto& zlink : zlinks) {
      string zaakId = get<string>(zlink["naar"]);

      auto zactors = sqlw.queryT("select * from ZaakActor where zaakId=?", {zaakId});
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
	auto reldocs = sqlw.queryT("select * from Document,Link where Link.naar=? and link.van=Document.id", {zaakId});
	for(auto& rd : reldocs) {
	  if(get<string>(rd["id"]) != documentId)
	    content+= "<li>"+get<string>(rd["soort"])+ " <a href='"+get<string>(rd["nummer"])+"'>" +get<string>(rd["onderwerp"]) +" (" + get<string>(rd["nummer"])+")</a></li>";
	}
	content+="</ul></p>";
      }
    
      auto besluiten = sqlw.queryT("select * from besluit where zaakid=? and verwijderd = 0 order by rowid", {zaakId});
      set<string> agendapuntids;
      for(auto& b: besluiten) {
	agendapuntids.insert(get<string>(b["agendapuntId"]));
      }
      
      for(auto& agendapuntid : agendapuntids) {
	auto agendapunten = sqlw.queryT("select * from Agendapunt where id = ?", {agendapuntid});
	for(auto& agendapunt: agendapunten)
	  actids.insert(get<string>(agendapunt["activiteitId"]));
      }
    }
    if(!actids.empty()) {
      content += "<p></p>Onderdeel van de volgende activiteiten:\n<ul>";
      for(auto& actid : actids) {
	auto activiteit = sqlw.queryT("select * from Activiteit where id = ? order by rowid desc limit 1", {actid});
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
    content += "<iframe width='95%'  height='1024' src='../getdoc/"+nummer+"'></iframe>";
    
    content += "</body></html>";
    res.set_content(content, "text/html");
  });

  svr.Get("/recent-docs", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    auto docs = sqlw.queryT("select * from Document where bronDocument='' order by datum desc limit 80");
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} docs\n", docs.size());
  });
  
  svr.Get("/search/:term", [&sqlw](const httplib::Request &req, httplib::Response &res) {
    string term="\""+req.path_params.at("term")+"\"";
    SQLiteWriter idx("tkindex.sqlite3");
    idx.query("ATTACH DATABASE 'tk.sqlite3' as meta");
    cout<<"Search: '"<<term<<"'\n";
    auto matches = idx.queryT("SELECT uuid,meta.Document.onderwerp, meta.Document.bijgewerkt, meta.Document.titel, nummer, datum, snippet(docsearch,-1, '<b>', '</b>', '...', 20) as snip FROM docsearch,meta.Document WHERE tekst MATCH ? and docsearch.uuid = Document.id order by bm25(docsearch) limit 40", {term});
    fmt::print("Got {} matches\n", matches.size());
    res.set_content(packResultsJsonStr(matches), "application/json");
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
    res.set_content(buf, "text/html");
    res.status = 500; 
  });

  svr.set_mount_point("/", "./html/");
  svr.listen("0.0.0.0", 8089);
}
