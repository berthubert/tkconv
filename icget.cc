#include "support.hh"
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <fmt/chrono.h>
#include <iostream>
#include "httplib.h"
#include "sqlwriter.hh"
#include "pugixml.hpp"

using namespace std;

void storeICDocument(const std::string& entryid, const std::string& nummer, string suffix, const std::string& content)
{
  string fname = makePathForExternalID(entryid+"-"+nummer, "ic", suffix, true);
  FILE* t = fopen((fname+".tmp").c_str(), "w");
  if(!t)
    throw runtime_error("Unable to open file "+fname+": "+string(strerror(errno)));
  
  shared_ptr<FILE> fp(t, fclose);
  if(fwrite(content.c_str(), 1, content.size(), fp.get()) != content.size()) {
    unlink(fname.c_str());
    throw runtime_error("Partial write to file "+fname);
  }
  fp.reset();
  
  if(rename((fname+".tmp").c_str(), fname.c_str()) < 0) {
    int e = errno;
    unlink((fname+".tmp").c_str());
    throw runtime_error("Unable to rename saved file "+fname+".tmp - "+strerror(e));
  }
}


void assureDocuments(SQLiteWriter& sqlw)
{
  auto docs = sqlw.queryT("select * from ICDocument");
  for(const auto& d : docs) {
    string entryId = eget(d, "entryId");
    string nummer = eget(d, "nummer");
    if(!haveExternalIdFile(entryId+"-"+nummer, "ic", ".pdf")) {
      cout<<"Miss file for "<<eget(d, "titel") << endl;
      // https://www.internetconsultatie.nl/isde/document/16024
      string url = "https://www.internetconsultatie.nl/"+ entryId + "/document/"+nummer;
      cout << "Going to retrieve from: '"<< url<<"'"<<endl;
      string content = getContentsFromUrl(url);
      storeICDocument(entryId, nummer, ".pdf", content);
    }
  }
}

