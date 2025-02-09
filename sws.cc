#include "sws.hh"
#include <map>
#include "bcrypt.h"
#include "support.hh"
#include <sclasses.hh>
using namespace std;

// turn "abcd=1234; defgh=6934" into a map
static unordered_map<string,string> getGen(const std::string& cookiestr, const string& sep)
{
  unordered_map<string,string> ret;
  if(cookiestr.size() > 2000) // this regex could take quite some time otherwise, thanks Wander Nauta
    return ret;
  std::regex cookie_regex("([^=]*=[^"+sep.substr(0,1)+"]*)");
  auto cookies_begin =
    std::sregex_iterator(cookiestr.begin(), cookiestr.end(), cookie_regex);
  auto cookies_end = std::sregex_iterator();

  for(auto iter = cookies_begin; iter != cookies_end; ++iter) {
    std::regex inner("("+sep+")?([^=]*)=([^"+sep.substr(0,1)+"]*)");
    std::smatch m;
    string s = iter->str();
    std::regex_search(s, m, inner);
    if(m.size() != 4)
      continue;
    ret[m[2].str()]=m[3].str();
  }

  return ret;
}

unordered_map<string,string> getCookies(const std::string& cookiestr)
{
  return getGen(cookiestr, "; ");
}

// teases the session cookie from the headers
string getSessionID(const httplib::Request &req) 
{
  auto cookies = getCookies(req.get_header_value("Cookie"));
  auto siter = cookies.find("tkconv_session");
  if(siter == cookies.end()) {
    throw std::runtime_error("No session cookie");
  }
  return siter->second;
}

std::string SimpleWebSystem::getIP(const httplib::Request& req) const
{
  if(d_tproxies.count(req.remote_addr) && req.has_header(d_realipheadername)) {
    string hdr = req.get_header_value(d_realipheadername);
    if(hdr.find("::ffff:")==0)
      hdr = hdr.substr(7);
    return hdr;
  }
  return req.remote_addr;
}

bool Users::userHasCap(const std::string& user, const Capability& cap, const httplib::Request* req)
{
  bool ret=false;
  if(cap== Capability::IsUser) {
    auto c = d_lsqw.query("select count(1) as c from users where user=? and disabled=0", {user});
    ret = (c.size()==1 && get<int64_t>(c[0]["c"])==1);
  }
  else if(cap==Capability::Admin) {
    auto c = d_lsqw.query("select count(1) as c from users where user=? and disabled=0 and admin=1", {user});
    ret = (c.size()==1 && get<int64_t>(c[0]["c"])==1);
  } else if(cap==Capability::EmailAuthenticated && req) {
    auto c = d_lsqw.query("select count(1) as c from sessions,users where users.user=? and users.user=sessions.user and disabled=0 and authenticated=1 and id=?", {user, getSessionID(*req)});
    ret = (c.size()==1 && get<int64_t>(c[0]["c"])==1);
  }
  return ret;
}

string Users::getEmail(const std::string& user)
{
  auto res = d_lsqw.query("select email from users where user=?", {user});
  if(res.size() == 1)
    return get<string>(res[0]["email"]);
  return "";
}

void Users::setEmail(const std::string& user, const std::string& email)
{
  d_lsqw.query("update users set email=? where user=?", {email, user});
}

bool Users::hasPassword(const std::string& user)
{
  auto res = d_lsqw.query("select pwhash from users where user=?", {user});
  return res.size() == 1 && !get<string>(res[0]["pwhash"]).empty();
}

bool Users::isUserDisabled(const std::string& user)
{
  auto res = d_lsqw.query("select disabled from users where user=?", {user});
  return res.size() == 0 || (get<int64_t>(res.at(0).at("disabled")) == 1);
}

bool Users::checkPassword(const std::string& user, const std::string& password) const
{
  if(password.empty()) // due to a bug, some users may have an empty password
    return false;
  auto res = d_lsqw.query("select pwhash, caps from users where user=? and disabled=0", {user});
  if(res.empty()) {
    return false;
  }
  return bcrypt::validatePassword(password, get<string>(res[0]["pwhash"]));
}

void Users::createUser(const std::string& user, const std::string& password, const std::string& email, bool admin)
{
  // if you specify an empty password, you don't get a password
  string pwhash = password.empty() ? "" : bcrypt::generateHash(password);
  d_lsqw.addValue({{"user", user}, {"pwhash", pwhash}, {"admin", (int)admin},
		   {"timsi", getLargeId()},
		   {"disabled", 0}, {"caps", ""}, {"lastLoginTstamp", 0}, {"email", email}}, "users");
}

