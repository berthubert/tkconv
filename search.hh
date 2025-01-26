#pragma once
#include "sqlwriter.hh"
#include <vector>
#include <set>
#include <string>

/* Important notes. SearchHelper accepts the FTS5 language, and not the user-friendly
   variant offered by the opentk user interface. In other words, you must call
   convertToSQLiteFTS5 to deal with user input.

   Secondly, the search index contains Verslagen of Vergaderingen. However, we want to
   expose the vergadering ID to users. So this class looks up the Vergadering ID and passes
   it back to the caller.

   The 'categorie' however is still called 'Verslag'.
*/

// you give this a SQLite connection with both tkindex and tk in there
struct SearchHelper
{
  explicit SearchHelper(SQLiteWriter& sqw) : d_sqw(sqw)
  {}
  
  struct Result
  {
    std::string nummer;
    std::string relurl; // vergadering.html?nummer=123-324 etc
    std::string categorie;
    std::string bijgewerkt;
    std::string soort;
    std::string datum;
    std::string snippet;
    std::string titel;
    std::string onderwerp;
    int persoonnummer=-1;
    double score;
  };

  std::vector<Result> search(const std::string& query,
			     const std::set<std::string>& categories={},
			     const std::string& cutoff="", unsigned int mseclimit=10000,
			     unsigned int itemlimit = 0);
  
  SQLiteWriter& d_sqw;
};
std::set<std::pair<std::string, std::string>> getZakenFromDocument(const std::string& id);
