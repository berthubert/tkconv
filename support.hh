#pragma once
#include <string>
#include <chrono>
#include "jsonhelper.hh"
#include "nlohmann/json.hpp"
#include "sqlwriter.hh"
#include <mutex>
#include "httplib.h"

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

struct LockedSqw
{
  LockedSqw(const LockedSqw&) = delete;
  LockedSqw(SQLiteWriter& sqw_, std::mutex& sqwlock_) : sqw(sqw_), sqwlock(sqwlock_){}
  
  SQLiteWriter& sqw;
  std::mutex& sqwlock;
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


std::string makePathForId(const std::string& id);
bool isPresentNonEmpty(const std::string& id);
bool isPDF(const std::string& fname);
bool isDocx(const std::string& fname);
