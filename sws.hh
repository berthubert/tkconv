#pragma once

#include "thingpool.hh"
#include "httplib.h"
#include <string>
#include "sqlwriter.hh"
#include "support.hh"
#include "fmt/format.h"

struct LockedSqw
{
  SQLiteWriter& sqw;
  std::mutex& sqwlock;
  LockedSqw(SQLiteWriter& sqw_, std::mutex& sqlock_) : sqw(sqw_), sqwlock(sqlock_)
  {}
  LockedSqw(const LockedSqw&) = delete;
  
  auto query(const std::string& query, const std::initializer_list<SQLiteWriter::var_t>& values ={})
  {
    std::lock_guard<std::mutex> l(sqwlock);
    return sqw.queryT(query, values);
  }

  void queryJ(httplib::Response &res, const std::string& q, const std::initializer_list<SQLiteWriter::var_t>& values={}) 
  {
    auto result = query(q, values);
    res.set_content(packResultsJsonStr(result), "application/json");
  }

  auto queryJRet(const std::string& q, const std::initializer_list<SQLiteWriter::var_t>& values={}) 
  {
    auto result = query(q, values);
    return packResultsJson(result);
  }
  
  void addValue(const std::initializer_list<std::pair<const char*, SQLiteWriter::var_t>>& values, const std::string& table="data")
  {
    std::lock_guard<std::mutex> l(sqwlock);
    sqw.addValue(values, table);
  }
  void addValue(const std::vector<std::pair<const char*, SQLiteWriter::var_t>>& values, const std::string& table="data")
  {
    std::lock_guard<std::mutex> l(sqwlock);
    sqw.addValue(values, table);
  }
};



enum class Capability {IsUser=1, Admin=2, EmailAuthenticated=3};

struct Users
{
  Users(LockedSqw& lsqw) : d_lsqw(lsqw)
  {}
  bool checkPassword(const std::string& user, const std::string& password) const;
  void createUser(const std::string& user, const std::string& password, const std::string& email, bool admin);
  void changePassword(const std::string& user, const std::string& password);
  std::string getEmail(const std::string& user);
  void setEmail(const std::string& user, const std::string& email);
  void delUser(const std::string& user);
  bool userHasCap(const std::string& user, const Capability& cap, const httplib::Request* req=0);
  bool hasPassword(const std::string& user);
  bool isUserDisabled(const std::string& user); // user that doesn't exist is also disabled
  LockedSqw& d_lsqw;
};

class Sessions
{
public:
  Sessions(LockedSqw& lsqw) : d_lsqw(lsqw)
  {}

  std::string getUserForSession(const std::string& sessionid, const std::string& agent, const std::string& ip) const;
  std::string createSessionForUser(const std::string& user, const std::string& agent, const std::string& ip, bool authenticated=false, std::optional<time_t> expire={});

  void dropSession(const std::string& sessionid, std::optional<std::string> user={});
  void cleanExpired();
  std::string getUser(const httplib::Request &req, const std::string& ip)  const;

private:
  LockedSqw& d_lsqw;
};

struct SWSStats
{
  std::atomic<int64_t> failedSessionJoin = 0;
  std::atomic<int64_t> successfulSessionJoin = 0;
  std::atomic<int64_t> sessionJoinInvite = 0;
};

struct SimpleWebSystem
{
  explicit SimpleWebSystem(ThingPool<SQLiteWriter>& tp, LockedSqw& lsqw);
  ThingPool<SQLiteWriter>& d_tp;
  LockedSqw& d_lsqw;
  Users d_users;
  Sessions d_sessions;
  SWSStats d_stats;
  httplib::Server d_svr;
  std::unordered_set<std::string> d_tproxies;
  std::string d_realipheadername;
  std::string d_extraCookieSpec;
  std::string getIP(const httplib::Request&) const;
  struct ComboReq
  {
    ThingPool<SQLiteWriter>& tp;
    LockedSqw& lsqw;
    const httplib::Request &req;
    httplib::Response &res;
    Users& users;
    Sessions& sessions;
    SWSStats& stats;
    const SimpleWebSystem& sws;
    std::string user;
    std::string getIP()
    {
      return sws.getIP(req);
    }
    void log(const std::initializer_list<std::pair<const char*, SQLiteWriter::var_t>>& fields)
    {
      // add agent?
      std::vector<std::pair<const char*, SQLiteWriter::var_t>> values{{"user", user}, {"ip", getIP()}, {"tstamp", time(0)}};
      for(const auto& f : fields)
        values.push_back(f);
      lsqw.addValue(values, "log");
      nlohmann::json j;
      for(const auto& v : values) {
        std::visit([&v,&j](auto&& arg) {
          j[v.first]=arg;
        }, v.second);
      }
      std::cout<<j.dump()<<std::endl;
    }
  };

  void setExtraCookieSpec(const std::string& spec);
  void setTrustedProxies(const std::vector<std::string>& ips, const std::string& realipheader);
  
  template<typename Func>
  void wrapGetOrPost(bool getOrPost, const std::set<Capability>& caps, const std::string& pattern, Func f) {
    auto func = [f, this, caps](const httplib::Request &req, httplib::Response &res) {
      std::string user;
      try {
        user = d_sessions.getUser(req, getIP(req));
      }
      catch(std::exception& e) {
        // cout<<"Error getting user from session: "<<e.what()<<endl;
      }
      for(const auto& c: caps) {
        if(!d_users.userHasCap(user, c, &req))
          throw std::runtime_error(fmt::format("Lacked a capability ({})", (int)c));
      }
      ComboReq cr{d_tp, d_lsqw, req, res, d_users, d_sessions, d_stats, *this, user};
      auto output = f(cr);
      if constexpr (std::is_same_v<decltype(output), std::pair<std::string, std::string>>) {
        res.set_content(output.first, output.second);
      }
      else {
        res.set_content(output.dump(), "application/json");
      }
    };
    getOrPost ? d_svr.Get(pattern, func) : d_svr.Post(pattern, func);
  }

  
  template<typename func>
  void wrapGet(const std::set<Capability>& caps, const std::string& pattern, func f) { wrapGetOrPost(true, caps, pattern, f); };
  template<typename func>
  void wrapPost(const std::set<Capability>& caps, const std::string& pattern, func f) { wrapGetOrPost(false, caps, pattern, f); };
  void standardFunctions();
};