void Users::delUser(const std::string& user)
{
  d_lsqw.query("delete from users where user=?", {user});
}

// empty disables password
void Users::changePassword(const std::string& user, const std::string& password)
{
  string pwhash = password.empty() ? "" : bcrypt::generateHash(password);
  auto res = d_lsqw.query("select user from users where user=?", {user});
  if(res.size()!=1 || get<string>(res[0]["user"]) != user) {
    d_lsqw.addValue({{"action", "change-password-failure"}, {"user", user}, {"ip", "xx missing xx"}, {"meta", "no such user"}, {"tstamp", time(0)}}, "log");
    throw std::runtime_error("Tried to change password for user '"+user+"', but does not exist");
  }
  d_lsqw.query("update users set pwhash=? where user=?", {pwhash, user});
}


string Sessions::getUserForSession(const std::string& sessionid, const std::string& agent, const std::string& ip) const
{
  try {
    auto ret = d_lsqw.query("select * from sessions where id=?", {sessionid});
    if(ret.size()==1) {
      time_t expire = std::get<int64_t>(ret[0]["expireTstamp"]);
      if(expire && expire < time(0)) {
        cout<<"Authenticated session expired"<<endl;
        d_lsqw.query("delete from sessions where id=?", {sessionid});
        return "";
      }
      
      d_lsqw.query("update sessions set lastUseTstamp=?, agent=?, ip=? where id=?", {time(0), agent, ip, sessionid});
      return get<string>(ret[0]["user"]);
    }
  }
  catch(std::exception&e ){ cout<<"Error: "<<e.what()<<endl;}
  return "";
}

string Sessions::getUser(const httplib::Request &req, const std::string& ip)  const
{
  string agent= req.get_header_value("User-Agent");
  return getUserForSession(getSessionID(req), agent, ip);
}

string Sessions::createSessionForUser(const std::string& user, const std::string& agent, const std::string& ip, bool authenticated, std::optional<time_t> expire)
{
  string sessionid=getLargeId();
  d_lsqw.addValue({{"id", sessionid}, {"user", user}, {"agent", agent}, {"ip", ip}, {"createTstamp", time(0)},
                   {"lastUseTstamp", 0}, {"expireTstamp", expire.value_or(0)},
                   {"authenticated", (int)authenticated}}, "sessions");
  return sessionid;
}

void Sessions::dropSession(const std::string& sessionid, std::optional<string> user)
{
  if(!user)
    d_lsqw.query("delete from sessions where id=?", {sessionid});
  else
    d_lsqw.query("delete from sessions where id=? and user=?", {sessionid, *user});
}

void Sessions::cleanExpired() 
{
  d_lsqw.query("delete from sessions where expireTstamp < ? and expireTstamp != 0 and expireTstamp not null", {(int64_t)time(0)});
}


void SimpleWebSystem::setExtraCookieSpec(const std::string& spec)
{
  d_extraCookieSpec = spec;
  if(!d_extraCookieSpec.empty() && *d_extraCookieSpec.rbegin()!=';')
    d_extraCookieSpec.append(1, ';');
}

void SimpleWebSystem::setTrustedProxies(const std::vector<std::string>& ips, const std::string& realipheadername)
{
  d_realipheadername = realipheadername;
  for(const auto& i : ips) {
    ComboAddress ca(i);
    d_tproxies.insert(ca.toString());
  }
}

