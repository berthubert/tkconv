#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <fmt/chrono.h>
#include <iostream>
#include "sqlwriter.hh"
#include "support.hh"
#include <set>
#include "meta.hh"
#include <magic.h>
using namespace std;

struct MHelper
{
  MHelper()
  {
    d_magic = magic_open(MAGIC_MIME_TYPE);
    if(!d_magic)
      throw runtime_error("Unable to open magic library");
    magic_load(d_magic, nullptr);
  }
  ~MHelper()
  {
    magic_close(d_magic);
  }

  std::string get(const std::string& fname)
  {
    auto ret = magic_file(d_magic, fname.c_str());
    if(!ret)
      return "Unknown file '"+fname+"'";
    return ret;
  }
  
  magic_t d_magic;
};

int main(int argc, char** argv)
{
  set<string> idsondisk;

  SQLiteWriter sqlw("tk.sqlite3", SQLWFlag::ReadOnly);
  
  ConversionFailureDB cfdb;
  MHelper mh;
  auto unattempted = cfdb.getUnattemptedItems();
  fmt::print("Database says there are {} (OO)Documents we should look at\n", unattempted.size());

  map<string, int> mimefailures;
  for(const auto& item : unattempted) {
    if(item.id.find_first_of("./") != string::npos) {
      fmt::print("{} contained a non-id character skipping", item.id);
      continue;
    }

    if(item.kind == "Document") {
      if(isPresentNonEmpty(item.id, "improvdocs")) {
	cout << item.id << " got improved already!"<<endl;
	cfdb.reportFix(item.id, item.kind, "Found as fixed");
	// maybe report it as fixed?
	continue;
      }
      if(!isPresentNonEmpty(item.id)) {
	cout<<item.id<<" is missing in the first place"<<endl;
	continue;
      }
      
      string fname = makePathForId(item.id);

      if(!isPDF(fname)) {
	string mimetype = mh.get(fname);
	fmt::print("{} is not a PDF in the first place, looks like {}\n",
		   fname, mimetype);
	cfdb.reportFailedAttempt(item.id, item.kind, "Unsupported mime type "+mimetype);
	mimefailures[mimetype]++;
	continue;
      }
      
      // also create path if needed
      string improvfname = makePathForId(item.id, "improvdocs", "", true);
      int ret = system(("ocrmypdf --redo-ocr -l nld "+fname+" "+improvfname).c_str());

      if(ret >= 0)  {
	if(isPresentNonEmpty(item.id, "improvdocs", "")) {
	  int rc = WEXITSTATUS(ret);
	  fmt::print("Improvement worked? {}\n", rc);
	  
	  if(rc == 0 )
	    cfdb.reportFix(item.id, item.kind, "OCRMyPdf");
	  else
	    cfdb.reportFailedAttempt(item.id, item.kind, "OCRMyPdf error code "+to_string(rc));
	}
	else
	  cfdb.reportFailedAttempt(item.id, item.kind, "OCRMyPdf did not produce a file");

      }

      
    }
    else if(item.kind == "OODocument") {
      if(haveExternalIdFile(item.id, "improvoo", ".pdf")) {
	cout << "OODocument "<< item.id << " got improved already!"<<endl;
	cfdb.reportFix(item.id, item.kind, "Found as fixed");
	// maybe report it as fixed?
	continue;
      }
      if(!haveExternalIdFile(item.id, "oo", ".pdf")) {
	cout<<item.id<<" is missing in the first place"<<endl;
	continue;
      }
      
      string fname = makePathForExternalID(item.id, "oo", ".pdf");

      if(!isPDF(fname)) {
	string mimetype = mh.get(fname);
	fmt::print("{} is not a PDF in the first place, looks like {}\n",
		   fname, mimetype);
	cfdb.reportFailedAttempt(item.id, item.kind, "Unsupported mime type "+mimetype);
		
	mimefailures[mimetype]++;
	continue;
      }
      
      // also create path if needed
      string improvfname = makePathForExternalID(item.id, "improvoo", ".pdf", true);
      int ret = system(("ocrmypdf --redo-ocr -l nld "+fname+" "+improvfname).c_str());
      
      if(ret >= 0) {
	if(haveExternalIdFile(item.id, "oo", ".pdf")) {
	  int rc = WEXITSTATUS(ret);
	  fmt::print("Improvement worked? {}\n", rc);
	  if(rc==0)
	    cfdb.reportFix(item.id, item.kind, "OCRMyPdf");
	  else
	    cfdb.reportFailedAttempt(item.id, item.kind, "OCRMyPdf error code "+to_string(rc));
	}
	else
	  cfdb.reportFailedAttempt(item.id, item.kind, "OCRMyPdf did not produce a file");
      }
      
    }

  }
  fmt::print("Mime types we could not deal with:\n{}\n",
	     mimefailures);
}
