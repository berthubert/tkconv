#include "support.hh"
#include <fmt/format.h>
#include <fmt/printf.h>
#include <sys/stat.h>
#include <vector>

using namespace std;


static bool fileStartsWith(const std::string& fname, const std::string& start)
{
  vector<char> buf(start.size());
  FILE* fp = fopen(fname.c_str(), "r");
  if(!fp)
    throw runtime_error("Can't check if "+fname+" is a PDF file, "+string(strerror(errno)));
  shared_ptr<FILE> tfp(fp, fclose);
  if(fread(&buf[0], 1, buf.size(), tfp.get()) != buf.size())
    throw runtime_error("Can't read from "+fname+", "+string(strerror(errno)));
  return (memcmp(&buf[0], start.c_str(), start.size())==0);
}


bool isPDF(const std::string& fname)
{
  return fileStartsWith(fname, "%PDF");
}

bool isDocx(const std::string& fname)
{
  return fileStartsWith(fname, "PK");
}

bool isDoc(const std::string& fname)
{
  return fileStartsWith(fname, "\xd0\xcf\x11\xe0");
}


string makePathForId(const std::string& id)
{
  if(id.size() < 10)
    throw runtime_error("Incomplete ID requested");
  // 92c78e5c-0bc0-4d3e-b1df-d43a100124bb
  if(id.find_first_not_of("0123456789-abcdef") != string::npos)
    throw runtime_error("Invalid ID");
  
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

bool isPresentRightSize(const std::string& id, int64_t size)
{
  struct stat sb;
  string fname = makePathForId(id);
  int ret = stat(fname.c_str(), &sb);
  if(ret < 0 || sb.st_size != size)
    return false;
  return true;
}
