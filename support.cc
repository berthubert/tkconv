#include "support.hh"
#include <fmt/format.h>
#include <fmt/printf.h>
#include <sys/stat.h>
using namespace std;

string makePathForId(const std::string& id)
{
  if(id.size() < 10)
    throw runtime_error("Incomplete ID requested");
  
  string a = id.substr(0,2);
  string b = id.substr(2,2);
  return fmt::sprintf("docs/%s/%s/%s", a, b, id);
}

bool isPresentNonEmpty(const std::string& id)
{
  struct stat sb;
  string fname = makePathForId(id);
  int ret = stat(fname.c_str(), &sb);
  if(ret < 0 || sb.st_size == 0)
    return false;
  return true;
}
