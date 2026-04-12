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

/* the open.overheid.nl route, start with:

https://open.overheid.nl/overheid/openbaarmakingen/api/v0/zoek?zoektekst=&start=0&aantalResultaten=50&sort=laatst-gewijzigd

"
{
  "totaal": 663177,
  "resultaten": [
    {
      "document": {
        "id": "eef2edec-019e-4085-8e2e-4d3f42686431_1",
        "pid": "https://open.overheid.nl/documenten/eef2edec-019e-4085-8e2e-4d3f42686431",
        "titel": "Kaart Oudemolen 10km",
        "omschrijving": "Deze kaart toont het vervoersbeperkingsgebied 10 km in verband met een besmetting vogelgriep op een locatie in Oudemolen.",
x       "openbaarmakingsdatum": "2026-03-21",
        "publisher": "ministerie van Landbouw, Visserij, Voedselzekerheid en Natuur",
        "aanbieder": "Aanleverloket",
x       "mutatiedatumtijd": "2026-03-21T09:23:36.587Z"
      },
      "bestandsgrootte": "0.94 MB",
      "aantalPaginas": 1,
      "bestandsType": "application/pdf"
    },
"

// actual document PDF, so ignore the 'pid'
https://open.overheid.nl/overheid/openbaarmakingen/api/v0/attachment/eef2edec-019e-4085-8e2e-4d3f42686431

Next block:

https://open.overheid.nl/overheid/openbaarmakingen/api/v0/zoek?zoektekst=&start=50&aantalResultaten=50&sort=laatst-gewijzigd ('start=50')
   
Data about a specific document:

https://open.overheid.nl/overheid/openbaarmakingen/api/v0/zoek/eef2edec-019e-4085-8e2e-4d3f42686431

{
  "document": {
    "pid": "https://open.overheid.nl/documenten/eef2edec-019e-4085-8e2e-4d3f42686431",
    "identifiers": [
      "nvt"
    ],
    "verantwoordelijke": {
      "type": "overheid:Ministerie",
      "id": "https://identifier.overheid.nl/tooi/id/ministerie/mnre1153",
x     "label": "ministerie van Landbouw, Visserij, Voedselzekerheid en Natuur"
    },
x   "medeverantwoordelijken": [],
    "publisher": {
      "type": "overheid:Ministerie",
      "id": "https://identifier.overheid.nl/tooi/id/ministerie/mnre1153",
      "label": "ministerie van Landbouw, Visserij, Voedselzekerheid en Natuur"
    },
    "opsteller": {
      "id": "https://identifier.overheid.nl/tooi/id/organisatie_onbekend",
      "label": "Onbekend"
    },
    "language": {
      "type": "eu:Taal",
      "id": "http://publications.europa.eu/resource/authority/language/NLD",
      "label": "Nederlands"
    },
    "onderwerpen": [],
x   "omschrijvingen": [
      "Deze kaart toont het vervoersbeperkingsgebied 10 km in verband met een besmetting vogelgriep op een locatie in Oudemolen."
    ],
    "titelcollectie": {
x     "officieleTitel": "Kaart Oudemolen 10km",
      "verkorteTitels": [],
      "alternatieveTitels": []
    },
    "classificatiecollectie": {
      "documentsoorten": [
        {
          "type": "overheid:Informatietype",
          "id": "https://identifier.overheid.nl/tooi/def/thes/kern/c_b3a6a7fc",
          "label": "kaart"
        }
      ],
      "themas": [
        {
          "id": "https://identifier.overheid.nl/tooi/id/thema_onbekend",
          "label": "Onbekend"
        }
      ],
      "onderwerpenRonl": [
        {
          "id": "https://opendata.rijksoverheid.nl/v1/sources/rijksoverheid/infotypes/subject/57aebb5a-d753-4159-a615-2627bc961e4b",
          "label": "Vogelgriep"
        }
      ],
      "informatiecategorieen": [
        {
          "type": "woo:Informatiecategorie",
          "id": "https://identifier.overheid.nl/tooi/def/thes/kern/c_759721e2",
          "label": "ontwerpen van wet- en regelgeving met adviesaanvraag"
        }
      ],
      "trefwoorden": [],
      "wooinformatiecategorie": []
    },
y   "hasParts": [],
    "documenthandelingen": [
      {
        "soortHandeling": {
          "id": "https://identifier.overheid.nl/tooi/def/thes/kern/c_641ecd76",
          "label": "vaststelling"
        },
        "atTime": "2026-03-21T09:22:31.961Z",
        "wasAssociatedWith": {
          "id": "https://identifier.overheid.nl/tooi/id/ministerie/mnre1153",
          "label": "ministerie van Landbouw, Visserij, Voedselzekerheid en Natuur"
        }
      }
    ],

    "extraMetadata": [],
    "publicatiebestemming": [
      "rijksoverheid.nl"
    ]
  },
  "plooiIntern": {
    "dcnId": "plooi-api-eef2edec-019e-4085-8e2e-4d3f42686431",
    "aanbieder": "Aanleverloket",
    "extId": [
      "eef2edec-019e-4085-8e2e-4d3f42686431"
    ],
    "sourceLabel": "plooi-api",
    "publicatiestatus": "gepubliceerd"
  },
  "documentrelaties": [
    {
x     "relation": "https://open.overheid.nl/documenten/b2e91e90-a433-4985-91ed-5c1bffbc72a4",
      "role": "https://identifier.overheid.nl/tooi/def/thes/kern/c_4d1ea9ba"   // PARENT
    }
  ],
  "versies": [
    {
x     "nummer": 1,
      "oorzaak": "aanlevering",
      "openbaarmakingsdatum": "2026-03-21",
      "mutatiedatumtijd": "2026-03-21T09:23:36.587641713Z",
      "blokkades": [],
      "bestanden": [
        {
x         "id": "https://open.overheid.nl/documenten/eef2edec-019e-4085-8e2e-4d3f42686431/file",
          "gepubliceerd": true,
          "label": "file",
x         "mime-type": "application/pdf",
x         "bestandsnaam": "KAART_2026_AI_Oudemolen_NB_I_10KM.pdf",
x         "mutatiedatumtijd": "2026-03-21T09:23:36.515964209Z",
x         "grootte": 985738,
          "paginas": 1,
x         "hash": "0c2f34fe8af84928dc2ea835836ca6ed1c457f4d"
        }
      ]
    }
  ]

*/


