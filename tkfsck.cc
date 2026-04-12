#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <fmt/chrono.h>
#include <iostream>
#include "sqlwriter.hh"
#include "support.hh"
#include <set>
using namespace std;

int main(int argc, char** argv)
{
  set<string> idsondisk;

  SQLiteWriter sqlw("tk.sqlite3", SQLWFlag::ReadOnly);
  
  for(int x = 0; x <= 255; ++x) {
    for(int y = 0; y <= 255; ++y) {
      string path = fmt::sprintf("docs/%02x/%02x",  x, y);
      for(const auto& entry : std::filesystem::directory_iterator(path)) {
	std::string filenameStr = entry.path().filename().string();
	//if the first found entry is directory go thru it
	if(entry.is_regular_file() && entry.path().extension().string().empty()) {
	  idsondisk.insert(entry.path().filename().string());
	}
      }
    }
  }
  cout<<"Got "<<idsondisk.size()<<" files"<<endl;
  auto rowsdoc = sqlw.queryT("select id from Document");
  auto rowsversl = sqlw.queryT("select id from Verslag");
  set<string> idsindb;
  for(const auto& r : rowsdoc)
    idsindb.insert(eget(r, "id"));
  for(const auto& r : rowsversl)
    idsindb.insert(eget(r, "id"));
  cout <<"Got "<<idsindb.size()<<" ids in database"<<endl;

  std::vector<string> diff;
 
  std::set_difference(idsondisk.begin(), idsondisk.end(), idsindb.begin(), idsindb.end(),
                        std::inserter(diff, diff.begin()));

  for(const auto& d : diff) {
    cout << makePathForId(d) << "\n";
  }
 
}