void SimpleWebSystem::standardFunctions()
{
  wrapGet({}, "/status", [](auto &cr) {
    nlohmann::json j{{"ok", 1}};
    j["login"] = !cr.user.empty();
    j["admin"] = false;
    //    j["version"] = GIT_VERSION;
    if(!cr.user.empty()) {
      j["user"] = cr.user;
      j["admin"] = cr.users.userHasCap(cr.user, Capability::Admin);
      j["email"] = cr.users.getEmail(cr.user);
      j["hasPw"] = cr.users.hasPassword(cr.user);
    }
    return j;
  });

  wrapPost({}, "/login", [](auto &cr) {
    string user = cr.req.get_file_value("user").content;
    string password = cr.req.get_file_value("password").content;
    nlohmann::json j{{"ok", 0}};
    if(cr.users.checkPassword(user, password)) {
      string ip=cr.getIP(), agent= cr.req.get_header_value("User-Agent");
      string sessionid = cr.sessions.createSessionForUser(user, agent, ip);
      cr.res.set_header("Set-Cookie",
                     "tkconv_session="+sessionid+"; SameSite=Strict; Path=/; HttpOnly; " + cr.sws.d_extraCookieSpec +" Max-Age="+to_string(5*365*86400));
      cout<<"Logged in user '"<<user<<"'"<<endl;
      j["ok"]=1;
      j["message"]="welcome!";

      cr.log({{"action", "login"}, {"for", user}});
      cr.lsqw.query("update users set lastLoginTstamp=? where user=?", {time(0), user});
    }
    else {
      cout<<"Wrong user or password for user " << user <<endl;
      j["message"]="Wrong user or password";
      cr.log({{"action", "failed-login"}, {"for", user}});
    }
    return j;
  });
  /*
  wrapPost({Capability::IsUser}, "/change-my-password/?", [](auto& cr) {
    auto origpwfield = cr.req.get_file_value("password0");
    auto pwfield = cr.req.get_file_value("password1");
    if(pwfield.content.empty())
      throw std::runtime_error("Can't set an empty password");
    if(cr.users.hasPassword(cr.user) && !cr.users.checkPassword(cr.user, origpwfield.content)) {
      throw std::runtime_error("Original password not correct");
    }
    cout<<"Attemping to set password for user "<<cr.user<<endl;
    cr.users.changePassword(cr.user, pwfield.content);
    cr.log({{"action", "change-my-password"}});

    return nlohmann::json{{"ok", 1}, {"message", "Changed password"}};
  });
  */
  wrapPost({}, "/join-session/(.*)", [](auto& cr) {
    string sessionid = cr.req.matches[1];
    nlohmann::json j{{"ok", 0}};

    auto c = cr.lsqw.query("select sessions.user, sessions.id, email from sessions,users where users.user = sessions.user and sessions.id=? and authenticated=1 and expireTstamp > ?", {sessionid, time(0)});
    if(c.size()==1) {
      string user= get<string>(c[0]["user"]);

      // because fucking microsoft attempts to click on the login link, we can no longer delete
      // the login session.....

      
      // delete this temporary session
      //cr.sessions.dropSession(sessionid, user);

      
      // emailauthenticated session so it can reset your password, but no expiration
      string newsessionid = cr.sessions.createSessionForUser(user, "synth", cr.getIP(), true);
      cr.res.set_header("Set-Cookie",
                     "tkconv_session="+newsessionid+"; SameSite=Strict; Path=/; HttpOnly; " + cr.sws.d_extraCookieSpec +"Max-Age="+to_string(5*365*86400));
      cr.lsqw.query("update users set lastLoginTstamp=? where user=?", {time(0), user});
      cr.log({{"action", "join-session"}, {"fromsessionid", sessionid}, {"sessionid", newsessionid}, {"user", user}});
      j["ok"]=1;
      j["email"] = eget(c[0], "email");
      cr.stats.successfulSessionJoin++;
    }
    else {
      cr.stats.failedSessionJoin++;
      cout<<"Could not find authenticated session "<<sessionid<<endl;
    }
    return j;
  });
  /*
  wrapPost({Capability::IsUser}, "/change-my-email/?", [](auto& cr) {
    auto email = cr.req.get_file_value("email").content;

    auto pwfield = cr.req.get_file_value("password");
    if(cr.users.hasPassword(cr.user) && !cr.users.checkPassword(cr.user, pwfield.content)) {
      throw std::runtime_error("Password not correct");
    }
    
    cr.users.setEmail(cr.user, email);
    cr.log({{"action", "change-my-email"}, {"to", email}});
    return nlohmann::json{{"ok", 1}, {"message", "Changed email"}};
  });
  */

  wrapGet({Capability::IsUser}, "/my-sessions", [](auto& cr) {
    return cr.lsqw.queryJRet("select * from sessions where user = ?", {cr.user});
  });
  
  wrapPost({Capability::IsUser}, "/kill-my-session/([^/]+)", [](auto& cr) {
    string session = cr.req.matches[1];
    cr.sessions.dropSession(session, cr.user);
    cr.log({{"action", "kill-my-session"}});
    return nlohmann::json{{"ok", 1}};
  });
  
  wrapPost({Capability::IsUser}, "/logout", [](auto& cr)  {
    cr.log({{"action", "logout"}});
    try {
      cr.sessions.dropSession(getSessionID(cr.req));
      cr.log({{"action", "logout"}});
    }
    catch(std::exception& e) {
      fmt::print("Failed to drop session from the database, perhaps there was none\n");
    }
    cr.res.set_header("Set-Cookie",
                      "tkconv_session="+getSessionID(cr.req)+"; SameSite=Strict; Path=/; Max-Age=0");
    return nlohmann::json{{"ok", 1}};
  });

    
  wrapGet({Capability::Admin}, "/all-users", [](auto& cr) {
    return cr.lsqw.queryJRet("select user, email, disabled, lastLoginTstamp, admin from users");
  });
    
  wrapGet({Capability::Admin}, "/all-sessions", [](auto& cr) {
    return cr.lsqw.queryJRet("select * from sessions");
  });

  wrapPost({Capability::Admin}, "/create-user", [](auto& cr) {
    string password1 = cr.req.get_file_value("password1").content;
    string user = cr.req.get_file_value("user").content;
    string email = cr.req.get_file_value("email").content;
    nlohmann::json j;
      
    if(user.empty()) {
      j["ok"]=0;
      j["message"] = "User field empty";
    }
    else {
      cr.users.createUser(user, password1, email, false);
      cr.log({{"action", "create-user"}, {"who", user}, {"email", email}});
        
      j["ok"] = 1;
    }
    return j;
  });
    
  wrapPost({Capability::Admin}, "/change-user-disabled/([^/]+)/([01])", [](auto& cr) {
    string user = cr.req.matches[1];
    bool disabled = stoi(cr.req.matches[2]);
    cr.lsqw.query("update users set disabled = ? where user=?", {disabled, user});
    if(disabled) {
      cr.lsqw.query("delete from sessions where user=?", {user}); // XX candidate for Sessions class
    } // "to" is used in another log action as a string
    cr.log({{"action", "change-user-disabled"}, {"who", user}, {"to", to_string(disabled)}});
    return nlohmann::json{{"ok", 1}};
  });
    
  wrapPost({Capability::Admin}, "/change-password/?", [](auto& cr) {
    auto pwfield = cr.req.get_file_value("password");
      
    string user = cr.req.get_file_value("user").content;
    cout<<"Attemping to set password for user "<<user<<endl;
    cr.users.changePassword(user, pwfield.content);
    cr.log({{"action", "change-password"}});
    return nlohmann::json{{"ok", 1}};
  });

  wrapPost({Capability::Admin}, "/change-email/?", [](auto& cr) {
    auto email = cr.req.get_file_value("email").content;
    auto user = cr.req.get_file_value("user").content;
    cr.users.setEmail(user, email);
    cr.log({{"action", "change-email"}, {"to", email}});
    return nlohmann::json{{"ok", 1}, {"message", "Changed email"}};
  });

  
  wrapPost({Capability::Admin}, "/kill-session/([^/]+)", [](auto& cr) {
    string session = cr.req.matches[1];
    cr.sessions.dropSession(session);
    cr.log({{"action", "kill-session"}});
    return nlohmann::json{{"ok", 1}};
  });
    
  wrapPost({Capability::Admin}, "/del-user/([^/]+)", [](auto& cr) {
    string user = cr.req.matches[1];
    cr.users.delUser(user);
      
    // XX logging is weird, 'user' should likely be called 'subject' here
    cr.log({{"action", "del-user"}, {"who", user}});
    return nlohmann::json{{"ok", 1}};
  });

  /*
  // hard to unit test this
  wrapPost({Capability::IsUser, Capability::EmailAuthenticated}, "/wipe-my-password/?", [](auto& cr) {
    cr.users.changePassword(cr.user, "");
    cr.log({{"action", "wipe-my-password"}});
    return nlohmann::json{{"ok", 1}};
  });
  */
  wrapPost({Capability::Admin}, "/stop" , [this](auto& cr) {
    cr.log({{"action", "stop"}});
    d_svr.stop();
    return nlohmann::json{{"ok", 1}};
  });
}

SimpleWebSystem::SimpleWebSystem(ThingPool<SQLiteWriter>& tp, LockedSqw& lsqw) :d_tp(tp),  d_lsqw(lsqw), d_users(lsqw), d_sessions(lsqw)
{
  d_svr.set_exception_handler([](const auto& req, auto& res, std::exception_ptr ep) {
    string reason;
    try {
      std::rethrow_exception(ep);
    } catch (std::exception &e) {
      reason = fmt::format("An error occurred: {}", e.what());
    } catch (...) {
      reason = "An unknown error occurred";
    }
    cout<<req.path<<": exception for "<<reason<<endl;
    string json="Failed to serialize error message";
    try {
      nlohmann::json j{{"ok", 0}, {"message", reason}, {"reason", reason}};
      json = j.dump();
    }catch(...) {}

    res.set_content(json, "application/json");
    res.status = 200;
  });
}
