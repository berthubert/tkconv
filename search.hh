#pragma once
#include "sqlwriter.hh"
#include <vector>
#include <set>
#include <string>

// you give this a SQLite connection with both tkindex and tk in there
struct SearchHelper
{
  explicit SearchHelper(SQLiteWriter& sqw) : d_sqw(sqw)
  {}
  
  struct Result
  {
    std::string nummer;
    std::string relurl;
    std::string categorie;
    std::string bijgewerkt;
    std::string soort;
    std::string datum;
    std::string snippet;
    std::string titel;
    std::string onderwerp;
  };

  std::vector<Result> search(const std::string& query,
			     const std::set<std::string>& categories={},
			     const std::string& cutoff="");
  
  SQLiteWriter& d_sqw;
};
