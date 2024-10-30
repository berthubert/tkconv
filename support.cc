#include "support.hh"
#include <fmt/format.h>
#include <fmt/printf.h>
#include <sys/stat.h>
#include <vector>
#include <random>
#include "siphash.h"

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

// we add the slash to prefix for you, you need to put the . in the suffix (if you want one)
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

bool isPresentRightSize(const std::string& id, int64_t size, const std::string& prefix)
{
  struct stat sb;
  string fname = makePathForId(id, prefix);
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

// Function to check if a string ends with a particular suffix
bool endsWith(const std::string& str, const std::string& suffix) {
    // Check if the suffix is longer than the string itself
    if (suffix.size() > str.size()) {
        return false;
    }

    // Compare the end of the string with the suffix
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

string makePathForExternalID(const std::string& id, const std::string& prefix, const std::string& suffix, bool makepath)
{
  if(id.find_first_of("./") != string::npos)
    throw runtime_error("id for external file contained bad characters");

  if(makepath) {
    string fname = prefix;
    int err = mkdir(fname.c_str(), 0770);
    if(err < 0 && errno != EEXIST) 
      throw runtime_error("Could not mkdir prefix dir "+fname+": "+strerror(errno));
    
    string subdir=getSubdirForExternalID(id);
    fname += "/" + subdir.substr(0,2);
    
    err= mkdir(fname.c_str(), 0770);
    
    if(err < 0 && errno != EEXIST) 
      throw runtime_error("Could not mkdir prefix dir "+fname+": "+strerror(errno));
    
    fname += "/" + subdir.substr(3);
    err= mkdir(fname.c_str(), 0770);
    if(err < 0 && errno != EEXIST) 
      throw runtime_error("Could not mkdir prefix dir "+fname+": "+strerror(errno));
  }
  
  return prefix + "/" + getSubdirForExternalID(id)+"/" +id + suffix;
}

bool haveExternalIdFile(const std::string& id, const std::string& prefix, const std::string& suffix)
{
  if(id.find_first_of("./") != string::npos)
    return false;
  
  struct stat sb;
  string fname = prefix + "/" + getSubdirForExternalID(id)+"/" +id + suffix;
  int ret = stat(fname.c_str(), &sb);

  if(ret < 0 || sb.st_size == 0)
    return false;
  return true;
}


/*int siphash(const void *in, const size_t inlen, const void *k, uint8_t *out,
            const size_t outlen);

    Computes a SipHash value
    *in: pointer to input data (read-only)
    inlen: input data length in bytes (any size_t value)
    *k: pointer to the key data (read-only), must be 16 bytes 
    *out: pointer to output data (write-only), outlen bytes must be allocated
    outlen: length of the output in bytes, must be 8 or 16
*/

// returns "f3/12", used for external id's 
string getSubdirForExternalID(const std::string& in)
{
  static unsigned char k[16]={1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  unsigned char out[8]={};
  int outlen = sizeof(out);

  siphash((const void*) in.c_str(), in.length(), k, out, outlen);
  return fmt::sprintf("%02x/%02x", out[4], out[6]);
}

time_t getTstamp(const std::string& str)
{
  //  2024-09-17T13:00:00
  //  2024-09-17T13:00:00+0200
  struct tm tm={};
  strptime(str.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
  
  return timelocal(&tm);
}

time_t getTstampUTC(const std::string& str)
{
  //  2024-09-17T13:00:00Z
  struct tm tm={};
  strptime(str.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
  
  return timegm(&tm);
}