/* From the above we can derive a lot of things. Key is to also get the documentrelaties, and set these as bronDocument
   It is also a lot of fun to store the hash, bestandsnaam, mime-type, file size. 

   Let's store the whole json detail thing just to be sure!

   Mechanism, request 50 most recently mutated documents
   Check if we have them, and if the mutation time matches, if so, do nothing
   If mutation time doesn't match, we're going to replace everything

   If there was no change in the 50, stop.

   Otherwise get next 50, unless we've hit our stop date.

   Then, we have a set to upgrade:
      Get further details and PDF for new/changed things
   

   
*/
   
string getUrl(const std::string& url)
{
  string host;
  if(auto pos = url.find('/', 8);  pos != string::npos) {
    // 01234567
    // https://
    host = url.substr(0, pos);
  }
  httplib::Client cli(host);
  cli.set_connection_timeout(15, 0); 
  cli.set_read_timeout(15, 0); 
  cli.set_write_timeout(15, 0); 
  
  auto res = cli.Get(url);
  if(!res)  {
    auto err = res.error();
    throw runtime_error("Oops retrieving: "+httplib::to_string(err));
  }
  return res->body;
}

void storeRODocument(const std::string& id, string suffix, const std::string& content)
{
  string fname = makePathForExternalID(id, "ro", suffix, true);
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


int main(int argc, char** argv)
{
  httplib::Client cli("https://open.overheid.nl");
  cli.set_connection_timeout(15, 0); 
  cli.set_read_timeout(15, 0); 
  cli.set_write_timeout(15, 0); 

  SQLiteWriter sqlw("oo.sqlite3");

  sqlw.queryT("CREATE table if not exists OOEntry (id TEXT)");
  sqlw.queryT("CREATE unique index if not exists OOEntryUnique on OOEntry(id)");
  

  sqlw.queryT("CREATE table if not exists OODocument (id TEXT NOT NULL REFERENCES OOEntry(id) ON DELETE CASCADE)");
  sqlw.queryT("CREATE unique index if not exists OODocumentUnique on OODocument(id)");
  
  int numresultaten=50;
  string newest="2025-01-01";

  bool dynamicStartDate=true;
  if(dynamicStartDate) {
    try
      {
	auto res = sqlw.queryT("select max(openbaarmakingsdatum) o from OOEntry");
	if(!res.empty())
	  newest = eget(res[0], "o");
      }
    catch(exception& e) {
      cout<<"Could not get newest date from database, perhaps doesn't exist yet"<<endl;
    }
  }
  cout<<"We are interested in entries newer than "<<newest<<endl;
  time_t t = getDateTimestamp(newest);
  int known=0, newentries=0;  
  for(;;) {
    if(t > time(nullptr))
      break;
    
    string startDatumNLFormat = fmt::format("{:%d-%m-%Y}", fmt::localtime(t));
    string day=  fmt::format("{:%Y-%m-%d}", fmt::localtime(t));
    
    cout<<"NL: "<<startDatumNLFormat<<", normal: "<<day<<endl;
    int start=0;

    for(;;) {
      sleep(1);

      // https://open.overheid.nl/overheid/openbaarmakingen/api/v0/zoek?zoektekst=&start=0&aantalResultaten=20&sort=publicatiedatum&publicatiedatumVan=01-01-2026
      string url = fmt::format("https://open.overheid.nl/overheid/openbaarmakingen/api/v0/zoek?zoektekst=&start={}&aantalResultaten={}&sort=publicatiedatum&order=asc&publicatiedatumVan={}", start, numresultaten, startDatumNLFormat);
      
      /*
	string url = fmt::format("https://open.overheid.nl/overheid/openbaarmakingen/api/v0/zoek?zoektekst=&start={}&aantalResultaten={}&sort=laatst-gewijzigd", start, numresultaten);
      */
      auto res = cli.Get(url);
      
      if(!res) {
	auto err = res.error();
	cerr << "Oops retrieving: " << httplib::to_string(err) << endl;
	continue;
	//	throw runtime_error("Oops retrieving: "+httplib::to_string(err));
      }
      
      nlohmann::json openbaarmakingen = nlohmann::json::parse(res->body.c_str());
      auto resultaten = openbaarmakingen["resultaten"];
      
      if(resultaten.empty()) {
	cout<<"No results returned!"<<endl;
	break;
      }
      else {
	cout<<"Got "<<resultaten.size()<<" possible entries"<<endl;
      }

      int overhang=0;
      for(const auto& o : resultaten) {
	// their format is yyyy-mm-dd (as it should be)
	string openbaarmakingsdatum = o["document"]["openbaarmakingsdatum"];
	if(openbaarmakingsdatum != day) 
	  overhang++;
	
	if(overhang > 3) {
	  cout << "Got too much of a new day "<< openbaarmakingsdatum <<" which is not the day we want "<<day<<endl;
	  goto done;
	}
	
	if(!o.count("bestandsType")) {
	  cout<< "Skipping "<<o["document"]["titel"]<<" has no file associated with it"<<endl;
	  continue;
	}
	string id = o["document"]["id"];
	
	if(auto pos = id.find('_'); pos != string::npos)
	  id.resize(pos);
	string mutatiedatumtijd = o["document"]["mutatiedatumtijd"];



	// we do this check since addOrReplace causes a delete on the OODocument through cascade!
	auto present = sqlw.queryT("select count(1) c from OOEntry where id=? and mutatiedatumtijd=?",
		    {id, (string)o["document"]["mutatiedatumtijd"]});
	if(!iget(present[0], "c")) {
	  cout<< openbaarmakingsdatum<<": "<<o["document"]["titel"] << ": " << o["bestandsType"]<<" id "<<id<<" "<<mutatiedatumtijd<<endl;
	  newentries++;
	  sqlw.addOrReplaceValue({
	    {"id", id},
	    {"titel", (string)o["document"]["titel"]},
	    {"omschrijving", o["document"].count("omschrijving") ? (string)o["document"]["omschrijving"] : ""},
	    {"openbaarmakingsdatum", (string)o["document"]["openbaarmakingsdatum"]},
	    {"mutatiedatumtijd", (string)o["document"]["mutatiedatumtijd"]},
	    {"publisher",(string)o["document"]["publisher"]},
	    {"aanbieder",(string)o["document"]["aanbieder"]},
	    {"bestandsType", (string)o["bestandsType"]}
	    
	  }, "OOEntry");
	}
	else
	  known++;
      }
      start += resultaten.size();
      
    }
  done:;
    t += 86400;
  }
  cout<<"Got " << newentries <<" new entries, saw "<<known<<" that we knew about already"<<endl;

  auto entries = sqlw.queryT("select ooentry.id as id from OOEntry left join OODocument on ooentry.id = oodocument.id where oodocument.id is null;");
  int count=1;
  for(const auto& e : entries) {
    try {      
      string id = eget(e, "id");
      cout<<"Trying to get "<<id<<" ("<<count<<"/"<<entries.size()<<"): ";
      count++;
      cout.flush();
      sleep(1);
      auto res = cli.Get("https://open.overheid.nl/overheid/openbaarmakingen/api/v0/zoek/"+id);
      
      if(!res) {
	auto err = res.error();
	throw runtime_error("Oops retrieving: "+httplib::to_string(err));
      }
      
      //      cout<<"HTTP status: "<<res->status<<endl;
      //    cout<<"Got: '"<<res->body.c_str()<<"'" <<endl;
      
      nlohmann::json details = nlohmann::json::parse(res->body.c_str());
      
      auto document=details["document"];
      if(details["versies"][0]["bestanden"].empty()) {
	cout<<"No bestanden, skipping"<<endl;
	continue;
      }
      
      string mutatiedatumtijd = details["versies"][0]["mutatiedatumtijd"];
      auto bestand = details["versies"][0]["bestanden"][0];
      string openbaarmakingsdatum = details["versies"][0]["openbaarmakingsdatum"];
      string bestandsnaam = bestand["bestandsnaam"];
      string bestandsid = bestand["id"];
      
      string hash = bestand["hash"]; // this appears to be SHA1
      string verantwoordelijke=document["verantwoordelijke"]["label"];
      int64_t grootte = bestand["grootte"];
      string contentType = bestand["mime-type"];
      string weblocatie;
      if(document.count("weblocatie"))
	 weblocatie = document["weblocatie"];
	 
      string titel;
      if(document.count("titelcollectie"))
	titel=document["titelcollectie"]["officieleTitel"];
      
      string icategorieen, dsoorten;
      if(document.count("classificatiecollectie")) {
	nlohmann::json j = nlohmann::json::array();
	auto informatiecategorieen = document["classificatiecollectie"]["informatiecategorieen"];
	for(const auto& i : informatiecategorieen) {
	  j.push_back(i["label"]);
	}
	icategorieen = j.dump();
	
	j = nlohmann::json::array();
	auto docsoorten = document["classificatiecollectie"]["documentsoorten"];
	for(const auto& i : docsoorten) {
	  j.push_back(i["label"]);
	}
	dsoorten = j.dump();
      }
      
      vector<string> omschr = document["omschrijvingen"];
      string omschrijvingen = fmt::format("{}", omschr);
      
      string bronDocument;
      if(!details["documentrelaties"].empty()) { // COULD BE MULTIPLE
	// parent
	if(details["documentrelaties"][0]["role"]=="https://identifier.overheid.nl/tooi/def/thes/kern/c_4d1ea9ba") {
	  string tmp=details["documentrelaties"][0]["relation"];
	  string prefix="https://open.overheid.nl/documenten/";
	  if(tmp.find(prefix)==0)
	    bronDocument=tmp.substr(prefix.length());
	}
      }
      cout << bestandsnaam <<" "<< mutatiedatumtijd <<" " << titel<<" "<<hash <<endl;
      
      sqlw.addOrReplaceValue( {
	  {"id", id},
	  {"mutatiedatumtijd", mutatiedatumtijd},
	  {"openbaarmakingsdatum", openbaarmakingsdatum},
	  {"bestandsnaam", bestandsnaam},
	  {"bestandsid", bestandsid},
	  {"bronDocument", bronDocument},
	  {"titel", titel},
	  {"verantwoordelijke", verantwoordelijke},
	  {"omschrijvingen", omschrijvingen},
	  {"hash", hash},
	  {"grootte", grootte},
	  {"informatiecategorieen", icategorieen},
	  {"documentsoorten", dsoorten},
	  {"json", details.dump()},
	  {"versie", (int64_t)details["versies"][0]["nummer"]},
	  {"weblocatie", weblocatie},
	  {"contentType", contentType}
	  
	}, "OODocument");
    }
    catch(std::exception& e) {
      cout<<"Error: "<<e.what()<<endl;
    }
  }
}





#if 0
  struct NewId
  {
    string openbaarmakingsdatum;
    string id;
    string origMutationTime;
    bool operator<(const NewId& rhs) const
    {
      return tie(openbaarmakingsdatum, id, origMutationTime)
	< tie(rhs.openbaarmakingsdatum, rhs.id, rhs.origMutationTime);
    }
  };
  
  set<NewId> newids;


  done:
    cout<<"Have "<<newids.size()<<" OO document ids to query!"<<endl;
    
    // https://open.overheid.nl/overheid/openbaarmakingen/api/v0/zoek/eef2edec-019e-4085-8e2e-4d3f42686431
    
    for(const auto& [openbaarmakingsdatum, id, origMutationDateTime] : newids) {
      cout<<"Trying to get "<<id<<": ";
      cout.flush();
      sleep(1);
      auto res = cli.Get("https://open.overheid.nl/overheid/openbaarmakingen/api/v0/zoek/"+id);
      
      if(!res) {
	auto err = res.error();
	throw runtime_error("Oops retrieving: "+httplib::to_string(err));
      }
      
      //      cout<<"HTTP status: "<<res->status<<endl;
      //    cout<<"Got: '"<<res->body.c_str()<<"'" <<endl;
      
      nlohmann::json details = nlohmann::json::parse(res->body.c_str());
      try {      
	auto document=details["document"];
	if(details["versies"][0]["bestanden"].empty()) {
	  cout<<"No bestanden, skipping"<<endl;
	  continue;
	}
	
	string mutatiedatumtijd = details["versies"][0]["mutatiedatumtijd"];
	auto bestand = details["versies"][0]["bestanden"][0];
	
	string bestandsnaam = bestand["bestandsnaam"];
	string bestandsid = bestand["id"];
	
	string hash = bestand["hash"]; // this appears to be SHA1
	string verantwoordelijke=document["verantwoordelijke"]["label"];
	int64_t grootte = bestand["grootte"];
	string contentType = bestand["mime-type"];
	
	string titel;
	if(document.count("titelcollectie"))
	  titel=document["titelcollectie"]["officieleTitel"];
	
	string icategorieen, dsoorten;
	if(document.count("classificatiecollectie")) {
	  nlohmann::json j = nlohmann::json::array();
	  auto informatiecategorieen = document["classificatiecollectie"]["informatiecategorieen"];
	  for(const auto& i : informatiecategorieen) {
	    j.push_back(i["label"]);
	  }
	  icategorieen = j.dump();
	  
	  j = nlohmann::json::array();
	  auto docsoorten = document["classificatiecollectie"]["documentsoorten"];
	  for(const auto& i : docsoorten) {
	    j.push_back(i["label"]);
	  }
	  dsoorten = j.dump();
	}
	
	vector<string> omschr = document["omschrijvingen"];
	string omschrijvingen = fmt::format("{}", omschr);
	
	string bronDocument;
	if(!details["documentrelaties"].empty()) { // COULD BE MULTIPLE
	  // parent
	  if(details["documentrelaties"][0]["role"]=="https://identifier.overheid.nl/tooi/def/thes/kern/c_4d1ea9ba") {
	    string tmp=details["documentrelaties"][0]["relation"];
	    string prefix="https://open.overheid.nl/documenten/";
	    if(tmp.find(prefix)==0)
	      bronDocument=tmp.substr(prefix.length());
	  }
	}
	cout << bestandsnaam <<" "<< mutatiedatumtijd <<" " << titel<<" "<<hash <<endl;
	
	sqlw.addValue( {
	    {"id", id},
	    {"mutatiedatumtijd", origMutationDateTime},
	    {"openbaarmakingsdatum", openbaarmakingsdatum},
	    {"bestandsnaam", bestandsnaam},
	    {"bestandsid", bestandsid},
	    {"bronDocument", bronDocument},
	    {"titel", titel},
	    {"verantwoordelijke", verantwoordelijke},
	    {"omschrijvingen", omschrijvingen},
	    {"hash", hash},
	    {"grootte", grootte},
	    {"informatiecategorieen", icategorieen},
	    {"documentsoorten", dsoorten},
	    {"json", details.dump()},
	    {"versie", (int64_t)details["versies"][0]["nummer"]},
	    {"contentType", contentType},
	    {"ophaaldatumtijd", getDBDateTimeString(time(nullptr))}
	    
	  }, "OODocument");
	
      }
      catch(std::exception& e) {
	cerr<<"Problem with "<<e.what()<< " with "<<res->body<<endl;
      }
    }
    #endif


// the Rijksoverheid route
// check the RSS feed on https://feeds.rijksoverheid.nl/kamerstukken.rss
// visit the pages, a sample is https://www.rijksoverheid.nl/documenten/kamerstukken/2025/01/16/kamerbrief-bij-standen-van-de-uitvoering-2023
// harvest links like these: https://open.overheid.nl/documenten/0e1baa05-7d67-4087-9600-0d48f903020e/file


/*
int main(int argc, char** argv)
{
  httplib::Client cli("https://feeds.rijksoverheid.nl");
  cli.set_connection_timeout(10, 0); 
  cli.set_read_timeout(10, 0); 
  cli.set_write_timeout(10, 0); 

  SQLiteWriter sqlw("ro.sqlite3");
  
  auto res = cli.Get("https://feeds.rijksoverheid.nl/kamerstukken.rss");
  
  if(!res) {
    auto err = res.error();
    throw runtime_error("Oops retrieving: "+httplib::to_string(err));
  }

  pugi::xml_document doc;
  if (!doc.load_string(res->body.c_str())) {
    cout<<"Could not load XML"<<endl;
    return -1;
  }

  // rss / channel / item
  auto channel = doc.child("rss").child("channel");
  for(auto& item  : channel.children("item")) {
    string link = item.child("link").child_value();
    string title = item.child("title").child_value();
    // <pubDate>Fri, 17 Jan 2025 10:09:41 GMT</pubDate>
    string pubdate = item.child("pubDate").child_value();
    string datum = pubdate;
    string description = item.child("description").child_value();
    cout<< title << endl;
    try {
      auto res = sqlw.queryT("select count(1) c from RijksoverheidDocument where page=?", {link});
      if(get<int64_t>(res[0]["c"])) {
	cout<<"\tLooked at "<<link<<" already, skipping\n";
	continue;
      }
    }catch(...){}

    string page = getUrl(link);
                   // https://open.overheid.nl/documenten/0e1baa05-7d67-4087-9600-0d48f903020e/file
    regex url_regex("https://open.overheid.nl/documenten/[^\"]*");
    auto urls_begin = 
        std::sregex_iterator(page.begin(), page.end(), url_regex);
    auto urls_end = std::sregex_iterator();

    if(urls_begin == urls_end) {
      cout<<"\tFound no documents in page "<<link<<"\n";
      continue;
    }
    for (std::sregex_iterator i = urls_begin; i != urls_end; ++i) {
      string url = i->str();
      cout<<"\tAttempting to retrieve "<<url<<".. ";
      cout.flush();
      string doc = getUrl(url);
      cout<<"Got "<<doc.size()<<" bytes"<<endl;

      // store in database the pretty url and the enclosure url and statistics AFTER retrieving and storing
      // use part after 'documenten' as key:
      //   0123456789012345678901234567890123456
      //   https://open.overheid.nl/documenten/dpc-dad5bffb9cc22b26c28afb7cf54dfd84c3c0bda5/pdf

      string docid = url.substr(36);
      auto pos = docid.find('/');
      if(pos == string::npos) {
	cout<<"Weird docid, can't parse '"<<docid<<"'\n";
	continue;
      }

      docid.resize(pos);
      cout<<"url: "<<url<<", docid: '"<<docid<<"'\n";
      storeRODocument(docid, "", doc);
      
      sqlw.addValue({
	  {"id", docid},
	  {"datum", datum},
	  {"page", link},
	  {"url", i->str()}, 
	  {"contentLength", (int64_t)doc.size()}, 
	  {"titel", title},
	  {"omschrijving", description}
	  }, "RijksoverheidDocument");
    }
    //    break;
  }
  
}
*/
