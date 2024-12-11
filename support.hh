#pragma once
#include <string>
#include <chrono>
#include "jsonhelper.hh"
#include "nlohmann/json.hpp"
#include "sqlwriter.hh"
#include <mutex>
#include "httplib.h"
#include "sws.hh"

struct DTime
{
  void start()
  {
    d_start =   std::chrono::steady_clock::now();
  }
  uint32_t lapUsec()
  {
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()- d_start).count();
    start();
    return usec;
  }

  std::chrono::time_point<std::chrono::steady_clock> d_start;
};

// we add the / to prefix for you
std::string makePathForId(const std::string& id, const std::string& prefix="docs", const std::string& suffix="", bool makepath=false);

// for external ids
bool haveExternalIdFile(const std::string& id, const std::string& prefix="op", const std::string& suffix=".odt");

// returns "f3/12" 
std::string getSubdirForExternalID(const std::string& in);

std::string makePathForExternalID(const std::string& id, const std::string& prefix="op", const std::string& suffix=".odt", bool makepath=false);

bool isPresentNonEmpty(const std::string& id, const std::string& prefix="docs", const std::string& suffix="");
bool isPresentRightSize(const std::string& id, int64_t size, const std::string& prefix="docs");
bool cacheIsNewer(const std::string& id, const std::string& cacheprefix, const std::string& suffix, const std::string& docprefix);
bool isPDF(const std::string& fname);
bool isDocx(const std::string& fname);
bool isDoc(const std::string& fname);
bool isRtf(const std::string& fname);
bool isXML(const std::string& fname);
uint64_t getRandom64();
std::string getLargeId();
bool endsWith(const std::string& str, const std::string& suffix);

time_t getTstamp(const std::string& str);
time_t getTstampUTC(const std::string& str);
void sendEmail(const std::string& server, const std::string& from, const std::string& to, const std::string& subject, const std::string& textBody, const std::string& htmlBody="");
void replaceSubstring(std::string &originalString, const std::string &searchString, const std::string &replaceString);
std::string htmlEscape(const std::string& data);
std::string getTodayDBFormat();

template<typename T>
std::string eget(const T& cont, const std::string& fname)
{
  std::string ret;
  auto iter = cont.find(fname);
  if(iter == cont.end() || !std::get_if<std::string>(&iter->second))
    return ret;

  return std::get<std::string>(iter->second);  
}

