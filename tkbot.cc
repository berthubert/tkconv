#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <iostream>
#include "httplib.h"
#include "sqlwriter.hh"
#include "pugixml.hpp"

using namespace std;

void makePost(auto r)
{
  auto datum = get<string>(r["datum"]);
  if(auto pos = datum.find('T'); pos != string::npos)
    datum.resize(pos);
  string soort = get<string>(r["soort"]);
  string onderwerp = get<string>(r["onderwerp"]);
  if(onderwerp.find("Beslisnota") != string::npos)
    return;

  string dir="kamervragen";
  if(soort=="Brief regering")
    dir = "brieven_regering";

  string url = fmt::format("https://www.tweedekamer.nl/kamerstukken/{}/detail?id={}&did={}", 	     dir,
			   get<string>(r["nummer"]),
			   get<string>(r["nummer"]));

  url=get<string>(r["enclosure"]);

  
  fmt::print("{}, {}: {} ({}) {}\n",
	     datum,
	     soort, 
	     onderwerp,
	     get<string>(r["nummer"]),
	     url
	     );

}

int main(int argc, char** argv)
{
  SQLiteWriter sqlw("tk.sqlite3");
  string category="Document";
  int64_t hwm;
  try {
    auto ret = sqlw.queryT("select latest from bothwm");
    if(!ret.empty())
      hwm = get<int64_t>(ret[0]["latest"]);
  }
  catch(std::exception& e) {
    fmt::print("Error: {}\n", e.what());
    fmt::print("No high water mark yet\n");
    auto ret = sqlw.queryT("select max(rowid) as hwm from\n "+category);
    if(!ret.empty()) {
      sqlw.addValue({{"latest", get<int64_t>(ret[0]["hwm"])}, {"category", category}}, "bothwm");
      fmt::print("Set high water mark for {} to {}\n", category, get<int64_t>(ret[0]["hwm"]));
      return 0;
    }
  }
  //  hwm -= 150;
  fmt::print("Retrieving updates beyond {} for {}\n", hwm, category);

  auto rows = sqlw.queryT("select rowid,* from "+category+" where rowid > ? order by rowid asc", {hwm} );
  fmt::print("Received {} updates for {}\n", rows.size(), category);
  int numdup=0, numremoved=0;
  int64_t newhwm = hwm;
  for(auto& r : rows) {
    newhwm = get<int64_t>(r["rowid"]);
    auto earlier = sqlw.queryT("select * from "+category+" where id=? and rowid <= ?",
			       {get<string>(r["id"]), hwm});
    if(!earlier.empty()) {
      //      fmt::print("{} was a dup, {} copies\n", get<string>(r["id"]), earlier.size());
      numdup++;
      continue;
    }
    if(get<int64_t>(r["verwijderd"])) {
      //      fmt::print("{} was removed\n", get<string>(r["id"]));
      numremoved++;
      continue;
    }
    makePost(r);
  }
  fmt::print("Filtered {} duplicates, {} removed updates\n",
	     numdup,
	     numremoved);
  sqlw.query("update bothwm set latest=? where category=?", {newhwm, category});
  fmt::print("Storing new hwm {}\n", hwm);
}
