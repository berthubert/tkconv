#include "support.hh"
#include <fmt/format.h>
#include <fmt/printf.h>
#include <sys/stat.h>
#include <vector>
#include <random>

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
bool isXML(const std::string& fname)
{
  return fileStartsWith(fname, "<?xml") || fileStartsWith(fname, "\xef\xbb\xbf<?xml");
}

bool isDoc(const std::string& fname)
{
  return fileStartsWith(fname, "\xd0\xcf\x11\xe0");
}

bool isRtf(const std::string& fname)
{
  return fileStartsWith(fname, "{\\rtf1");
}

// we add the slash to prefix for you
string makePathForId(const std::string& id, const std::string& prefix, const std::string& suffix, bool makepath)
{
  if(id.size() < 10)
    throw runtime_error("Incomplete ID requested");
  // 92c78e5c-0bc0-4d3e-b1df-d43a100124bb
  if(id.find_first_not_of("0123456789-abcdef") != string::npos)
    throw runtime_error("Invalid ID");
  
  string a = id.substr(0,2);
  string b = id.substr(2,2);

  if(makepath) {
    string dir = prefix;
    int err = mkdir(dir.c_str(), 0770);
    if(err < 0 && errno != EEXIST)
      throw runtime_error("Could not mkdir prefix dir "+dir+": "+strerror(errno));
    dir += "/"+a;
    err = mkdir(dir.c_str(), 0770);
    if(err < 0 && errno != EEXIST)
      throw runtime_error("Could not mkdir storage dir "+dir);
    dir += "/"+b;
    err = mkdir(dir.c_str(), 0770);
    if(err < 0 && errno != EEXIST)
      throw runtime_error("Could not mkdir storage dir "+dir);
  }
  
  return fmt::sprintf("%s/%s/%s/%s%s", prefix,a, b, id, suffix);
}

bool isPresentNonEmpty(const std::string& id, const std::string& prefix, const std::string& suffix)
{
  struct stat sb;
  string fname = makePathForId(id, prefix, suffix);
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

bool cacheIsNewer(const std::string& id, const std::string& cacheprefix, const std::string& suffix, const std::string& docprefix)
{

  string cachename = makePathForId(id, cacheprefix, suffix);
  string origname = makePathForId(id, docprefix);

  struct stat sbcache, sborig;
  int ret = stat(cachename.c_str(), &sbcache);
  if(ret < 0 || sbcache.st_size == 0) // not newer then
    return false;

  ret = stat(origname.c_str(), &sborig);
  if(ret < 0 || sborig.st_size == 0) // if orig is gone, cache is newer!
    return true;

  return sborig.st_mtim.tv_sec < sbcache.st_mtim.tv_sec;
}


uint64_t getRandom64()
{
  static std::random_device rd; // 32 bits at a time. At least on recent Linux and gcc this does not block
  return ((uint64_t)rd() << 32) | rd();
}
