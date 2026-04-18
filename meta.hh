#pragma once
#include <vector>
#include "sqlwriter.hh"

/* This is where parts of tkconv that need to store operational metadata can do their thing */

// there are URLs that fail, and that we don't want to retry too often
class ThrottleDB
{
public:
  ThrottleDB();
  void report(const std::string& thing, const std::string& reason="");
  bool shouldThrottle(const std::string& thing, int limitSeconds, int limit);

private:
  SQLiteWriter d_sqlw;
  constexpr static unsigned int s_retention = 7*86400;
};

class ConversionFailureDB
{
public:
  ConversionFailureDB(const std::string& path="");
  void reportFailure(const std::string& id, const std::string& kind, const std::string& reason);
  void reportFix(const std::string& id, const std::string& kind, const std::string& reason);
  void reportFailedAttempt(const std::string& id, const std::string& kind, const std::string& reason);

  struct Item
  {
    const std::string id, kind, reason, reportingTime, fixedTime, fixedHow, attemptResult;
    bool success;
  };

  std::vector<Item> getUnattemptedItems();
  std::vector<Item> getItems();
private:
  SQLiteWriter d_sqlw;

};
