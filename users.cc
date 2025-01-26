#include "inja.hpp"
#include "thingpool.hh"
#include "sws.hh"
#include "scanmon.hh"
#include "pugixml.hpp"
#include "search.hh"
#include <fmt/chrono.h>
using namespace std;

static auto prepRSS(auto& doc, const std::string& title, const std::string& desc)
{
  doc.append_attribute("standalone") = "yes";
  doc.append_attribute("version") = "1.0";
  
  doc.append_attribute("encoding") = "utf-8";
  pugi::xml_node rss = doc.append_child("rss");
  rss.append_attribute("version")="2.0";
  rss.append_attribute("xmlns:atom")="http://www.w3.org/2005/Atom";
  
  pugi::xml_node channel = rss.append_child("channel");
  channel.append_child("title").append_child(pugi::node_pcdata).set_value(title.c_str());
  channel.append_child("description").append_child(pugi::node_pcdata).set_value(desc.c_str());
  channel.append_child("link").append_child(pugi::node_pcdata).set_value("https://berthub.eu/tkconv/");
  channel.append_child("generator").append_child(pugi::node_pcdata).set_value("OpenTK");
  return channel;
}
  

void addTkUserManagement(SimpleWebSystem& sws, const std::string& mailserver,
			 const std::string& fromaddr,
			 const std::string& baseUrl)
{  
  sws.wrapPost({}, "/create-user-invite", [mailserver, baseUrl, fromaddr](auto& cr) {
    string email = cr.req.get_file_value("email").content;
    nlohmann::json j;
    
    if(email.empty()) {
      j["ok"]=0;
      j["message"] = "User field empty";
    }
    else {
      try {
	auto rows = cr.lsqw.query("select * from users where email=?", {email});
	if(!rows.empty()) {
	  string user=get<string>(rows[0]["user"]);
	  cr.sessions.cleanExpired(); // needs to run somewhere
	  
	  string session = cr.sessions.createSessionForUser(user, "Passwordless login session", cr.getIP(), true, time(0)+ 14 * 86400); // authenticated session

	  string dest=baseUrl + "/mijn.html?session="+session;
	  sendEmail(mailserver, fromaddr, email, "[OpenTK] log-in link",
		    "Log in door op deze link te klikken: "+dest+"\nDeze link werkt maar *14* dagen!",
		    fmt::format("Log in door op deze link <a href='{}'>{}</a> te klikken. Let op, deze link werkt maar *14* dagen!",
				dest, dest));
	  cout<<"Sent email pointing user "<<email<<" at "<<dest<<endl;
	  cr.stats.sessionJoinInvite++;
	  cr.log({{"action", "invite-for-rejoin"}, {"email", email}, {"for", user}, {"session", session}});
	  j["ok"] = 1;
	  return j;
	}
      }
      catch(std::exception& e) {
	cerr<<"Could fail if there is no users table yet, is ok: "<<e.what()<<endl;
      }
      string id = getLargeId();      
      time_t tstamp = time(0);
      cr.lsqw.addValue({{"id", id}, {"timestamp", tstamp}, {"email", email}}, "userInvite");
      cr.lsqw.query("delete from userInvite where email=? and timestamp < ?", {email, tstamp});

      sendEmail(mailserver, fromaddr, email, "[OpenTK] Koppel je email-adres",
		fmt::format("Welkom!\nKlik nu op {}/mijn.html?id={} om je email-adres te koppelen aan OpenTK.", baseUrl, id),
		fmt::format("Welkom!\nKlik nu op <a href='{}/mijn.html?id={}'>{}/mijn.html?id={}</a> om je email-adres te koppelen aan OpenTK.",
			    baseUrl, id,
			    baseUrl, id),
		fromaddr
		);
      cr.log({{"action", "initial-invite"}, {"email", email}, {"session", id}});
      j["ok"] = 1;
    }
    return j;
  });

  sws.wrapPost({}, "/confirm-user-invite", [](auto& cr) {
    string id = cr.req.get_file_value("id").content;
    nlohmann::json j;
      
    if(id.empty()) {
      j["ok"]=0;
      j["message"] = "id field empty";
      return j;
    }

    auto rows = cr.lsqw.query("select * from userInvite where id=?", {id});
    if(rows.empty()) {
      j["ok"]=0;
      j["message"] = "id field empty";
      return j;
    }
    string email = get<string>(rows[0]["email"]);
    string user = getLargeId();
    cr.users.createUser(user, "", email, false);

    cr.lsqw.query("delete from userInvite where email=?", {email});
    
    string ip=cr.getIP(), agent= cr.req.get_header_value("User-Agent");
    string sessionid = cr.sessions.createSessionForUser(user, agent, ip);
    cr.res.set_header("Set-Cookie",
		      "tkconv_session="+sessionid+"; SameSite=Strict; Path=/; HttpOnly; " + cr.sws.d_extraCookieSpec +" Max-Age="+to_string(5*365*86400));

    cout<<"Logged in user '"<<user<<"' with email '"<<email<<"'"<<endl;
    cr.log({{"action", "confirm-invite"}, {"for", user}, {"session", sessionid}});
    j["ok"] = 1;
    j["email"] = email;
    return j;
  });


  sws.wrapPost({Capability::IsUser}, "/add-person-monitor", [](auto& cr) {
    string nummer = cr.req.get_file_value("nummer").content;
    string id = getLargeId();
    cr.lsqw.addValue({{"id", id}, {"userid", cr.user}, {"soort", "persoon"}, {"nummer", nummer}, {"cutoff", getTodayDBFormat()}}, "scanners");
    
    return nlohmann::json{{"ok", 1}, {"id", id}};
  });

  sws.wrapPost({Capability::IsUser}, "/add-commissie-monitor", [](auto& cr) {
    string nummer = cr.req.get_file_value("nummer").content;
    string id = getLargeId();
    cr.lsqw.addValue({{"id", id}, {"userid", cr.user}, {"soort", "commissie"}, {"commissieId", nummer}, {"cutoff", getTodayDBFormat()}}, "scanners");
    return nlohmann::json{{"ok", 1}, {"id", id}};
  });

  sws.wrapPost({Capability::IsUser}, "/add-activiteit-monitor", [](auto& cr) {
    string nummer = cr.req.get_file_value("nummer").content;
    string id = getLargeId();
    cr.lsqw.addValue({{"id", id}, {"userid", cr.user}, {"soort", "activiteit"}, {"nummer", nummer}, {"cutoff", getTodayDBFormat()}}, "scanners");
    return nlohmann::json{{"ok", 1}, {"id", id}};
  });
  sws.wrapPost({Capability::IsUser}, "/add-zaak-monitor", [](auto& cr) {
    string nummer = cr.req.get_file_value("nummer").content;
    string id = getLargeId();
    cr.lsqw.addValue({{"id", id}, {"userid", cr.user}, {"soort", "zaak"}, {"nummer", nummer}, {"cutoff", getTodayDBFormat()}}, "scanners");
    return nlohmann::json{{"ok", 1}, {"id", id}};
  });

  sws.wrapPost({Capability::IsUser}, "/add-search-monitor", [](auto& cr) {
    string query = cr.req.get_file_value("query").content;
    query = convertToSQLiteFTS5(query); // this was unfortunate
    string categorie = cr.req.get_file_value("categorie").content;
    string id = getLargeId();
    cr.lsqw.addValue({{"id", id}, {"userid", cr.user}, {"soort", "zoek"}, {"categorie", ""}, {"query", query}, {"cutoff", getTodayDBFormat()}}, "scanners");
    return nlohmann::json{{"ok", 1}, {"id", id}};
  });

  
  sws.wrapPost({Capability::IsUser}, "/add-ksd-monitor", [](auto& cr) {
    string nummer = cr.req.get_file_value("nummer").content;
    string toevoeging = cr.req.get_file_value("toevoeging").content;
    string id = getLargeId();
    cr.lsqw.addValue({{"id", id}, {"userid", cr.user}, {"soort", "ksd"}, {"nummer", nummer}, {"toevoeging", toevoeging}, {"cutoff", getTodayDBFormat()}}, "scanners");
    return nlohmann::json{{"ok", 1}, {"id", id}};    
  });

  sws.wrapPost({Capability::IsUser}, "/remove-monitor", [](auto& cr) {
    string id = cr.req.get_file_value("id").content;
    cr.lsqw.query("delete from scanners where id=? and userid=?", {id, cr.user});
    nlohmann::json j;
    j["ok"]=1;
    return j;
  });


  sws.wrapGet({Capability::IsUser}, "/have-monitor/:kind/:nummer", [](auto& cr) {
    string kind = cr.req.path_params.at("kind");
    string nummer = cr.req.path_params.at("nummer");
    string selector;
    std::vector<std::unordered_map<std::string,MiniSQLite::outvar_t>> rows;
    if(kind == "persoon" || kind =="activiteit" || kind=="zaak") 
      selector = "nummer";
    else if(kind == "commissie") {
      selector = "commissieId";
    }
    else if(kind == "ksd") {
      string toevoeging = cr.req.get_param_value("toevoeging");
      rows = cr.lsqw.query("select id from scanners where soort=? and nummer =? and toevoeging =? and userid=?", {kind, nummer, toevoeging, cr.user});
    }
    
    if(!selector.empty()) {
      rows=cr.lsqw.query("select id from scanners where soort=? and "+selector+" =? and userid=?", {kind, nummer, cr.user});
    }
    nlohmann::json j;
    j["ok"]=1;
    j["have"] = rows.size();
    if(!rows.empty())
      j["id"] = get<string>(rows[0]["id"]);
    return j;
  });
  
  sws.wrapGet({Capability::IsUser}, "/my-monitors", [](auto& cr) {
    auto rows=cr.lsqw.query("select *,(select count(1) from sentNotification where scannerId = scanners.id) as cnt from scanners where userid=? order by rowid desc", {cr.user});
    struct ScannerCombo
    {
      unique_ptr<Scanner> ptr;
      int64_t count;
    };
    vector<pair<string, ScannerCombo> >scanners;
    std::lock_guard<std::mutex> l(cr.lsqw.sqwlock);
    for(auto& ts: rows) {
      if(auto iter = g_scanmakers.find(eget(ts,"soort")); iter != g_scanmakers.end()) {
	ScannerCombo sc;
	sc.ptr = iter->second(cr.lsqw.sqw, get<string>(ts["id"]));
	sc.count = get<int64_t>(ts["cnt"]);
	scanners.emplace_back(get<string>(ts["id"]), std::move(sc));

      } // XXXX this is wrong we don't have an autolocking sqlwriter not to use it
    }
    
    nlohmann::json j;
    j["ok"]=1;
    nlohmann::json jmonitors=nlohmann::json::array();
    for(auto& [id, sc] : scanners) {
      auto& [ptr, count] = sc;
      nlohmann::json jmon;
      jmon["description"] = ptr->describe(cr.tp.getLease().get());
      jmon["type"] = ptr->getType();
      jmon["id"] = id;
      jmon["cnt"] = count;
      jmonitors.push_back(jmon);
    }
    j["monitors"] = jmonitors;
    return j;
  });

  /*
    <?xml version="1.0" encoding="utf-8" standalone="yes"?>
<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">
  <channel>
    <title>Bert Hubert&#39;s writings</title>
    <link>https://berthub.eu/articles/</link>
    <description>Recent content on Bert Hubert&#39;s writings</description>
    <generator>Hugo -- gohugo.io</generator>
    <lastBuildDate>Fri, 13 Dec 2024 14:13:41 +0000</lastBuildDate><atom:link href="https://berthub.eu/articles/index.xml" rel="self" type="application/rss+xml" />
    <item>
      <title>Welkom bij OpenTK (deel 2): de monitors</title>
      <link>https://berthub.eu/articles/posts/welkom-bij-opentk-deel-2/</link>
      <pubDate>Fri, 13 Dec 2024 14:13:41 +0000</pubDate>
      
      <guid>https://berthub.eu/articles/posts/welkom-bij-opentk-deel-2/</guid>
      <description>Welkom!
Goed inzicht in ons parlement is belangrijk, soms omdat er dingen in het nieuws zijn. En soms juist omdat dingen (nog) niet in het nieuws zijn, maar er binnenkort wel besluiten over genomen gaan worden. De Tweede Kamer publiceert alles wat ze doen via een technische API, en dat is echt geweldig. Hierdoor kunnen we op Internet eigen viewers maken om deze data zo goed mogelijk zichtbaar te maken.</description>
    </item>
  */
  

  sws.wrapGet({}, "/index.xml", [](auto& cr) {
    pugi::xml_document doc;

    string dlim = fmt::format("{:%Y-%m-%d}", fmt::localtime(time(0) - 8*86400));
    bool onlyRegeringsstukken=false;
    auto rows = cr.tp.getLease()->queryT("select Document.datum datum, Document.nummer nummer, Document.onderwerp onderwerp, Document.titel titel, Document.soort soort, Document.bijgewerkt bijgewerkt, ZaakActor.naam naam, ZaakActor.afkorting afkorting from Document left join Link on link.van = document.id left join zaak on zaak.id = link.naar left join  ZaakActor on ZaakActor.zaakId = zaak.id and relatie = 'Voortouwcommissie' where +bronDocument='' and Document.soort != 'Sprekerslijst' and datum > ? and (? or Document.soort in ('Brief regering', 'Antwoord schriftelijke vragen', 'Voorstel van wet', 'Memorie van toelichting', 'Antwoord schriftelijke vragen (nader)')) order by datum desc, bijgewerkt desc",
						  {dlim, !onlyRegeringsstukken});

    time_t latest = time(0);
    if(!rows.empty()) {
      string maxbw;
      for(const auto& r: rows) {
	if(maxbw < eget(r, "bijgewerkt"))
	  maxbw = eget(r, "bijgewerkt");
      }
      latest = getTstamp(maxbw);
    }
    string date = fmt::format("{:%a, %d %b %Y %H:%M:%S %z}", fmt::localtime(latest));
    pugi::xml_node channel = prepRSS(doc, "Hoofd OpenTK feed", "Meest recente kamerdocumenten");
    channel.append_child("lastBuildDate").append_child(pugi::node_pcdata).set_value(date.c_str());

    for(const auto& r : rows) {
      pugi::xml_node item = channel.append_child("item");
      string onderwerp = eget(r, "onderwerp");
      item.append_child("title").append_child(pugi::node_pcdata).set_value(onderwerp.c_str());

      onderwerp = eget(r, "naam")+" | " + eget(r, "titel") + " " + onderwerp;
      
      item.append_child("description").append_child(pugi::node_pcdata).set_value(onderwerp.c_str());

      
      item.append_child("link").append_child(pugi::node_pcdata).set_value(
									  fmt::format("https://berthub.eu/tkconv/document.html?nummer={}", eget(r,"nummer")).c_str());
      item.append_child("guid").append_child(pugi::node_pcdata).set_value(("tkconv_"+eget(r, "nummer")).c_str());



      // 2024-12-06T06:01:10.2530000
      string pubDate = eget(r, "bijgewerkt");
      time_t then = getTstamp(pubDate);
     
      //      <pubDate>Fri, 13 Dec 2024 14:13:41 +0000</pubDate>
      date = fmt::format("{:%a, %d %b %Y %H:%M:%S %z}", fmt::localtime(then));
      item.append_child("pubDate").append_child(pugi::node_pcdata).set_value(date.c_str());

      
    }
    ostringstream str;
    doc.save(str);
    return make_pair<string,string>(str.str(), "application/xml");
  });

  // https://berthub.eu/tkconv/search.html?q=bert+hubert&twomonths=false&soorten=alles
  sws.wrapGet({}, "/search/index.xml", [](auto& cr) {
    string q = convertToSQLiteFTS5(cr.req.get_param_value("q"));
    string categorie;

    SQLiteWriter own("tkindex-small.sqlite3", SQLWFlag::ReadOnly);
    own.query("ATTACH database 'tk.sqlite3' as meta");
    SearchHelper sh(own);

    // for now we can't do the rest, only Document XXX
    auto matches = sh.search(q, {"Document"});
    cout<<"Have "<<matches.size()<<" matches\n";
    pugi::xml_document doc;
    pugi::xml_node channel = prepRSS(doc, "Zoek RSS naar " +q, "Documenten gematched door zoekstring "+q);
    
    bool first = true;
    
    
    for(auto& m : matches) {
      auto docs = own.queryT("select Document.onderwerp, Document.titel titel, Document.nummer nummer, Document.bijgewerkt bijgewerkt, ZaakActor.naam naam, ZaakActor.afkorting afkorting from Document left join Link on link.van = document.id left join zaak on zaak.id = link.naar left join  ZaakActor on ZaakActor.zaakId = zaak.id and relatie = 'Voortouwcommissie'  where Document.nummer=?", {m.nummer});

      if(docs.empty()) {
	cout<<"No hits for "<< m.nummer<<endl;
	continue;
      }
      auto& r = docs[0];
      pugi::xml_node item = channel.append_child("item");
      string onderwerp = eget(r, "onderwerp");
      item.append_child("title").append_child(pugi::node_pcdata).set_value(onderwerp.c_str());
      onderwerp = eget(r, "naam")+" | " + eget(r, "titel") + " " + onderwerp;
      item.append_child("description").append_child(pugi::node_pcdata).set_value(onderwerp.c_str());

      
      item.append_child("link").append_child(pugi::node_pcdata).set_value(
									  fmt::format("https://berthub.eu/tkconv/document.html?nummer={}", eget(r,"nummer")).c_str());
	item.append_child("guid").append_child(pugi::node_pcdata).set_value(("tkconv_"+eget(r, "nummer")).c_str());

      // 2024-12-06T06:01:10.2530000
      string pubDate = eget(r, "bijgewerkt");
      time_t then = getTstamp(pubDate);
     
      //      <pubDate>Fri, 13 Dec 2024 14:13:41 +0000</pubDate>
      string date = fmt::format("{:%a, %d %b %Y %H:%M:%S %z}", fmt::localtime(then));
      item.append_child("pubDate").append_child(pugi::node_pcdata).set_value(date.c_str());

      if(first) {
	channel.prepend_child("lastBuildDate").append_child(pugi::node_pcdata).set_value(date.c_str());
	first=false;
      }
      
    }

    if(first) {
      string date = fmt::format("{:%a, %d %b %Y %H:%M:%S %z}", fmt::localtime(time(0)));
      channel.append_child("pubDate").append_child(pugi::node_pcdata).set_value(date.c_str());
    }
    
    ostringstream str;
    doc.save(str);
    return make_pair<string,string>(str.str(), "application/xml");
  });
  
  sws.wrapGet({}, "/:timsi/index.xml", [](auto& cr) {
    string timsi = cr.req.path_params.at("timsi");
    cout<<"Called for timsi "<< timsi <<endl;

    auto docids = cr.lsqw.query("select identifier,timestamp from sentNotification,users where sentNotification.userid = users.user and users.timsi=? order by timestamp desc", {timsi});

    cout<<"Got "<<docids.size()<<" docids\n";
    
    pugi::xml_document doc;
    pugi::xml_node channel = prepRSS(doc, "Monitor RSS", "Documenten gematched door jouw monitors");
    
    bool first = true;


    for(const auto& di : docids) {
      pugi::xml_node item = channel.append_child("item");

      auto docs = cr.tp.getLease()->queryT("select Document.onderwerp, Document.titel titel, Document.nummer nummer, Document.bijgewerkt bijgewerkt, ZaakActor.naam naam, ZaakActor.afkorting afkorting from Document left join Link on link.van = document.id left join zaak on zaak.id = link.naar left join  ZaakActor on ZaakActor.zaakId = zaak.id and relatie = 'Voortouwcommissie'  where Document.nummer=?", {eget(di, "identifier")});
      if(docs.empty())
	continue;
      auto& r = docs[0];
      
      string onderwerp = eget(r, "onderwerp");
      item.append_child("title").append_child(pugi::node_pcdata).set_value(onderwerp.c_str());

      onderwerp = eget(r, "naam")+" | " + eget(r, "titel") + " " + onderwerp;
      
      item.append_child("description").append_child(pugi::node_pcdata).set_value(onderwerp.c_str());

      
      item.append_child("link").append_child(pugi::node_pcdata).set_value(
									  fmt::format("https://berthub.eu/tkconv/document.html?nummer={}", eget(r,"nummer")).c_str());
      item.append_child("guid").append_child(pugi::node_pcdata).set_value(("tkconv_"+eget(r, "nummer")).c_str());

      // 2024-12-06T06:01:10.2530000
      string pubDate = eget(r, "bijgewerkt");
      time_t then = getTstamp(pubDate);
     
      //      <pubDate>Fri, 13 Dec 2024 14:13:41 +0000</pubDate>
      string date = fmt::format("{:%a, %d %b %Y %H:%M:%S %z}", fmt::localtime(then));
      item.append_child("pubDate").append_child(pugi::node_pcdata).set_value(date.c_str());

      if(first) {
	channel.prepend_child("lastBuildDate").append_child(pugi::node_pcdata).set_value(date.c_str());
	first=false;
      }
      
    }

    if(first) {
      string date = fmt::format("{:%a, %d %b %Y %H:%M:%S %z}", fmt::localtime(time(0)));
      channel.append_child("pubDate").append_child(pugi::node_pcdata).set_value(date.c_str());
    }
    
    ostringstream str;
    doc.save(str);
    return make_pair<string,string>(str.str(), "application/xml");
  });

  sws.wrapGet({}, "/commissie/:commissieid/index.xml", [](auto& cr) {
    string commissieid = cr.req.path_params.at("commissieid");
    cout<<"Called for commissieid "<<commissieid<<endl;
    string dlim = fmt::format("{:%Y-%m-%d}", fmt::localtime(time(0) - 100*86400));
    auto docs = cr.tp.getLease()->queryT("select Document.*, ZaakActor.relatie from Zaak,ZaakActor,Document,Link where ZaakActor.zaakId = zaak.id and zaakactor.commissieId=? and Document.id = link.van and Zaak.id = link.naar and ZaakActor.relatie='Voortouwcommissie' and Document.datum > ? order by Document.datum desc", {commissieid, dlim});

    cout<<"Got "<<docs.size()<<" docids\n";

    auto comm = cr.tp.getLease()->queryT("select naam from commissie where id=?", {commissieid});
    if(comm.empty())
      throw runtime_error("Commissie " + commissieid + " bestaat niet");
    string naam = eget(comm[0], "naam");
    
    pugi::xml_document doc;
    pugi::xml_node channel = prepRSS(doc, "OpenTK: "+ naam + " RSS", "Documenten voor " + naam);
    
    bool first = true;

    for(const auto& r : docs) {
      pugi::xml_node item = channel.append_child("item");
      
      string onderwerp = eget(r, "onderwerp");
      item.append_child("title").append_child(pugi::node_pcdata).set_value(onderwerp.c_str());

      onderwerp = eget(r, "naam")+" | " + eget(r, "titel") + " " + onderwerp;
      
      item.append_child("description").append_child(pugi::node_pcdata).set_value(onderwerp.c_str());
      item.append_child("link").append_child(pugi::node_pcdata).set_value(
									  fmt::format("https://berthub.eu/tkconv/document.html?nummer={}", eget(r,"nummer")).c_str());
      item.append_child("guid").append_child(pugi::node_pcdata).set_value(("tkconv_"+eget(r, "nummer")).c_str());
      // 2024-12-06T06:01:10.2530000
      string pubDate = eget(r, "bijgewerkt");
      time_t then = getTstamp(pubDate);
     
      //      <pubDate>Fri, 13 Dec 2024 14:13:41 +0000</pubDate>
      string date = fmt::format("{:%a, %d %b %Y %H:%M:%S %z}", fmt::localtime(then));
      item.append_child("pubDate").append_child(pugi::node_pcdata).set_value(date.c_str());

      if(first) {
	channel.prepend_child("lastBuildDate").append_child(pugi::node_pcdata).set_value(date.c_str());
	first=false;
      }
    }

    if(first) {
      string date = fmt::format("{:%a, %d %b %Y %H:%M:%S %z}", fmt::localtime(time(0)));
      channel.append_child("pubDate").append_child(pugi::node_pcdata).set_value(date.c_str());
    }
    
    ostringstream str;
    doc.save(str);
    return make_pair<string,string>(str.str(), "application/xml");
  });

  
}  