int main(int argc, char** argv)
{
  httplib::Client cli("https://www.internetconsultatie.nl");
  cli.set_connection_timeout(15, 0); 
  cli.set_read_timeout(15, 0); 
  cli.set_write_timeout(15, 0); 
  cli.set_follow_location(true);

  
  SQLiteWriter sqlw("ic.sqlite3");

  sqlw.queryT("CREATE table if not exists ICEntry (id TEXT)");
  sqlw.queryT("CREATE unique index if not exists ICEntryUnique on ICEntry(id)");


  sqlw.queryT("CREATE table if not exists ICDocument (entryId TEXT NOT NULL REFERENCES ICEntry(id) ON DELETE CASCADE)");
   
  string url="https://www.internetconsultatie.nl/rest/openconsultaties/";
  auto res = cli.Get(url);
  if(!res) {
    auto err = res.error();
    cerr << "Oops retrieving '"<<url<<"' " << httplib::to_string(err) << endl;
    
    exit(EXIT_FAILURE);
  }
  pugi::xml_document doc;
  if (!doc.load_string(res->body.c_str())) {
    cout<<"Could not load XML"<<endl;
    return -1;
  }
  
  auto feed = doc.child("consultaties");
  if(!feed) {
    cout<<"No feed in XML at "<< url <<"\n";
    return -1;
  }

  /*
    <consultatie>
    <titel>Wijzigingsregeling ISDE 2027</titel>
    <url>https://www.internetconsultatie.nl/isde</url>
    <startdatum>17-07-2026</startdatum>
    <einddatum>21-08-2026</einddatum>
    <departementen>
    <departement>EZK</departement>
    </departementen>
    </consultatie>
  */

  // the "unique key" here is "isde", which leads to a URL
  // from this URL we can scrape numbered documents, with a title
  // there is no indication the number is globally unique
  // best to say the document id is 'isde-16024'

  /* strategy might be:
     retrieve the full XML every time, and check if there are new unique keys in there
     for each unique id, try to retrieve the page and all documents
  */  

  auto fixDate = [](const string& in) {
    if(in.size() != 10 || in[2] != '-' || in[5] != '-')
      throw runtime_error("Incorrect date format '"+in+"'");
    
    string ret;
    ret = in.substr(6);
    ret += "-";
    ret += in.substr(3,2);
    ret += "-";
    ret += in.substr(0,2);
    return ret;
  };
  
  for(const auto& node : feed.children("consultatie")) {
    string startdatum = fixDate(node.child("startdatum").child_value());
    string einddatum = fixDate(node.child("einddatum").child_value());
    string id = node.child("url").child_value();
    string titel = trim_both(node.child("titel").child_value()); // trailing spaces somehow
    if(!id.starts_with("https://www.internetconsultatie.nl/")) {
      cout <<"non-canonical URL!"<<endl;
      continue;
    }
    id = id.substr(35);

    if(id.find_first_of("./") != string::npos) {
      cout <<"Skipping bad id "<<id<<endl;
      continue;
    }

    
    vector<string> departementen;
    for(const auto& dep : node.children("departementen")) {
      departementen.push_back(dep.child("departement").child_value());
    }
    nlohmann::json depo =  departementen;
    try {
      auto present=sqlw.queryT("select count(1) c from ICEntry where id=?", {id});
      if(iget(present[0], "c")) {
	cout<<"Skipping "<<id<<", known already"<<endl;
	continue;
      }
    }
    catch(std::exception& e)
    {
      cout<<"Could not check ICEntry for existance, possible first run: "<<e.what()<<endl;
    }
    
    
    cout << node.child("startdatum").child_value() <<" ("<<startdatum<<"): "<<id<<" "<<node.child("titel").child_value() << endl;
    /*
                <p class="p-intro">Deze consultatie gaat over een wijziging van de Investeringssubsidie duurzame energie en energiebesparing (ISDE). Deze regeling biedt subsidie voor verduurzamingsmaatregelen zoals isolatie en warmtepompen aan woningeigenaren en zakelijke gebouweigenaren. Via de wijziging die hierbij in internetconsultatie gaat, wordt de regeling verbeterd en vereenvoudigd.</p>
<p>In de gewijzigde ISDE regeling zijn verbetervoorstellen vanuit marktpartijen en de Rijksdienst voor Ondernemend Nederland (RVO) verwerkt. De voorgenomen wijzigingen betreffen onder andere het aanpassen van de categorisering en subsidiehoogte van glasisolatie en het aanpassen van de opbouw van het subsidiebedrag voor warmtepompen. Een belangrijk onderdeel van deze consultatie is daarnaast het voornemen om aanvullende technische vereisten te stellen om het slim kunnen aansturen van warmtepompen te faciliteren, in verband met netcongestie. Er worden twee opties voorgesteld. Door middel van deze consultatie wordt u gevraagd te reageren op de impact en uitvoerbaarheid van deze opties.</p>
                
                    <p>
                        <a href="/isde/reageren" id="mainContentPlaceHolder_reagerenAnchor" class="cta">Reageren op deze consultatie</a>
                    </p>
                
            </section>

     */

    

    string htmlurl = node.child("url").child_value();
    res = cli.Get(htmlurl);
    if(!res) {
      cout << "Error retrieving "<<htmlurl <<": "<<httplib::to_string(res.error())<<endl;
      continue;
    }
    const string& html = res->body;    
    regex introreg(R"(<p class="p-intro">(.*)</p>)");
    std::smatch sms;
    string intro, introplus;
    if (std::regex_search(html, sms, introreg)) {
      intro = trim_both(deHTML(sms[1]));
      replaceSubstring(intro, "&nbsp;", " ");
      cout << intro <<endl;

      // now capture the rest
      introplus = sms.suffix();
      auto pos = introplus.find("</section>");
      if(pos != string::npos) {
	introplus.resize(pos);
      }
      else
	cout<<"Could not find </section> in "<<introplus<<endl;
      introplus = deHTML(introplus, "\n");
      replaceSubstring(introplus, "Reageren op deze consultatie", "");
      replaceSubstring(introplus, "&nbsp;", " ");
      introplus = trim_both(introplus);
    }
    else {
      cout << html;
      exit(1);
    }

    // next up, harvest documents and their titles
    
    /*
    <a href="/landelijkeveiligheidsmonitor/document/16009" id="mainContentPlaceHolder_conceptregelingDocumentDownloadLink_downloadAnchor" class="button ">Downloaden <span class="visually-hidden">ministeriële regeling</span></a>
    */

    regex docregex(R"_(<a href="/[^/]*/document/([0-9]*)" [^>]*>Downloaden (.*)</a>)_");
    auto doc_begin = std::sregex_iterator(html.begin(), html.end(), docregex);
    auto doc_end = std::sregex_iterator();

    if(doc_begin != doc_end) { // we have a somewhat well parsed entry
      sqlw.addValue({{"id", id}, {"departementen", depo.dump()}, {"titel", titel}, {"intro", intro}, {"introplus", introplus}, {"startdatum", startdatum}, {"einddatum", einddatum}, {"retrievalTime", getNowDBFormat()}}, "ICEntry");
      for(auto i = doc_begin; i!= doc_end ; ++i) {
	std::smatch m = *i;
	string titel = trim_both(deHTML(m[2]));
	string nummer=m[1];
	if(nummer.find_first_of("./") != string::npos)
	  continue;
	cout<<"Document number: "<< nummer <<", name: '"<<titel<<"'"<<endl;
	sqlw.addValue({{"entryId", id}, {"nummer", nummer}, {"titel", titel}}, "ICDocument");
	// https://www.internetconsultatie.nl/landelijkeveiligheidsmonitor/document/16009
      }
    }
    sleep(1);
  }
  assureDocuments(sqlw);
}
