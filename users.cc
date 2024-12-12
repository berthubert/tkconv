#include "inja.hpp"
#include "thingpool.hh"
#include "sws.hh"
#include "scanmon.hh"

using namespace std;

void addTkUserManagement(SimpleWebSystem& sws)
{
  sws.wrapPost({}, "/create-user-invite", [](auto& cr) {
    string email = cr.req.get_file_value("email").content;
    nlohmann::json j;
    string baseUrl="https://berthub.eu/tkconv";
    
    if(email.empty()) {
      j["ok"]=0;
      j["message"] = "User field empty";
    }
    else {
      try {
	auto rows = cr.lsqw.query("select * from users where email=?", {email});
	if(!rows.empty()) {
	  string session = cr.sessions.createSessionForUser(get<string>(rows[0]["user"]), "Passwordless login session", cr.getIP(), true, time(0)+86400); // authenticated session
	  string dest=baseUrl + "/mijn.html?session="+session;
	sendEmail("10.0.0.2", "opentk@hubertnet.nl", email, "OpenTK log-in link",
		  "Log in door op deze link te klikken: "+dest+"\nDeze link werkt maar *1* keer!",
		  fmt::format("Log in door op deze link <a href='{}'>{}</a> te klikken. Let op, deze link werkt maar *1* keer!",
			      dest, dest));
	cout<<"Sent email pointing user at "<<dest<<endl;
	j["ok"] = 1;
	return j;
	}
      }
      catch(...) {
	cerr<<"Could fail if there is no users table yet, is ok"<<endl;
      }
      string id = getLargeId();      
      time_t tstamp = time(0);
      cr.lsqw.addValue({{"id", id}, {"timestamp", tstamp}, {"email", email}}, "userInvite");
      cr.lsqw.query("delete from userInvite where email=? and timestamp < ?", {email, tstamp});

      sendEmail("10.0.0.2", "opentk@hubertnet.nl", email, "Koppel je account",
		fmt::format("Welkom bij tkconv!\nKlik op {}/mijn.html?id={} om je account te koppelen.", baseUrl, id),
		fmt::format("Welkom bij tkconv, klik op <a href='{}/mijn.html?id={}'>{}/mijn.html?id={}</a> om je account te koppelen!",
			    baseUrl, id,
			    baseUrl, id));
		
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

  
}  
