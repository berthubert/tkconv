#define CPPHTTPLIB_USE_POLL
#define CPPHTTPLIB_THREAD_POOL_COUNT 32
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/os.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>
#include <iostream>
#include <map>
#include "httplib.h"
#include "sqlwriter.hh"
#include "argparse/argparse.hpp"
#include <sys/resource.h>
#include "jsonhelper.hh"
#include "support.hh"
#include "pugixml.hpp"
#include "inja.hpp"
#include "thingpool.hh"
#include "sws.hh"

using namespace std;
void addTkUserManagement(SimpleWebSystem& sws);

template<class UnaryFunction>
void recursive_iterate(nlohmann::json& j, UnaryFunction f)
{
    for(auto it = j.begin(); it != j.end(); ++it)
    {
        if (it->is_structured())
        {
            recursive_iterate(*it, f);
        }
        else
        {
            f(it);
        }
    }
}

void bulkEscape(nlohmann::json& j)
{
  recursive_iterate(j, [](auto& item) {
    if(item->is_string()) {
      (*item) = htmlEscape(*item);
    }
  });
}

static string getReasonableJPEG(const std::string& id)
{
  if(isPresentNonEmpty(id, "photoscache", ".jpg") && cacheIsNewer(id, "photoscache", ".jpg", "photos")) {
    string fname = makePathForId(id, "photoscache", ".jpg");
    FILE* pfp = fopen(fname.c_str(), "r");
    if(!pfp)
      throw runtime_error("Unable to get cached photo "+id+": "+string(strerror(errno)));
    
    shared_ptr<FILE> fp(pfp, fclose);
    char buffer[4096];
    string ret;
    for(;;) {
      int len = fread(buffer, 1, sizeof(buffer), fp.get());
      if(!len)
	break;
      ret.append(buffer, len);
    }
    if(!ferror(fp.get())) {
      fmt::print("Had a cache hit for {} photo\n", id);
      return ret;
    }
    // otherwise fall back to normal process
  }
  // 
  string fname = makePathForId(id, "photos");
  string command = fmt::format("convert -resize 400 -format jpeg - - < '{}'",
			  fname);
  FILE* pfp = popen(command.c_str(), "r");
  if(!pfp)
    throw runtime_error("Unable to perform conversion for '"+command+"': "+string(strerror(errno)));
  
  shared_ptr<FILE> fp(pfp, pclose);
  char buffer[4096];
  string ret;
  for(;;) {
    int len = fread(buffer, 1, sizeof(buffer), fp.get());
    if(!len)
      break;
    ret.append(buffer, len);
  }
  if(ferror(fp.get()))
    throw runtime_error("Unable to perform pandoc: "+string(strerror(errno)));

  string rsuffix ="."+to_string(getRandom64());
  string oname = makePathForId(id, "photoscache", "", true);
  {
    auto out = fmt::output_file(oname+rsuffix);
    out.print("{}", ret);
  }
  if(rename((oname+rsuffix).c_str(), (oname+".jpg").c_str()) < 0) {
    unlink((oname+rsuffix).c_str());
    fmt::print("Rename of cached JPEG failed\n");
  }
  
  return ret;
}

static string getContentsOfFile(const std::string& fname)
{
  FILE* pfp = fopen(fname.c_str(), "r");
  if(!pfp)
    throw runtime_error("Unable to get document "+fname+": "+string(strerror(errno)));
  
  shared_ptr<FILE> fp(pfp, fclose);
  char buffer[4096];
  string ret;
  for(;;) {
    int len = fread(buffer, 1, sizeof(buffer), fp.get());
    if(!len)
      break;
    ret.append(buffer, len);
  }
  if(!ferror(fp.get())) {
    return ret;
  }
  return "";
}

// for verslag XML, this makes html w/o <html> etc, for use in a .div
static string getHtmlForDocument(const std::string& id, bool bare=false)
{
  string suffix = bare ? ".div" : ".html";
  if(isPresentNonEmpty(id, "doccache", suffix) && cacheIsNewer(id, "doccache", suffix, "docs")) {
    string fname = makePathForId(id, "doccache", suffix);
    string ret = getContentsOfFile(fname);
    fmt::print("Cache hit in {} for {}, bare={}\n", __FUNCTION__, id, bare);
    if(!ret.empty())
      return ret;
    // otherwise fall back to normal process
  }
  
  string fname = makePathForId(id);
  string command;

  if(isDocx(fname))
    command = fmt::format("pandoc {} -f docx   --embed-resources  --variable maxwidth=72em -t html '{}'",
			  bare ? "" : "-s", fname);
  else if(isRtf(fname))
    command = fmt::format("pandoc {} -f rtf   --embed-resources  --variable maxwidth=72em -t html '{}'",
			  bare ? "" : "-s",
			  fname);
  else if(isDoc(fname))
    command = fmt::format("echo '<pre>' ; catdoc < '{}'; echo '</pre>'",
			  fname);
  else if(isXML(fname))
    command = fmt::format("xmlstarlet tr tk-div.xslt < '{}'",
			  fname);
  else
    command = fmt::format("pdftohtml {} {} -dataurls -stdout", bare ? "": "-s", fname);

  fmt::print("Command: {} {} \n", command, isXML(fname));
  FILE* pfp = popen(command.c_str(), "r");
  if(!pfp)
    throw runtime_error("Unable to perform conversion for '"+command+"': "+string(strerror(errno)));
  
  shared_ptr<FILE> fp(pfp, pclose);
  char buffer[4096];
  string ret;
  for(;;) {
    int len = fread(buffer, 1, sizeof(buffer), fp.get());
    if(!len)
      break;
    ret.append(buffer, len);
  }
  if(ferror(fp.get()))
    throw runtime_error("Unable to perform pdftotext: "+string(strerror(errno)));

  if(bare) { // xmlstarlet somehow always emits this
    string remove="<!DOCTYPE html>\n";
    if(ret.substr(0, remove.size())==remove)
      ret = ret.substr(remove.size());      
  }
  
  string rsuffix ="."+to_string(getRandom64());
  string oname = makePathForId(id, "doccache", "", true);
  {
    auto out = fmt::output_file(oname+rsuffix);
    out.print("{}", ret);
  }
  if(rename((oname+rsuffix).c_str(), (oname+suffix).c_str()) < 0) {
    unlink((oname+rsuffix).c_str());
    fmt::print("Rename of cached HTML failed\n");
  }
  return ret;
}

static string getRawDocument(const std::string& id)
{
  string fname;
  if(isPresentNonEmpty(id, "improvdocs")) {
    fmt::print("we have a better version of {}\n", id);
    fname = makePathForId(id, "improvdocs");
  }
  else
    fname = makePathForId(id);
  string ret = getContentsOfFile(fname);
  if(ret.empty())
     throw runtime_error("Unable to get raw document: "+string(strerror(errno)));
  return ret;
}

struct VoteResult
{
  set<string> voorpartij, tegenpartij, nietdeelgenomenpartij;
  int voorstemmen=0, tegenstemmen=0, nietdeelgenomen=0;
};

static string getPartyFromNumber(SQLiteWriter& sqlw, int nummer)
{
  auto party = sqlw.queryT("select afkorting, persoon.functie from Persoon,fractiezetelpersoon,fractiezetel,fractie where persoon.nummer=? and  persoonid=persoon.id and fractiezetel.id=fractiezetelpersoon.fractiezetelid and fractie.id=fractiezetel.fractieid order by fractiezetelpersoon.van desc limit 1", {nummer});
  if(party.empty())
    return "";

  if(get<string>(party[0]["functie"]) != "Tweede Kamerlid")
    return "Ooit " +std::get<string>(party[0]["afkorting"])+ " kamerlid";
  else
    return std::get<string>(party[0]["afkorting"]);
}

bool getVoteDetail(SQLiteWriter& sqlw, const std::string& besluitId, VoteResult& vr)
{
  // er is een mismatch tussen Stemming en Persoon, zie https://github.com/TweedeKamerDerStaten-Generaal/OpenDataPortaal/issues/150
  // dus we gebruiken die tabel maar niet hier
  auto votes = sqlw.queryT("select persoonId,soort,actorFractie,fractieGrootte,actorNaam from Stemming where besluitId=?", {besluitId});
  if(votes.empty())
    return false;
  //  cout<<"Got "<<votes.size()<<" votes for "<<besluitId<<endl;
  bool hoofdelijk = false;
  if(!get<string>(votes[0]["persoonId"]).empty()) {
    //fmt::print("Hoofdelijke stemming!\n");
    hoofdelijk=true;
  }
  
  for(auto& v : votes) {
    string soort = get<string>(v["soort"]);
    string partij = get<string>(v["actorFractie"]);
    int zetels = get<int64_t>(v["fractieGrootte"]);
    if(soort == "Voor") {
      if(hoofdelijk) {
	vr.voorstemmen++;
	vr.voorpartij.insert(get<string>(v["actorNaam"]));
      }
      else {
	vr.voorstemmen += zetels;
	vr.voorpartij.insert(partij);
      }
    }
    else if(soort == "Tegen") {
      if(hoofdelijk) {
	vr.tegenstemmen++;
	vr.tegenpartij.insert(get<string>(v["actorNaam"]));
      }
      else {
	vr.tegenstemmen += zetels;
	vr.tegenpartij.insert(partij);
      }
    }
    else if(soort=="Niet deelgenomen") {
      if(hoofdelijk) {
	vr.nietdeelgenomen++;
	vr.nietdeelgenomenpartij.insert(get<string>(v["actorNaam"]));
      }
      else {
	vr.nietdeelgenomen+= zetels;
	vr.nietdeelgenomenpartij.insert(partij);
      }
    }
  }
  return true;
}



// this processes .odt from officielepublicaties and turns it into HTML
std::string getBareHtmlFromExternal(const std::string& id)
{
  if(id.find_first_of("./") != string::npos)
    throw runtime_error("external id contained illegal characters");

  if(haveExternalIdFile(id, "opcache", ".html")) {
    string ret = getContentsOfFile(makePathForExternalID(id, "opcache", ".html"));
    if(!ret.empty()) {
      fmt::print("Got cache hit for external content {}!\n", id);
      return ret;
    }
  }

  string command = fmt::format("pandoc -f odt op/{}/{}.odt -t html --embed-resources -t html",
			       getSubdirForExternalID(id),
			       id);
  FILE* pfp = popen(command.c_str(), "r");
  if(!pfp)
    throw runtime_error("Unable to perform conversion for '"+command+"': "+string(strerror(errno)));
  
  shared_ptr<FILE> fp(pfp, pclose);
  char buffer[4096];
  string ret;
  for(;;) {
    int len = fread(buffer, 1, sizeof(buffer), fp.get());
    if(!len)
      break;
    ret.append(buffer, len);
  }
  if(ferror(fp.get()))
    throw runtime_error("Unable to perform odt to html: "+string(strerror(errno)));

  string rsuffix ="."+to_string(getRandom64());
  string oname = makePathForExternalID(id, "opcache", ".html", true);
  {
    auto out = fmt::output_file(oname+rsuffix);
    out.print("{}", ret);
  }
  if(rename((oname+rsuffix).c_str(), (oname).c_str()) < 0) {
    int t = errno;
    unlink((oname+rsuffix).c_str());
    fmt::print("Rename of cached ODT->HTML failed: {}\n", strerror(t));
  }
  
  return ret;
}


auto getVerslagen(SQLiteWriter& sqlw, int days)
{
  string f = fmt::format("{:%Y-%m-%d}", fmt::localtime(time(0) - days*86400));
  auto verslagen = sqlw.queryT("select * from vergadering,verslag where verslag.vergaderingid=vergadering.id and datum > ? and status != 'Casco' order by datum desc, verslag.updated desc", {f});
  
  set<string> seen;
  decltype(verslagen) tmp;
  for(auto& v: verslagen) {
    string vid = get<string>(v["vergaderingId"]);
    if(seen.count(vid))
      continue;
    string datum = get<string>(v["datum"]);
    datum.resize(10);
    v["datum"] = datum;
    string aanvangstijd = get<string>(v["aanvangstijd"]);
    v["aanvangstijd"] = aanvangstijd.substr(11, 5);
    tmp.push_back(v);
    
    seen.insert(vid);
  }
  sort(tmp.begin(), tmp.end(), [](auto&a, auto&b)
  {
    return std::tie(get<string>(a["datum"]), get<string>(a["aanvangstijd"])) >
      std::tie(get<string>(b["datum"]), get<string>(b["aanvangstijd"]));
  });
  return tmp;
}

string onderwerpToFilename(const std::string& onderwerp)
{
  string ret = onderwerp;
  for(auto& c: ret) {
    if(isalnum(c))
      continue;
    else c='-';
  }
  return ret;
}

struct Stats
{
  atomic<uint64_t> hits = 0;
  atomic<uint64_t> etagHits = 0;
  atomic<uint64_t> searchUsec = 0;
  atomic<uint64_t> http2xx = 0;
  atomic<uint64_t> http3xx = 0;
  atomic<uint64_t> http4xx = 0;
  atomic<uint64_t> http5xx = 0;
  atomic<uint64_t> usec = 0;
  array<atomic<uint64_t>, 12> lats;
} g_stats;


time_t g_starttime;

int main(int argc, char** argv)
{
  g_starttime = time(0);
  argparse::ArgumentParser args("tkserv", "0.0");

  args.add_argument("--rnd-admin-password").help("Create admin user if necessary, and set a random password").default_value(string(""));
  args.add_argument("--insecure-cookie").help("Use an insecure cookie, for non-https operations").default_value(string(""));
  try {
    args.parse_args(argc, argv);
  }
  catch (const std::runtime_error& err) {
    std::cout << err.what() << std::endl << args;
    std::exit(1);
  }

  
  ThingPool<SQLiteWriter> tp("tk.sqlite3", SQLWFlag::ReadOnly);
  
  signal(SIGPIPE, SIG_IGN); // every TCP application needs this
  SQLiteWriter userdb("user.sqlite3",
		      {
			{"users", {{"user", "PRIMARY KEY"}, {"email", "collate nocase"}}},
			{"sessions", {{"user", "NOT NULL REFERENCES users(user) ON DELETE CASCADE"}}},
			{"scanners", {{"userid", "NOT NULL REFERENCES users(user) ON DELETE CASCADE"}}}
		      }
		      );

  try {
    userdb.query("create index if not exists timsiidx on users(timsi)");
  }
  catch(...){}
    
  
  std::mutex userdblock;
  LockedSqw ulsqw{userdb, userdblock};
  
  SimpleWebSystem sws(tp, ulsqw);
  sws.d_svr.set_keep_alive_max_count(1); 
  sws.d_svr.set_keep_alive_timeout(1);
  sws.standardFunctions();
  addTkUserManagement(sws);
  
  
  if(args.is_used("--rnd-admin-password")) {
    bool changed=false;
    string pw = getLargeId();

    try {
      if(sws.d_users.userHasCap("admin", Capability::Admin)) {
        cout<<"Admin user existed already, updating password to: "<< pw << endl;
        sws.d_users.changePassword("admin", pw);
        changed=true;
      }
    }
    catch(...) {
    }

    if(!changed) {
      fmt::print("Creating user admin with password: {}\n", pw);
      sws.d_users.createUser("admin", pw, "", true);
    }
    if(args.get<string>("rnd-admin-password") != "continue")
      return EXIT_SUCCESS;
  }
  
  // hoe je de fractienaam krijgt bij een persoonId:
  //  select f.afkorting from FractieZetelPersoon fzp,FractieZetel fz,Fractie f, Persoon p where fzp.fractieZetelId = fz.id and fzp.persoonId = p.id and p.id = ?
  
  sws.d_svr.Get("/getraw/:nummer", [&tp](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.path_params.at("nummer"); // 2023D41173
    cout<<"getraw nummer: "<<nummer<<endl;
    string id, onderwerp, contentType;
    auto sqlw = tp.getLease();
    auto ret=sqlw->queryT("select * from Document where nummer=? order by rowid desc limit 1", {nummer});

    if(ret.empty()) {
      ret = sqlw->queryT("select * from Verslag where id=? order by rowid desc limit 1", {nummer});
      if(ret.empty()) {
	res.set_content(fmt::format("Could not find a Verslag with id {}", id), "text/plain");
	return;
      }
      id=nummer;
    }
    else {
      id = get<string>(ret[0]["id"]);
      onderwerp = eget(ret[0], "onderwerp");
      contentType = eget(ret[0], "contentType");
    }

    sqlw.release();
    if(!onderwerp.empty()) {
      string fname=nummer+"-"+onderwerpToFilename(onderwerp);
      if(contentType =="application/vnd.openxmlformats-officedocument.wordprocessingml.document")
	fname+=".docx";
      else if(contentType=="application/pdf")
	fname += ".pdf";
      res.set_header("Content-Disposition",  fmt::format("inline; filename=\"{}\"", fname));
    }
    string content = getRawDocument(id);


    res.set_content(content, get<string>(ret[0]["contentType"]));
  });

  sws.d_svr.Get("/personphoto/:nummer", [&tp](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.path_params.at("nummer"); // 1234
    cout<<"persoon nummer: "<<nummer<<endl;
    auto ret=tp.getLease()->queryT("select * from Persoon where nummer=? order by rowid desc limit 1", {nummer});

    if(ret.empty()) {
      res.status = 404;
      res.set_content("No such persoon", "text/plain");
      return;
    }
    string id = get<string>(ret[0]["id"]);
    string content = getReasonableJPEG(id);
    res.set_content(content, "image/jpeg");
  });

  sws.d_svr.Get("/sitemap-(20\\d\\d).txt", [&tp](const auto& req, auto& res) {
    auto sqlw=tp.getLease();
    string year = req.matches[1];
    year += "-%";
    auto nums=sqlw->queryT("select nummer from Document where datum like ?", {year});
    string resp;
    for(auto& n : nums) {
      resp += fmt::format("https://berthub.eu/tkconv/document.html?nummer={}\n", get<string>(n["nummer"]));
    }
    nums=sqlw->queryT("select vergadering.id from vergadering,verslag where vergaderingid=vergadering.id and status != 'Casco' and datum like ? group by vergadering.id", {year});
    for(auto& n : nums) {
      resp += fmt::format("https://berthub.eu/tkconv/verslag.html?vergaderingid={}\n", get<string>(n["id"]));
    }
    
    res.set_content(resp, "text/plain");
  });

  sws.d_svr.Get("/sitemap-(20\\d\\d)-H([12]).txt", [&tp](const auto& req, auto& res) {
    auto sqlw=tp.getLease();
    string year = req.matches[1];
    string h = req.matches[2];

    string fromDate, toDate;
    if(h=="1") {
      fromDate = year+"-01-01";
      toDate = year+"-07-01";
    }
    if(h=="2") {
      fromDate = year+"-07-02";
      toDate = year+"-12-31";
    }
    cout<<"fromDate: "<<fromDate<<", toDate: "<<toDate<<endl;
    
    auto nums=sqlw->queryT("select nummer from Document where datum >= ? and datum <= ?", {fromDate, toDate});
    string resp;
    for(auto& n : nums) {
      resp += fmt::format("https://berthub.eu/tkconv/document.html?nummer={}\n", get<string>(n["nummer"]));
    }
    nums=sqlw->queryT("select vergadering.id from vergadering,verslag where vergaderingid=vergadering.id and status != 'Casco' and datum >= ? and datum <= ? group by vergadering.id", {fromDate, toDate});
    for(auto& n : nums) {
      resp += fmt::format("https://berthub.eu/tkconv/verslag.html?vergaderingid={}\n", get<string>(n["id"]));
    }
    
    res.set_content(resp, "text/plain");
  });


  
  // officiele publicatie redirect
  sws.d_svr.Get("/op/:extid", [&tp](const httplib::Request &req, httplib::Response &res) {
    string extid=req.path_params.at("extid"); 
    auto docs = tp.getLease()->queryT("select nummer from documentversie,document where externeidentifier=? and documentversie.documentid=document.id", {extid});
    if(docs.empty()) {
      res.status = 404;
      res.set_content(fmt::format("No such external identifier {}", extid), "text/plain");
      return;
    }
    string dest = get<string>(docs[0]["nummer"]);
    res.status = 301;
    res.set_header("Location", "../document.html?nummer="+dest);
  });
  
  sws.d_svr.Get("/sitemap-(20\\d\\d-\\d\\d).txt", [&tp](const auto& req, auto& res) {
    auto sqlw = tp.getLease();
    string year = req.matches[1];
    year += "-%";
    auto nums=sqlw->queryT("select nummer from Document where datum like ?", {year});
    string resp;
    for(auto& n : nums) {
      resp += fmt::format("https://berthub.eu/tkconv/document.html?nummer={}\n", get<string>(n["nummer"]));
    }
    nums = sqlw->queryT("select vergadering.id from vergadering,verslag where vergaderingid=vergadering.id and status != 'Casco' and datum like ? group by vergadering.id", {year});
    for(auto& n : nums) {
      resp += fmt::format("https://berthub.eu/tkconv/verslag.html?vergaderingid={}\n", get<string>(n["id"]));
    }
    res.set_content(resp, "text/plain");
  });

  
  sws.d_svr.Get("/jarig-vandaag", [&tp](const httplib::Request &req, httplib::Response &res) {
    string f = fmt::format("{:%%-%m-%d}", fmt::localtime(time(0)));
    auto sqlw = tp.getLease();
    auto jarig = packResultsJson(sqlw->queryT("select geboortedatum,roepnaam,initialen,tussenvoegsel,achternaam,afkorting,persoon.nummer from Persoon,fractiezetelpersoon,fractiezetel,fractie where geboortedatum like ? and persoon.functie ='Tweede Kamerlid' and  persoonid=persoon.id and fractiezetel.id=fractiezetelpersoon.fractiezetelid and fractie.id=fractiezetel.fractieid order by achternaam, roepnaam", {f}));
    res.set_content(jarig.dump(), "application/json");
    return;
  });

  sws.d_svr.Get("/commissie/:id", [&tp](const httplib::Request &req, httplib::Response &res) {
    string id = req.path_params.at("id");
    auto sqlw = tp.getLease();
    nlohmann::json j = nlohmann::json::object();

    auto comm = sqlw->queryT("select Commissie.naam, commissie.afkorting cafkorting from Commissie where id=?", {id});
    if(comm.empty())
      return;
    j["commissie"] = packResultsJson(comm)[0];
    
    j["leden"] = packResultsJson(sqlw->queryT("select Commissie.naam, commissie.afkorting cafkorting, commissiezetel.gewicht, CommissieZetelVastPersoon.functie cfunctie, persoon.*, fractie.afkorting fafkorting from Commissie,CommissieZetel,CommissieZetelVastPersoon, Persoon,fractiezetelpersoon,fractiezetel,fractie where Persoon.id=commissiezetelvastpersoon.PersoonId and CommissieZetel.commissieId = Commissie.id and CommissieZetelVastpersoon.CommissieZetelId = commissiezetel.id and fractiezetel.id=fractiezetelpersoon.fractiezetelid and fractie.id=fractiezetel.fractieid and fractiezetelpersoon.persoonId = Persoon.id and commissie.id=? and fractie.datumInactief='' and fractiezetelpersoon.totEnMet='' and CommissieZetelVastPersoon.totEnMet='' order by commissiezetel.gewicht", {id})); 

    j["zaken"] = packResultsJson(sqlw->queryT("select * from ZaakActor,Zaak where commissieId=? and Zaak.id=ZaakActor.zaakid order by gestartOp desc limit 20", {id}));

    j["activiteiten"] = packResultsJson(sqlw->queryT("select Activiteit.*, 'Voortouwcommissie' as relatie from Activiteit,Commissie where commissie.id=? and voortouwnaam like commissie.naam  union all select Activiteit.*, relatie from ActiviteitActor,Activiteit where commissieId=? and Activiteit.id=ActiviteitActor.activiteitid order by datum desc limit 50", {id, id}));
		   
    res.set_content(j.dump(), "application/json");    
  });

  
  sws.d_svr.Get("/persoon.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    int nummer = atoi(req.get_param_value("nummer").c_str());
    auto sqlw = tp.getLease();
    auto lid = packResultsJson(sqlw->queryT("select * from Persoon where persoon.nummer=?", {nummer}));
    if(lid.empty()) {
      res.status=404;
      res.set_content("Geen kamerlid met nummer "+to_string(nummer), "text/plain");
      return;
    }

    lid[0]["afkorting"] = getPartyFromNumber(sqlw.get(), nummer);
    nlohmann::json j = nlohmann::json::object();
    j["meta"] = lid[0];

    auto zaken = packResultsJson(sqlw->queryT("select substr(zaak.gestartOp,0,11) gestartOp, zaak.onderwerp, zaak.nummer, zaak.id from zaakactor,zaak where persoonid=? and relatie='Indiener' and zaak.id=zaakid order by gestartop desc, nummer desc", {(string)lid[0]["id"]}));

    for(auto& z: zaken) {
      z["aangenomen"]="";
      z["docs"] = packResultsJson(sqlw->queryT("select soort from link,document where link.naar=? and category='Document' and document.id=link.van order by datum", {(string)z["id"]}));

      auto besluiten = packResultsJson(sqlw->queryT("select datum, besluit.id,stemmingsoort,tekst from zaak,besluit,agendapunt,activiteit where zaak.nummer=? and besluit.zaakid = zaak.id and agendapunt.id=agendapuntid and activiteit.id=agendapunt.activiteitid order by datum asc", {(string)z["nummer"]}));

      for(auto& b : besluiten) {
	z["aangenomen"]=b["tekst"];
      }
    }
    j["zaken"] = zaken;
    auto gesproken = packResultsJson(sqlw->queryT("select vergaderingspreker.vergaderingid, substr(datum,0,11) datum,soort,zaal,titel,round(1.0*sum(seconden)/60,1) as minuten from VergaderingSpreker,VergaderingSprekerTekst,Persoon,Vergadering where vergadering.id=vergaderingspreker.vergaderingid and Persoon.id=vergaderingspreker.persoonId and persoon.nummer=? and vergaderingsprekertekst.persoonId = Persoon.id and vergaderingsprekertekst.vergaderingid = vergaderingspreker.vergaderingid group by 1 order by datum desc", {nummer}));
    for(auto& g : gesproken) {
      double mins = (double)g["minuten"];
      string minuten = fmt::sprintf("%.1f", mins);
      replaceSubstring(minuten, ".", ",");
      g["minuten"] = minuten;
    }
    
    j["gesproken"] = gesproken;

    j["activiteiten"] = packResultsJson(sqlw->queryT("select substr(activiteit.datum, 0, 11) datum, activiteit.onderwerp, activiteit.nummer, activiteit.voortouwNaam, activiteit.soort from ActiviteitActor,activiteit,persoon where persoon.nummer=? and activiteit.id=activiteitid and activiteitactor.persoonid = persoon.id order by datum desc", {nummer}));

    j["geschenken"] = packResultsJson(sqlw->queryT("select datum, substr(persoongeschenk.bijgewerkt,0,11) bijgewerkt, omschrijving from PersoonGeschenk,Persoon where persoon.id=persoonid and nummer=? order by gewicht", {nummer}));
    
    j["pagemeta"]["title"]="Kamerlid";
    string tv = lid[0]["tussenvoegsel"];
    if(!tv.empty())
      tv += ' ';
    
    j["og"]["title"] = (string)lid[0]["roepnaam"] + " "+tv + (string)lid[0]["achternaam"];
    j["og"]["description"] = (string)lid[0]["functie"];
    j["og"]["imageurl"] = "https://berthub.eu/tkconv/personphoto/"+to_string(nummer);

    inja::Environment e;
    e.set_html_autoescape(true);
    
    res.set_content(e.render_file("./partials/persoon.html", j), "text/html");
    return;
  });

  sws.wrapGet({}, "/mijn.html", [&tp](auto& cr) {
    nlohmann::json data;
    data["pagemeta"]["title"]="Mijn";
    data["og"]["title"] = "Mijn";
    data["og"]["description"] = "Mijn";
    data["og"]["imageurl"] = "";

    inja::Environment e;
    e.set_html_autoescape(true);
    // need to get timsi somehow
    auto timsi = cr.lsqw.query("select timsi from users where user=?", {cr.user});
    if(timsi.size()==1) {
      data["timsi"] = eget(timsi[0], "timsi");
    }
      
    return make_pair<string,string>(e.render_file("./partials/mijn.html", data), "text/html");
  });

  sws.wrapGet({}, "/search.html", [&tp](auto& cr) {
    string q = cr.req.get_param_value("q");
    nlohmann::json data;
    data["pagemeta"]["title"]="Zoek naar "+htmlEscape(q);
    data["og"]["title"] = "Zoek naar "+htmlEscape(q);
    data["og"]["description"] = "Zoek naar "+htmlEscape(q);
    data["og"]["imageurl"] = "";
    data["q"] = urlEscape(q);
    inja::Environment e;
    e.set_html_autoescape(false); // !!
      
    return make_pair<string,string>(e.render_file("./partials/search.html", data), "text/html");
  });


  sws.wrapGet({}, "/personen.html", [&tp](auto& cr) {
    nlohmann::json data;
    data["pagemeta"]["title"]="Alle personen";
    data["og"]["title"] = "Alle personen";
    data["og"]["description"] = "Alle personen";
    data["og"]["imageurl"] = "";

    auto personen = packResultsJson(tp.getLease()->queryT(R"(select min(van) voorheteerst, titels, persoon.nummer, roepnaam,tussenvoegsel,achternaam,json_group_array(distinct(afkorting)) fracties from persoon,fractiezetelpersoon,fractiezetel,fractie where  fractiezetelpersoon.persoonid = persoon.id and fractiezetelpersoon.fractiezetelid = fractiezetel.id and fractie.id=fractiezetel.fractieid group by persoon.id order by achternaam,roepnaam)"));

    for(auto& p : personen) {
      nlohmann::json a =  nlohmann::json::parse((string)p["fracties"]);
      p["fracties"] = a;
    }
    data["data"] = personen;
    
    inja::Environment e;
    e.set_html_autoescape(true); 
      
    return make_pair<string,string>(e.render_file("./partials/personen.html", data), "text/html");
  });


  
  sws.d_svr.Get("/zaak.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    string nummer = req.get_param_value("nummer");
    nlohmann::json z = nlohmann::json::object();
    auto sqlw = tp.getLease();
    auto zaken = sqlw->queryT("select *, substr(gestartOp, 0, 11) gestartOp from zaak where nummer=?", {nummer});
    if(zaken.empty()) {
      res.status = 404;
      return;
    }
    z["zaak"] = packResultsJson(zaken)[0];
    string zaakid = z["zaak"]["id"];

    z["actors"] = packResultsJson(sqlw->queryT("select *,zaakactor.functie functie from zaakactor left join persoon on persoon.id = zaakactor.persoonid where zaakid=?", {zaakid}));

    // Multi: {"activiteit", "agendapunt", "gerelateerdVanuit", "vervangenVanuit"}
    // Hasref: {"activiteit", "agendapunt", "gerelateerdVanuit", "kamerstukdossier", "vervangenVanuit"}

    z["activiteiten"] = packResultsJson(sqlw->queryT("select * from Activiteit,Link where Link.van=? and Activiteit.id=link.naar order by datum desc", {zaakid}));
    z["agendapunten"] = packResultsJson(sqlw->queryT("select Agendapunt.* from Agendapunt,Link where Link.van=? and Agendapunt.id=link.naar", {zaakid}));
    for(auto &d : z["agendapunten"]) {
      d["activiteit"] = packResultsJson(sqlw->queryT("select *, substr(aanvangstijd, 0, 17) aanvangstijd from Activiteit where id=?", {(string)d["activiteitId"]}))[0];
      string aanv = d["activiteit"]["aanvangstijd"];
      aanv[10]=' ';
      d["activiteit"]["aanvangstijd"]=aanv;
    }
    auto& ap = z["agendapunten"];
    sort(ap.begin(), ap.end(),
	 [](const auto& a, const auto& b) {
	   return a["activiteit"]["aanvangstijd"] > b["activiteit"]["aanvangstijd"];
	 }
	 );
    z["gerelateerd"] = packResultsJson(sqlw->queryT("select * from Zaak,Link where Link.van=? and Zaak.id=link.naar and linkSoort='gerelateerdVanuit'", {zaakid}));
    z["vervangenVanuit"] = packResultsJson(sqlw->queryT("select * from Zaak,Link where Link.van=? and Zaak.id=link.naar and linkSoort='vervangenVanuit'", {zaakid}));
    z["vervangenDoor"] = packResultsJson(sqlw->queryT("select * from Zaak,Link where Link.naar=? and Zaak.id=link.van and linkSoort='vervangenVanuit'", {zaakid}));
    
    z["docs"] = packResultsJson(sqlw->queryT("select Document.*, substr(datum, 0, 11) datum from Document,Link where Link.naar=? and Document.id=link.van order by datum desc", {zaakid}));
    for(auto &d : z["docs"]) {
      string docid = d["id"];
      d["actors"] = packResultsJson(sqlw->queryT("select * from DocumentActor where documentId=?", {docid}));
    }

    z["kamerstukdossier"]= packResultsJson(sqlw->queryT("select * from kamerstukdossier where id=?",
							{(string)z["zaak"]["kamerstukdossierId"]}));

    z["besluiten"] = packResultsJson(sqlw->queryT("select substr(datum,0,17) datum, besluit.id,besluit.status, stemmingsoort,tekst from zaak,besluit,agendapunt,activiteit where zaak.nummer=? and besluit.zaakid = zaak.id and agendapunt.id=agendapuntid and activiteit.id=agendapunt.activiteitid order by datum desc", {nummer}));
    
    for(auto& b : z["besluiten"]) {
      string datum = b["datum"];
      datum[10] = ' ';
      b["datum"] = datum;
      VoteResult vr;
      if(getVoteDetail(sqlw.get(), b["id"], vr)) {
	b["voorpartij"] = vr.voorpartij;
	b["tegenpartij"] = vr.tegenpartij;
	b["nietdeelgenomenpartij"] = vr.nietdeelgenomenpartij;
	b["voorstemmen"] = vr.voorstemmen;
	b["tegenstemmen"] = vr.tegenstemmen;
	b["nietdeelgenomenstemmen"] = vr.nietdeelgenomen;
      }
    }
    
    // XXX agendapunt multi
    z["pagemeta"]["title"]="Zaak";
    z["og"]["title"] = "Persoon";
    z["og"]["description"] = "Persoon";
    z["og"]["imageurl"] = "";

    inja::Environment e;
    e.set_html_autoescape(true);
    
    res.set_content(e.render_file("./partials/zaak.html", z), "text/html");
  });    

  sws.d_svr.Get("/activiteit/:nummer", [&tp](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.path_params.at("nummer"); // 2024A02517
    auto sqlw = tp.getLease();
    cout<<"/activiteit/:nummer: "<<nummer<<endl;

    auto ret=sqlw->queryT("select * from Activiteit where nummer=? order by rowid desc limit 1", {nummer});
    
    if(ret.empty()) {
      res.set_content("Found nothing!!", "text/plain");
      return;
    }
    nlohmann::json r = nlohmann::json::object();
    r["meta"] = packResultsJson(ret)[0];
    string activiteitId = r["meta"]["id"];

    
    auto persactors = sqlw->queryT("select ActiviteitActor.*, Persoon.nummer from ActiviteitActor left join Persoon on Persoon.id=ActiviteitActor.persoonId where activiteitId=? and activiteitactor.relatie='Deelnemer' order by volgorde", {activiteitId});

    auto comactors = sqlw->queryT("select ActiviteitActor.*, Commissie.id from ActiviteitActor left join Commissie on commissie.id=ActiviteitActor.commissieId where activiteitId=? and relatie like '%commissie%' order by volgorde", {activiteitId});
    
    r["persactors"] = packResultsJson(persactors);
    r["comactors"] = packResultsJson(comactors);
    auto zalen = packResultsJson(sqlw->queryT("select * from Reservering,Zaal where Reservering.activiteitId = ? and zaal.id = zaalId", {activiteitId}));
    string zaalnaam;
    if(!zalen.empty()) {
      zaalnaam = zalen[0]["naam"];
      r["zaal"] = zalen[0];
    }
    else
      r["zaal"]="";

    r["agendapunten"]= packResultsJson(sqlw->queryT("select * from Agendapunt where activiteitId = ? order by volgorde", {activiteitId}));

    
    for(auto& ap: r["agendapunten"]) {
      ap["docs"] = packResultsJson(sqlw->queryT("select * from Document where agendapuntid=?",
						{(string)ap["id"]}));
      ap["zdocs"] = packResultsJson(sqlw->queryT("select Document.* from link,link link2,zaak,document where link.naar=? and zaak.id=link.van and link2.naar = zaak.id and document.id=link2.van",  {(string)ap["id"]}));
    }

    r["docs"] = packResultsJson(sqlw->queryT("select Document.* from link,Document where linkSoort='Activiteit' and link.naar=? and Document.id=link.van", {activiteitId}));

    r["toezeggingen"] = packResultsJson(sqlw->queryT("select * from Toezegging where activiteitId=?", {activiteitId}));
    
    try {
      string url = fmt::format("https://cdn.debatdirect.tweedekamer.nl/search?van={}&tot={}&sortering=relevant&vanaf=0&appVersion=10.34.1&platform=web&totalFormat=new",
			       ((string)r["meta"]["datum"]).substr(0,10),
			       ((string)r["meta"]["datum"]).substr(0,10));
      
      httplib::Client cli("https://cdn.debatdirect.tweedekamer.nl");
      cli.set_connection_timeout(1, 0); 
      cli.set_read_timeout(1, 0); 
      cli.set_write_timeout(1, 0); 
      
      fmt::print("Retrieving from {}\n", url);
      auto httpres = cli.Get(url);
      if(!httpres) {
	auto err = httpres.error();
	fmt::print("Oops retrieving from {} -> {}", url, httplib::to_string(err));
	res.set_content(r.dump(), "application/json");
	return;
      }
      nlohmann::json j = nlohmann::json::parse(httpres->body);
      cout<<j.dump()<<endl;
      //    cout<<httpres->body<<endl;
      string videourl;
      cout<<"onderwerp: "<<(string)(r["meta"]["onderwerp"])<<endl;
      cout<<"datum: "<<(string)(r["meta"]["datum"])<<endl;
      cout<<"tijd: "<<(string)(r["meta"]["aanvangstijd"])<< " - " <<(string)(r["meta"]["eindtijd"])<<endl;
      // tijd: 2024-09-10T16:15:00 - 2024-09-10T16:50:00
      if(zaalnaam.empty())
	zaalnaam = "Plenaire zaal";
      cout<<"zaal: "<<zaalnaam<<endl;
      //    cout <<j["hits"]["hits"].dump()<<endl;
      
      /*
	"startsAt": "2024-09-17T13:00:00+0200",
	"endsAt": "2024-09-17T14:00:00+0200",
	"startedAt": "2024-09-17T13:24:08+0200",
	"endedAt": "2024-09-17T13:43:37+0200",
      */

      time_t tkmidtstamp = (getTstamp((string)(r["meta"]["aanvangstijd"])) + getTstamp((string)(r["meta"]["eindtijd"])))/2;
      time_t tklen = getTstamp((string)(r["meta"]["eindtijd"])) - getTstamp((string)(r["meta"]["aanvangstijd"]));
      
      std::multimap<time_t, nlohmann::json> candidates;
      for(auto& h : j["hits"]["hits"]) {
	auto d = h["_source"];
	time_t ddmidtstamp = (getTstamp((string)d["startsAt"]) + getTstamp((string)d["endsAt"]))/2;
	time_t ddlen = getTstamp((string)d["endsAt"]) - getTstamp((string)d["startsAt"]);
	double lenrat = (ddlen+1.)/(tklen+1.);
	if((string)d["locationName"]==zaalnaam && lenrat >0.1 && lenrat < 10.0)
	  candidates.insert({{abs(ddmidtstamp - tkmidtstamp)}, d});
	fmt::print("'{}' -> '{}' ({}) {} - {} {} {} {}\n",
		   (string)d["name"], (string)d["slug"], (string)d["locationName"],
		   (string)d["startsAt"], (string)d["endsAt"], lenrat,
		   tklen, ddlen
		   );
	
      }
      if(!candidates.empty()) {
	auto c =candidates.begin()->second;
	fmt::print("Best smart match: {} {}\n", (string)c["name"], (string)c["startsAt"]);
	videourl = "https://debatdirect.tweedekamer.nl/" + (string)c["debateDate"] + "/" + (string)c["categoryIds"][0] +"/"+(string)c["locationId"] +"/"+(string)c["slug"];

      }
      r["videourl"]=videourl;
    }
    catch(exception& e) {
      fmt::print("Error getting debatdirect link: {}\n", e.what());
    }
    res.set_content(r.dump(), "application/json");
  });


  auto doTemplate = [&](const string& name, const string& file, const string& q = string()) {
    sws.d_svr.Get("/"+name+"(/?.*)", [&tp, name, file, q](const httplib::Request &req, httplib::Response &res) {
      inja::Environment e;
      e.set_html_autoescape(true);
      nlohmann::json data;
      if(!q.empty())
	data["data"] = packResultsJson(tp.getLease()->queryT(q));
      
      data["pagemeta"]["title"]="";
      data["og"]["title"] = name;
      data["og"]["description"] = name;
      data["og"]["imageurl"] = "";

      res.set_content(e.render_file("./partials/"+file, data), "text/html");
    });
  };


  doTemplate("kamerstukdossiers.html", "kamerstukdossiers.html");
  doTemplate("vragen.html", "vragen.html"); // unlisted
  doTemplate("commissies.html", "commissies.html", "select commissieid,substr(max(datum), 0, 11) mdatum,commissie.afkorting, commissie.naam, inhoudsopgave,commissie.soort from activiteitactor,commissie,activiteit where commissie.id=activiteitactor.commissieid and activiteitactor.activiteitid = activiteit.id group by 1 order by commissie.naam asc");

  doTemplate("kamerleden.html", "kamerleden.html", R"(select fractiezetel.gewicht fzgewicht, persoon.titels, persoon.roepnaam, persoon.tussenvoegsel, persoon.achternaam, persoon.nummer, afkorting from Persoon,fractiezetelpersoon,fractiezetel,fractie where persoon.functie='Tweede Kamerlid' and persoonid=persoon.id and fractiezetel.id=fractiezetelpersoon.fractiezetelid and fractie.id=fractiezetel.fractieid and totEnMet='' union all select fractiezetel.gewicht fzgewicht, '' titels, '' roepnaam, '' tussenvoegsel, 'Vacature' achternaam, nummer, afkorting from FractieZetelVacature,fractieZetel, fractie where totEnMet='' and fractiezetel.id = fractiezetelid and fractie.id = fractieid order by afkorting, fzgewicht)");


  
  doTemplate("geschenken.html", "geschenken.html", "select datum, omschrijving, functie, initialen, tussenvoegsel, roepnaam, achternaam, gewicht,nummer,substr(persoongeschenk.bijgewerkt,0,11)  pgbijgewerkt from persoonGeschenk, Persoon where Persoon.id=persoonId and datum > '2019-01-01' order by persoongeschenk.bijgewerkt desc");


  doTemplate("toezeggingen.html", "toezeggingen.html", "select toezegging.id, tekst, toezegging.nummer, ministerie, status, naamToezegger, substr(activiteit.datum, 0, 11) datum, kamerbriefNakoming, datumNakoming, activiteit.nummer activiteitNummer, initialen, tussenvoegsel, achternaam, functie, fractie.afkorting as fractienaam, voortouwAfkorting, voortouwNaam from Toezegging,Activiteit left join Persoon on persoon.id = toezegging.persoonId left join Fractie on fractie.id = toezegging.fractieId where  Toezegging.activiteitId = activiteit.id and status != 'Voldaan' order by activiteit.datum desc");

  
  sws.d_svr.Get("/index.html", [](const httplib::Request &req, httplib::Response &res) {
    res.status = 301;
    res.set_header("Location", "./");
  });
  
  sws.d_svr.Get("/", [&tp](const httplib::Request &req, httplib::Response &res) {
    auto sqlw = tp.getLease();

    bool onlyRegeringsstukken = req.has_param("onlyRegeringsstukken") && req.get_param_value("onlyRegeringsstukken") != "0";

    string dlim = fmt::format("{:%Y-%m-%d}", fmt::localtime(time(0) - 8*86400));
    nlohmann::json data;
    auto recentDocs = packResultsJson(sqlw->queryT("select Document.datum datum, Document.nummer nummer, Document.onderwerp onderwerp, Document.titel titel, Document.soort soort, Document.bijgewerkt bijgewerkt, ZaakActor.naam naam, ZaakActor.afkorting afkorting from Document left join Link on link.van = document.id left join zaak on zaak.id = link.naar left join  ZaakActor on ZaakActor.zaakId = zaak.id and relatie = 'Voortouwcommissie' where  Document.soort != 'Sprekerslijst' and datum > ? and (? or Document.soort in ('Brief regering', 'Antwoord schriftelijke vragen', 'Voorstel van wet', 'Memorie van toelichting', 'Antwoord schriftelijke vragen (nader)')) and +bronDocument='' order by datum desc, bijgewerkt desc",
						  {dlim, !onlyRegeringsstukken}));


    nlohmann::json out = nlohmann::json::array();
    unordered_set<string> seen;
    for(auto& rd : recentDocs) {
      if(seen.count(rd["nummer"]))
	continue;
      seen.insert(rd["nummer"]);
      
      if(!rd.count("afkorting"))
	rd["afkorting"]="";
      string datum = ((string)rd["datum"]).substr(0,10);

      rd["datum"]=datum;
      string bijgewerkt = ((string)rd["bijgewerkt"]).substr(0,16);
      replaceSubstring(bijgewerkt, "T", "\xc2\xa0"); // &nsbp;
      replaceSubstring(bijgewerkt, "-", "\xe2\x80\x91"); // Non-Breaking Hyphen[1]
      rd["bijgewerkt"] = bijgewerkt;
      if(((string)rd["titel"]).empty())
	rd["titel"] = rd["soort"];
      out.push_back(rd);
    }

    // datum, bijgewerkt, afkorting, onderwerp, titel, nummer

    auto recentVerslagen = packResultsJson(getVerslagen(sqlw.get(), 8));
    for(auto& rv : recentVerslagen) {
      string datum = ((string)rv["datum"]).substr(0,10);

      rv["datum"]=datum;
      time_t updated = getTstampUTC(rv["updated"]);
      rv["bijgewerkt"] = fmt::format("{:%Y\xe2\x80\x91%m\xe2\x80\x91%d\xc2\xa0%H:%M}", fmt::localtime(updated));
      //      rd["bijgewerkt"]=
      rv["afkorting"]="Steno";
      // onderwerp should be ok
      rv["onderwerp"]=rv["titel"];
      rv["titel"] = "Verslag (" + (string)rv["status"]+")";
      rv["soort"] = "Verslag";
      rv["nummer"] = ((string)rv["vergaderingId"]).substr(0, 8);
      out.push_back(rv);
    }

    sort(out.begin(), out.end(), [](auto& a, auto& b) {
      return std::make_tuple((string)a["datum"], (string) a["bijgewerkt"]) >
	std::make_tuple((string)b["datum"], (string) b["bijgewerkt"]);
    });
    data["recentDocs"] = out;

    string f = fmt::format("{:%%-%m-%d}", fmt::localtime(time(0)));
    data["jarigVandaag"] = packResultsJson(sqlw->queryT("select geboortedatum,roepnaam,initialen,tussenvoegsel,achternaam,afkorting,persoon.nummer from Persoon,fractiezetelpersoon,fractiezetel,fractie where geboortedatum like ? and persoon.functie ='Tweede Kamerlid' and  persoonid=persoon.id and fractiezetel.id=fractiezetelpersoon.fractiezetelid and fractie.id=fractiezetel.fractieid and fractiezetelpersoon.totEnMet='' order by achternaam, roepnaam", {f}));

    inja::Environment e;
    e.set_html_autoescape(true);

    data["pagemeta"]["title"]="Hoofdpagina – OpenTK";
    data["og"]["title"] = "Recente documenten";
    data["og"]["description"] = "Recente documenten uit de Tweede Kamer";
    data["og"]["imageurl"] = "";
    
    res.set_content(e.render_file("./partials/index.html", data), "text/html");
  });

  // experimental, for vragen.html
  sws.d_svr.Get("/recente-kamervragen", [&tp](const httplib::Request &req, httplib::Response &res) {
    res.set_content(packResultsJson(tp.getLease()->queryT("select nummer,onderwerp,naam,gestartOp from Zaak,ZaakActor where zaakid=zaak.id and relatie='Indiener' and gestartOp > '2018-01-01' and soort = 'Schriftelijke vragen' order by gestartOp desc")).dump(), "application/json"); // XXX hardcoded date
  });
  
  sws.d_svr.Get("/open-vragen.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    nlohmann::json data;
    auto lease = tp.getLease();

    auto ovragen =  packResultsJson(lease->queryT("select openvragen.*, zaak.gestartOp, json_group_array(naam) as namen, max(naam) filter (where relatie='Indiener') as indiener, json_group_array(relatie) as relaties, json_group_array(zaakactor.functie) as functies, max(persoon.nummer) filter (where relatie='Indiener') as indiennummer from openvragen,zaakactor,zaak left join persoon on persoon.id=persoonid where zaakactor.zaakid = openvragen.id and zaak.nummer=openvragen.nummer group by openvragen.id order by gestartOp desc"));

    for(auto& ov : ovragen) {
      ov["gestartOp"] = ((string)ov["gestartOp"]).substr(0,10);
      /*
        namen = ["E. Heinen","T. van Oostenbruggen","P.H. Omtzigt","J.N. van Vroonhoven"]
     relaties = ["Gericht aan","Gericht aan","Indiener","Medeindiener"]
     functies = ["minister van Financiën","staatssecretaris van Financiën","Tweede Kamerlid","Tweede Kamerlid"]
 indiennummer = 2401
      */

      nlohmann::json functies = nlohmann::json::parse((string)ov["functies"]);
      set<string> dest;
      for(auto& f : functies) {
	string aan = f;
	// this also catches minister-president
	if(aan.find("inister") == string::npos && aan.find("taatssecretaris")==string::npos) {
	  continue;
	}
	replaceSubstring(aan, "minister van ", "");
	replaceSubstring(aan, "minister voor " , "");
	replaceSubstring(aan, "staatssecretaris van ", "");
	dest.insert(aan);
      }
      string aan;
      for(const auto& d : dest) {
	if(!aan.empty())
	  aan+=", ";
	aan += d;
      }
	
      ov["aan"] = aan;
      if(ov.count("indiennummer"))
	ov["fractie"] = getPartyFromNumber(lease.get(), ov["indiennummer"]);
    }
    data["openVragen"] = ovragen;
    
    inja::Environment e;
    e.set_html_autoescape(true);

    data["pagemeta"]["title"]="Open Vragen – OpenTK";
    data["og"]["title"] = "Open vragen";
    data["og"]["description"] = "Open vragen uit de Tweede Kamer";
    data["og"]["imageurl"] = "";
    
    res.set_content(e.render_file("./partials/open-vragen.html", data), "text/html");
  });

    sws.d_svr.Get("/getdoc/:nummer", [&tp](const httplib::Request &req, httplib::Response &res) {
    auto sqlw = tp.getLease();
    string nummer=req.path_params.at("nummer"); // 2023D41173

    string id, contentType;
    if(nummer.length()==36 && nummer.find_first_not_of("0123456789abcdef-") == string::npos) {
      id = nummer;
      auto ret = sqlw->queryT("select contentType from Document where id=?", {id});
      if(!ret.empty()) {
	contentType = get<string>(ret[0]["contentType"]);
      }
      else {
	ret = sqlw->queryT("select contentType from Verslag where id=?", {id});
	if(ret.empty()) {
	  fmt::print("Kon {} niet vinden, niet als document, niet als verslag\n", id);
	  res.set_content("Found nothing!!", "text/plain");
	  res.status=404;
	  return;
	}
	contentType = get<string>(ret[0]["contentType"]);
      }
    }
    else {
      auto ret=sqlw->queryT("select * from Document where nummer=? order by rowid desc limit 1", {nummer});
      if(ret.empty()) {
	res.set_content("Found nothing!!", "text/plain");
	res.status=404;
	return;
      }
      id = get<string>(ret[0]["id"]);
      contentType = get<string>(ret[0]["contentType"]);

    }
    sqlw.release();
    string content = getHtmlForDocument(id);
    res.set_content(content, "text/html; charset=utf-8");
  });


  sws.d_svr.Get("/besluiten.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    nlohmann::json data;
    string dlim = fmt::format("{:%Y-%m-%d}", fmt::localtime(time(0) - 8*86400));
    auto besluiten =  packResultsJson(tp.getLease()->queryT("select activiteit.datum, activiteit.nummer anummer, zaak.nummer znummer, agendapuntZaakBesluitVolgorde volg, besluit.status,agendapunt.onderwerp aonderwerp, zaak.onderwerp zonderwerp, naam indiener, besluit.tekst from besluit,agendapunt,activiteit,zaak left join zaakactor on zaakactor.zaakid = zaak.id and relatie='Indiener' where besluit.agendapuntid = agendapunt.id and activiteit.id = agendapunt.activiteitid and zaak.id = besluit.zaakid and datum > ? order by datum asc,agendapuntZaakBesluitVolgorde asc", {dlim}));

    for(auto& b : besluiten) {
      b["datum"] = ((string)b["datum"]).substr(0,10);
    }
    data["besluiten"] = besluiten;

    inja::Environment e;
    e.set_html_autoescape(true);

    data["pagemeta"]["title"]="Besluiten – OpenTK";
    data["og"]["title"] = "Recente en toekomstige besluiten";
    data["og"]["description"] = "Recente en toekomstige besluiten in de Tweede Kamer";
    data["og"]["imageurl"] = "";
    
    res.set_content(e.render_file("./partials/besluiten.html", data), "text/html");
  });
  
  // this is still alpine.js based though somehow!
  sws.d_svr.Get("/activiteit.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.get_param_value("nummer");
    nlohmann::json data;
    auto act = packResultsJson(tp.getLease()->queryT("select * from Activiteit where nummer=?", {nummer}));

    if(act.empty()) {
      res.status=404;
      res.set_content("No such activity", "text/plain");
      return;
    }
    inja::Environment e;
    e.set_html_autoescape(true);

    data["pagemeta"]["title"]="Activiteit – OpenTK";
    data["og"]["title"] = act[0]["onderwerp"];
    data["og"]["description"] = (string)act[0]["datum"] + ": "+ (string)act[0]["onderwerp"];
    data["og"]["imageurl"] = "";
    
    res.set_content(e.render_file("./partials/activiteit.html", data), "text/html");
  });

  sws.d_svr.Get("/activiteiten.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    // from 4 days ago into the future
    string dlim = fmt::format("{:%Y-%m-%d}", fmt::localtime(time(0)-4*86400));
    
    auto acts = packResultsJson(tp.getLease()->queryT("select Activiteit.datum datum, activiteit.bijgewerkt bijgewerkt, activiteit.nummer nummer, naam, noot, onderwerp, voortouwAfkorting, voortouwNaam from Activiteit left join Reservering on reservering.activiteitId=activiteit.id  left join Zaal on zaal.id=reservering.zaalId where datum > ? order by datum asc", {dlim}));

    for(auto& a : acts) {
      a["naam"] = htmlEscape(a["naam"]);
      a["onderwerp"] = htmlEscape(a["onderwerp"]);
      string datum = a["datum"];
		       
      datum=datum.substr(0,16);
      replaceSubstring(datum, "T", "&nbsp;");
      a["datum"]=datum;
    }
    nlohmann::json data = nlohmann::json::object();
    data["data"] = acts;
    inja::Environment e;
    e.set_html_autoescape(false); // NOTE WELL!

    data["pagemeta"]["title"]="Activiteiten – OpenTK";
    data["og"]["title"] = "Activiteiten";
    data["og"]["description"] = "Activiteiten Tweede Kamer";
    data["og"]["imageurl"] = "";
    
    res.set_content(e.render_file("./partials/activiteiten.html", data), "text/html");
  });

  sws.d_svr.Get("/ongeplande-activiteiten.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    auto acts = packResultsJson(tp.getLease()->queryT("select * from Activiteit where datum='' order by updated desc"));

    for(auto& a : acts) {
      a["onderwerp"] = htmlEscape(a["onderwerp"]);
      a["soort"] = htmlEscape(a["soort"]);
      string datum = a["bijgewerkt"];
		       
      datum=datum.substr(0,16);
      replaceSubstring(datum, "T", "&nbsp;");
      a["bijgewerkt"]=datum;
    }
    nlohmann::json data = nlohmann::json::object();
    data["data"] = acts;
    inja::Environment e;
    e.set_html_autoescape(false); // NOTE WELL!

    data["pagemeta"]["title"]="Ongeplande Activiteiten – OpenTK";
    data["og"]["title"] = "Nog ongeplande activiteiten";
    data["og"]["description"] = "Ongeplande activiteiten Tweede Kamer";
    data["og"]["imageurl"] = "";
    
    res.set_content(e.render_file("./partials/ongeplande-activiteiten.html", data), "text/html");
  });

  sws.d_svr.Get("/commissie.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    string id =req.get_param_value("id");

    auto deets = tp.getLease()->queryT("select * from commissie where id=?", {id});
    if(deets.empty()) {
      res.status = 404;
      res.set_content("Geen commissie met id "+id, "text/plain");
      return;
    }
    
    inja::Environment e;
    e.set_html_autoescape(true);
    nlohmann::json data;
    data["pagemeta"]["title"]= eget(deets[0], "naam");
    data["og"]["title"] = eget(deets[0], "naam");
    data["og"]["description"] = eget(deets[0], "naam");
    data["og"]["imageurl"] = "";

    data["id"] = id;
    data["naam"] = eget(deets[0], "naam");

    res.set_content(e.render_file("./partials/commissie.html", data), "text/html");
  });
  
  sws.d_svr.Get("/ksd.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    int nummer=atoi(req.get_param_value("ksd").c_str()); // 36228
    string toevoeging=req.get_param_value("toevoeging").c_str();
    auto sqlw = tp.getLease();
    auto docs = packResultsJson(sqlw->queryT("select document.nummer docnummer,* from Document,Kamerstukdossier where kamerstukdossier.nummer=? and kamerstukdossier.toevoeging=? and Document.kamerstukdossierid = kamerstukdossier.id order by volgnummer desc", {nummer, toevoeging}));
    nlohmann::json data = nlohmann::json::object();
    for(auto& d : docs) {
      d["datum"] = ((string)d["datum"]).substr(0, 10);
    }
    data["docs"] = docs;

    auto meta = sqlw->queryT("select * from kamerstukdossier where nummer=? and toevoeging=?",
			     {nummer, toevoeging});
    
    if(!meta.empty())
      data["meta"] = packResultsJson(meta)[0];
    inja::Environment e;
    e.set_html_autoescape(true);

    data["pagemeta"]["title"]="";
    data["og"]["title"] = docs[0]["titel"];
    data["og"]["description"] = docs[0]["titel"];
    data["og"]["imageurl"] = "";
    
    res.set_content(e.render_file("./partials/ksd.html", data), "text/html");
  });
  
  sws.d_svr.Get("/get/:nummer", [](const httplib::Request &req, httplib::Response &res) {
    string nummer=req.path_params.at("nummer"); // 2023D41173
    res.status = 301;
    res.set_header("Location", "../document.html?nummer="+nummer);
    res.set_content("Redirecting..", "text/plain");
  });

  sws.d_svr.Get("/document.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    string nummer = req.get_param_value("nummer"); // 2023D41173
    auto sqlw = tp.getLease();

    if(nummer.length() > 13 &&
       ranges::all_of(nummer.cbegin(), nummer.cend(),
		      [](char c) { return isalnum(c) || c=='-'; })) {
      res.status = 301;
      res.set_header("Location", "verslag.html?vergaderingid="+nummer);
      return;
    }

    nlohmann::json data = nlohmann::json::object();
    auto ret=sqlw->queryT("select Document.*, DocumentVersie.externeidentifier, DocumentVersie.versienummer, DocumentVersie.extensie from Document left join DocumentVersie on DocumentVersie.documentId = Document.id where nummer=? limit 1", {nummer});
    if(ret.empty()) {
      res.set_content("Found nothing in document.html!!", "text/plain");
      res.status = 404;
      return;
    }


    string externeid = eget(ret[0], "externeidentifier");
    data["meta"] = packResultsJson(ret)[0];
    data["meta"]["datum"] = ((string)data["meta"]["datum"]).substr(0, 10);

    string bijgewerkt = ((string)data["meta"]["bijgewerkt"]).substr(0, 16);
    bijgewerkt[10]=' ';
    data["meta"]["bijgewerkt"] = bijgewerkt;    
    string kamerstukdossierId, kamerstuktoevoeging;
    int kamerstuknummer=0, kamerstukvolgnummer=0;
    string kamerstuktitel;
    try {
      kamerstukdossierId = get<string>(ret[0]["kamerstukdossierId"]);
      auto kamerstuk = sqlw->queryT("select * from kamerstukdossier where id=? order by rowid desc limit 1", {kamerstukdossierId});
      if(!kamerstuk.empty()) {
	kamerstuknummer = get<int64_t>(kamerstuk[0]["nummer"]);
	kamerstuktoevoeging = get<string>(kamerstuk[0]["toevoeging"]);
	kamerstuktitel = get<string>(kamerstuk[0]["titel"]);
	kamerstukvolgnummer = get<int64_t>(ret[0]["volgnummer"]);

	data["kamerstuk"]["nummer"]=kamerstuknummer;
	data["kamerstuk"]["toevoeging"]=kamerstuktoevoeging;
	data["kamerstuk"]["titel"]=kamerstuktitel;
	data["kamerstuk"]["volgnummer"]=kamerstukvolgnummer;
      }
    }
    catch(exception& e) {
      fmt::print("Could not get kamerstukdetails: {}\n", e.what());
    }

    string bronDocumentId = std::get<string>(ret[0]["bronDocument"]);
    
    string dir="kamervragen";
    if(data["meta"]["soort"]=="Brief regering")
      dir = "brieven_regering";
    
    data["kamerurl"] = fmt::format("https://www.tweedekamer.nl/kamerstukken/{}/detail?id={}&did={}", 	     dir,
			     get<string>(ret[0]["nummer"]),
			     get<string>(ret[0]["nummer"]));

    string documentId=get<string>(ret[0]["id"]);
    data["docactors"]= packResultsJson(sqlw->queryT("select DocumentActor.*, Persoon.nummer from DocumentActor left join Persoon on Persoon.id=Documentactor.persoonId where documentId=? order by relatie", {documentId}));

    for(auto& da : data["docactors"]) {
      if(da["nummer"] != "")
	da["fractie"] = getPartyFromNumber(sqlw.get(), (int)da["nummer"]);
    }

    if(!bronDocumentId.empty()) {
      data["brondocumentData"] = packResultsJson(sqlw->queryT("select * from document where id=? order by rowid desc limit 1", {bronDocumentId}));
    }

    data["bijlagen"] = packResultsJson(sqlw->queryT("select * from document where bronDocument=?", {documentId}));

    auto zlinks = sqlw->queryT("select distinct(naar) as naar, zaak.nummer znummer from Link,Zaak where van=? and naar=zaak.id and category='Document' and linkSoort='Zaak'", {documentId});

    set<string> actids;
    set<string> znummers;
    for(auto& zlink : zlinks) {
      string zaakId = get<string>(zlink["naar"]);
      string znummer = get<string>(zlink["znummer"]);
      auto zactors = packResultsJson(sqlw->queryT("select * from Zaak,ZaakActor where zaak.id=?  and ZaakActor.zaakId = zaak.id order by relatie", {zaakId}));
      data["zaken"][znummer]["actors"] = zactors;

      if(!zactors.empty()) {
	for(auto& z: zactors) 
	  znummers.insert((string)(z["nummer"]));
      }

      data["zaken"][znummer]["docs"] = packResultsJson(sqlw->queryT("select * from Document,Link where Link.naar=? and link.van=Document.id", {zaakId}));

      data["zaken"][znummer]["besluiten"] = packResultsJson(sqlw->queryT("select * from besluit where zaakid=? order by rowid", {zaakId}));
      set<string> agendapuntids;
      for(auto& b: data["zaken"][znummer]["besluiten"]) {
	agendapuntids.insert((string)b["agendapuntId"]);
      }

      for(auto& agendapuntid : agendapuntids) {
	auto agendapunten = sqlw->queryT("select * from Agendapunt where id = ?", {agendapuntid});
	for(auto& agendapunt: agendapunten)
	  actids.insert(get<string>(agendapunt["activiteitId"]));
      }
    }
    data["znummers"]=znummers;
    nlohmann::json activiteiten = nlohmann::json::array();

    if(!actids.empty()) {
      for(auto& actid : actids) {
	auto activiteit = packResultsJson(sqlw->queryT("select * from Activiteit where id = ? order by rowid desc limit 1", {actid}));
	for(auto&a : activiteit) {
	  string d = ((string)a["datum"]).substr(0,16);
	  d[10]= ' ';
	  a["datum"] = d;
	  activiteiten.push_back(a);
	}
      }
    }
    // directly linked activity
    auto diract = packResultsJson(sqlw->queryT("select Activiteit.* from Link,Activiteit where van=? and naar=Activiteit.id", {documentId}));
    for(auto&a : diract) {
      string d = ((string)a["datum"]).substr(0,16);
      d[10]= ' ';
      a["datum"] = d;
      activiteiten.push_back(a);
    }
    sort(activiteiten.begin(), activiteiten.end(), [](auto&a, auto&b) {
      return a["datum"] < b["datum"];
    });
    data["activiteiten"] = activiteiten;

    inja::Environment e;
    e.set_html_autoescape(false);

    data["pagemeta"]["title"]=get<string>(ret[0]["onderwerp"]);
    data["og"]["title"] = get<string>(ret[0]["onderwerp"]);
    data["og"]["description"] = get<string>(ret[0]["titel"]) + " " +get<string>(ret[0]["onderwerp"]);
    data["og"]["imageurl"] = "";

    bulkEscape(data); 

    if(!externeid.empty() && haveExternalIdFile(externeid)) {
      data["content"] = getBareHtmlFromExternal(externeid);
    }
    else if(get<string>(ret[0]["contentType"])=="application/pdf") {
      string agent;
      if (req.has_header("User-Agent")) {
	agent = req.get_header_value("User-Agent");
      }
      if(agent.find("Firefox") == string::npos && (agent.find("iPhone") != string::npos || agent.find("Android") != string::npos ))
	data["meta"]["iframe"]="getdoc";
      else
	data["meta"]["iframe"]="getraw";
    }
    else {
      data["content"] = getHtmlForDocument(documentId, true); // bare!
      data["meta"]["iframe"] = "getdoc";
    }
    res.set_content(e.render_file("./partials/getorig.html", data), "text/html");
  });
  
  sws.d_svr.Get("/verslag.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    string id = req.get_param_value("vergaderingid"); // 9e79de98-e914-4dc8-8dc7-6d7cb09b93d7
    auto verslagen = packResultsJson(tp.getLease()->queryT("select *,substr(datum,0,11) datum from vergadering,verslag where verslag.vergaderingid=vergadering.id and status != 'Casco' and vergadering.id=? order by datum desc, verslag.updated desc limit 1", {id}));
    if(verslagen.empty()) {
      res.status = 404;
      res.set_content("Geen vergadering gevonden", "text/plain");
      return;
    }

    nlohmann::json data = verslagen[0];
    
    // 2024-09-19T12:19:10.3141655Z
    string updated = data["updated"];
    struct tm tm;
    strptime(updated.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
    time_t then = timegm(&tm);
    data["updated"] = fmt::format("{:%Y-%m-%d %H:%M}", fmt::localtime(then));
    // this accidentally gets the "right" id 

    inja::Environment e;
    e.set_html_autoescape(false); // XX 

    data["pagemeta"]["title"]=data["onderwerp"];
    data["og"]["title"] = data["onderwerp"];
    data["og"]["description"] = (string)data["titel"];
    data["og"]["imageurl"] = "";

    bulkEscape(data); 
    data["htmlverslag"]=getHtmlForDocument(data["id"], true);
    res.set_content(e.render_file("./partials/verslag.html", data), "text/html");
  });

  
  sws.d_svr.Get("/verslagen.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    auto tmp = getVerslagen(tp.getLease().get(), 2*365);
    inja::Environment e;
    e.set_html_autoescape(true);
    nlohmann::json data;
    data["recenteVerslagen"] = packResultsJson(tmp);
    data["pagemeta"]["title"]="Verslagen – OpenTK";
    data["og"]["title"] = "Recente verslagen";
    data["og"]["description"] = "Recente verslagen uit de Tweede Kamer";
    data["og"]["imageurl"] = "";
    
    res.set_content(e.render_file("./partials/verslagen.html", data), "text/html");
  });

  
  sws.d_svr.Get("/recent-kamerstukdossiers", [&tp](const httplib::Request &req, httplib::Response &res) {
    
    auto docs = tp.getLease()->queryT("select kamerstukdossier.nummer, max(document.datum) mdatum,kamerstukdossier.titel,kamerstukdossier.toevoeging,hoogsteVolgnummer from kamerstukdossier,document where document.kamerstukdossierid=kamerstukdossier.id and document.datum > '2020-01-01' group by kamerstukdossier.id,toevoeging order by 2 desc");
    // XXX hardcoded date
    res.set_content(packResultsJsonStr(docs), "application/json");
    fmt::print("Returned {} kamerstukdossiers\n", docs.size());
  });

  
  // select * from persoonGeschenk, Persoon where Persoon.id=persoonId order by Datum desc


  /* stemmingen. Poeh. Een stemming is eigenlijk een Stem, en ze zijn allemaal gekoppeld aan een besluit.
     een besluit heeft een Zaak en een Agendapunt
     een agendapunt hoort weer bij een activiteit, en daar vinden we eindelijk een datum

     Er zijn vaak twee besluiten per motie, eentje "ingediend" en dan nog een besluit waarop
     gestemd wordt.
     
  */

  sws.d_svr.Get("/stemmingen.html", [&tp](const httplib::Request &req, httplib::Response &res) {
    string start = fmt::format("{:%Y-%m-%d}", fmt::localtime(time(0) - 21 * 86400));
    auto sqlw = tp.getLease();
    auto besluiten = packResultsJson(sqlw->queryT("select besluit.id as besluitid, besluit.soort as besluitsoort, besluit.tekst as besluittekst, besluit.opmerking as besluitopmerking, activiteit.datum, activiteit.nummer anummer, zaak.nummer znummer, agendapuntZaakBesluitVolgorde volg, besluit.status,agendapunt.onderwerp aonderwerp, zaak.onderwerp zonderwerp, naam indiener from besluit,agendapunt,activiteit,zaak left join zaakactor on zaakactor.zaakid = zaak.id and relatie='Indiener' where besluit.agendapuntid = agendapunt.id and activiteit.id = agendapunt.activiteitid and zaak.id = besluit.zaakid and datum > ? order by datum desc,agendapuntZaakBesluitVolgorde asc", {start})); 

    nlohmann::json stemmingen = nlohmann::json::array();
    for(auto& b : besluiten) {
      VoteResult vr;
      if(!getVoteDetail(sqlw.get(), b["besluitid"], vr))
	continue;

      /*
      fmt::print("{}, voor: {} ({}), tegen: {} ({}), niet deelgenomen: {} ({})\n",
		 (string)b["besluitid"],
		 vr.voorpartij, vr.voorstemmen,
		 vr.tegenpartij, vr.tegenstemmen,
		 vr.nietdeelgenomenpartij, vr.nietdeelgenomen);
      */
      string datum = b["datum"]; // 2024-10-10T12:13:14
      datum[10]=' ';
      datum.resize(16);
      b["datum"] = datum;
      b["voorpartij"] = vr.voorpartij;
      b["tegenpartij"] = vr.tegenpartij;
      b["voorstemmen"] = vr.voorstemmen;
      b["tegenstemmen"] = vr.tegenstemmen;
      b["nietdeelgenomenstemmen"] = vr.nietdeelgenomen;
      stemmingen.push_back(b);
    }
    nlohmann::json data;
    data["stemmingen"] = stemmingen;
    
    inja::Environment e;
    e.set_html_autoescape(true); 

    data["pagemeta"]["title"]="Stemmingen – OpenTK";
    data["og"]["title"] = "Stemmingen";
    data["og"]["description"] = "Stemmingen";
    data["og"]["imageurl"] = "";

    res.set_content(e.render_file("./partials/stemmingen.html", data), "text/html");
  });

  sws.d_svr.Post("/search", [](const httplib::Request &req, httplib::Response &res) {
    string term = req.get_file_value("q").content;
    string twomonths = req.get_file_value("twomonths").content;
    string soorten = req.get_file_value("soorten").content;
    string limit = "";
    if(twomonths=="true")
      limit = "2024-08-11";

    term = convertToSQLiteFTS5(term);

    SQLiteWriter idx("tkindex.sqlite3", SQLWFlag::ReadOnly);
    idx.query("ATTACH DATABASE 'tk.sqlite3' as meta");
    
    static auto s_uc = make_shared<int>(0);
    cout<<"Search: '"<<term<<"', limit '"<<limit<<"', soorten: '"<<soorten<<"', " <<
      s_uc.use_count()<<" ongoing"<<endl;
    DTime dt;
    dt.start();
    std::vector<std::unordered_map<std::string,MiniSQLite::outvar_t>> matches; // ugh
    int mseclimit = 10000;

    shared_ptr<int> uc = s_uc;
    
    if(s_uc.use_count() > 8) {
      mseclimit = 1000;
      fmt::print(">6 searchs ongoing, limiting to {}\n", mseclimit);
    }
    else if(s_uc.use_count() > 5) {
      mseclimit = 2000;
      fmt::print(">3 searchs ongoing, limiting to {}\n", mseclimit);
    }
    
    if(soorten=="moties") {
      matches = idx.queryT("SELECT uuid, soort, Document.onderwerp, Document.titel, document.nummer, document.bijgewerkt, document.datum, snippet(docsearch,-1, '<b>', '</b>', '...', 20) as snip, bm25(docsearch) as score, category FROM docsearch, meta.document WHERE docsearch match ? and document.id = uuid and document.datum > ? and document.soort='Motie' order by score limit 280", {term, limit}, mseclimit);
    }
    else if(soorten=="vragenantwoorden") {
      matches = idx.queryT("SELECT uuid, soort, Document.onderwerp, Document.titel, document.nummer, document.bijgewerkt, document.datum, snippet(docsearch,-1, '<b>', '</b>', '...', 20) as snip, bm25(docsearch) as score, category FROM docsearch, meta.document WHERE docsearch match ? and document.id = uuid and document.datum > ? and document.soort in ('Schriftelijke vragen', 'Antwoord schriftelijke vragen', 'Antwoord schriftelijke vragen (nader)')  order by score limit 280", {term, limit}, mseclimit);
    }
    else {
      matches = idx.queryT("SELECT uuid, snippet(docsearch,-1, '<b>', '</b>', '...', 20) as snip, bm25(docsearch) as score, category FROM docsearch WHERE docsearch match ? and datum > ? order by score limit 280", {term, limit}, mseclimit);
      
      for(auto& m : matches) {
	auto doc =  idx.queryT("select onderwerp, bijgewerkt, titel, nummer, datum FROM meta.Document where id=?",
			      {get<string>(m["uuid"])});
	if(doc.empty())
	  continue;
	for(const auto& f : doc[0])
	  m[f.first]=f.second;
      }
      for(auto& m : matches) {
	if(m.count("onderwerp")) // already matched as document
	  continue;
      
	auto verslag = idx.queryT("SELECT titel as onderwerp, Vergadering.id as vergaderingId, Verslag.updated as bijgewerkt, '' as titel, Vergadering.datum FROM meta.Verslag, meta.Vergadering WHERE Verslag.id = ? and Vergadering.id = Verslag.vergaderingId", {get<string>(m["uuid"])});
	
	if(verslag.empty()) {
	  fmt::print("Weird - uuid {} is not a Document and not a Verslag? Probably erased. Category {}\n", get<string>(m["uuid"]), eget(m, "category"));
	  m.clear();
	  continue;
	}

	for(const auto& f : verslag[0])
	  m[f.first]=f.second;

	m["category"]="Vergadering";
	m["nummer"] =get<string>(verslag[0]["vergaderingId"]).substr(0, 8);
      }
    }
    // remove empty matches which could not be found
    erase_if(matches, [](const auto& m) { return m.empty(); });
    auto usec = dt.lapUsec();
    fmt::print("Got {} matches in {} msec\n", matches.size(), usec/1000.0);
    g_stats.searchUsec += usec;
    nlohmann::json response=nlohmann::json::object();
    response["results"]= packResultsJson(matches);

    response["milliseconds"] = usec/1000.0;
    res.set_content(response.dump(), "application/json");
  });


  auto addMetric = [](ostringstream& ret, std::string_view key, std::string_view desc, std::string_view kind, const auto& value)
  {
    ret << "# HELP tkserv_" << key << " " <<desc <<endl;
    ret << "# TYPE tkserv_"<< key << " " << kind <<endl;
    ret<<"tkserv_"<<key<<" "<< std::fixed<< value <<endl;
  };

  
  sws.wrapGet({}, "/metrics", [&](auto& cr) {
    ostringstream os;
    addMetric(os, "hits", "hits", "counter", g_stats.hits);
    addMetric(os, "latency_usec", "total latency in microseconds", "counter", g_stats.usec);
    addMetric(os, "etaghits", "etag hits", "counter", g_stats.etagHits);
    addMetric(os, "searchUsec", "search microseconds", "counter", g_stats.searchUsec);
    addMetric(os, "http2xx", "2xx error codes", "counter", g_stats.http2xx);
    addMetric(os, "http3xx", "3xx error codes", "counter", g_stats.http3xx);
    addMetric(os, "http4xx", "4xx error codes", "counter", g_stats.http4xx);
    addMetric(os, "http5xx", "5xx error codes", "counter", g_stats.http5xx);
    addMetric(os, "maxdb", "maximum number of concurrent db queries", "gauge", cr.tp.d_maxout);

    auto row = cr.lsqw.query("select count(1) c from users");
    addMetric(os, "numusers", "Number of user accounts", "gauge", get<int64_t>(row[0]["c"]));
    row = cr.lsqw.query("select count(1) c from scanners");
    addMetric(os, "numscanners", "Number of scanners", "gauge", get<int64_t>(row[0]["c"]));
    row = cr.lsqw.query("select count(1) c from userInvite");
    addMetric(os, "numinvites", "Number of user invites", "gauge", get<int64_t>(row[0]["c"]));
    row = cr.lsqw.query("select count(1) c from sessions");
    addMetric(os, "numsessions", "Number of user sessions", "gauge", get<int64_t>(row[0]["c"]));
    row = cr.lsqw.query("select count(1) c from sentNotification");
    addMetric(os, "numnotifications", "Number of sent notifications", "gauge", get<int64_t>(row[0]["c"]));

    row = cr.lsqw.query("select count(1) c from emissions");
    addMetric(os, "numemissions", "Number of (email) emissions", "gauge", get<int64_t>(row[0]["c"]));

    
    row = cr.tp.getLease()->queryT("select count(1) c from Document");
    addMetric(os, "numdocs", "Number of Document", "gauge", get<int64_t>(row[0]["c"]));
    row = cr.tp.getLease()->queryT("select count(1) c from Verslag");
    addMetric(os, "numverslagen", "Number of Verslag", "gauge", get<int64_t>(row[0]["c"]));
    row = cr.tp.getLease()->queryT("select count(1) c from Activiteit");
    addMetric(os, "numactiviteiten", "Number of Activiteit", "gauge", get<int64_t>(row[0]["c"]));

    addMetric(os, "uptime", "Seconds of uptime", "gauge",  time(0) - g_starttime);
    
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    double sec = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec/1000000.0 + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec/1000000.0;
    addMetric(os, "cpuselfmsec" , "Number of self CPU milliseconds", "counter", 1000.0 * sec);
    getrusage(RUSAGE_CHILDREN, &usage);
    sec= usage.ru_utime.tv_sec + usage.ru_utime.tv_usec/1000000.0 + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec/1000000.0;
    addMetric(os, "cpuchildrenmsec" , "Number of child CPU milliseconds", "counter", 1000.0 * sec);

    addMetric(os, "failed_session_join", "Failed attempts to join an existing session", "counter", cr.stats.failedSessionJoin);
    addMetric(os, "successful_session_join", "Successful attempts to join an existing session", "counter", cr.stats.successfulSessionJoin);
    addMetric(os, "session_join_invite", "Emailed invites to join an existing session", "counter", cr.stats.sessionJoinInvite);

    addMetric(os, "sqlite_execs", "Number of executed prepared SQLite queries", "counter", (uint64_t)MiniSQLite::s_execs);
    addMetric(os, "sqlite_fullscans", "Number of SQLite fullscan steps", "counter", (uint64_t)MiniSQLite::s_fullscans);
    addMetric(os, "sqlite_sorts", "Number of SQLite sort operations", "counter", (uint64_t)MiniSQLite::s_sorts);
    addMetric(os, "sqlite_autoindexes", "Number of SQLite autoindex operations", "counter", (uint64_t)MiniSQLite::s_autoindexes);

    int ulim=1;
    for(unsigned int n=0; n < g_stats.lats.size() -1; ++n) {
      addMetric(os, "lat_lt_"+to_string(ulim), "Queries with latency less than "+to_string(ulim)+" msec", "counter", (uint64_t)g_stats.lats[n]);
      ulim *= 2;
    }
    addMetric(os, "lat_gt_"+to_string(ulim/2), "Queries with latency more than "+to_string(ulim/2)+" msec", "counter", (uint64_t)g_stats.lats[g_stats.lats.size()-1]);
    return make_pair<string,string>(os.str(), "text/plain");
  });
  
  sws.d_svr.set_exception_handler([](const auto& req, auto& res, std::exception_ptr ep) {
    auto fmt = "<h1>Error 500</h1><p>%s</p>";
    string buf;
    try {
      std::rethrow_exception(ep);
    } catch (std::exception &e) {
      buf = fmt::sprintf(fmt, htmlEscape(e.what()).c_str());
    } catch (...) { // See the following NOTE
      buf = fmt::sprintf(fmt, "Unknown exception");
    }
    cout<<"Error: '"<<buf<<"'"<<endl;
    res.set_content(buf, "text/html");
    res.status = 500; 
  });

  TimeKeeper<httplib::Request> tk;
  
  sws.d_svr.set_pre_routing_handler([&tp, &tk](const auto& req, auto& res) {
    /*
    fmt::print("Req: {} {} {} max-db {} {}\n", req.path, req.params, req.has_header("User-Agent") ? req.get_header_value("User-Agent") : "",
	       (unsigned int)tp.d_maxout, (void*)&req);
    */
    tk.report(&req);
    return httplib::Server::HandlerResponse::Unhandled;
  });
  
  sws.d_svr.set_post_routing_handler([&tp, &tk](const auto& req, auto& res) {
    auto msec = tk.getMsec(&req);
    g_stats.usec += 1000.0*msec;
    unsigned int off=0;
    if(msec < 1)
      off=0;
    else if(msec < 2)
      off=1;
    else if(msec < 4)
      off=2;
    else if(msec < 8)
      off=3;
    else if(msec < 16)
      off=4;
    else if(msec < 32)
      off=5;
    else if(msec < 64)
      off=6;
    else if(msec < 128)
      off=7;
    else if(msec < 256)
      off=8;
    else if(msec < 512)
      off=9;
    else if(msec < 1024)
      off=10;
    else
      off=11;
    g_stats.lats[off]++;
    
    fmt::print("Req: {} {} {} max-db {} {} msec {}\n", req.path, req.params, req.has_header("User-Agent") ? req.get_header_value("User-Agent") : "",
	       (unsigned int)tp.d_maxout, msec, off);

    res.set_header("Content-Security-Policy", "frame-ancestors 'self';");

    if(endsWith(req.path, ".js") || endsWith(req.path, ".css"))
      res.set_header("Cache-Control", "max-age=3600");

    // Allow conditional GET and HEAD for all requests: if browsers, proxies or bots already have
    // a response cached that is bytewise identical to what we've just generated, don't resend it.
    if(res.status == 200 && (req.method == "GET" || req.method == "HEAD") && !res.has_header("ETag")) {
      string etag = fmt::format("\"{:x}-{}\"", hash<string>{}(res.body), res.body.size());
      res.set_header("ETag", etag);

      if(req.get_header_value("If-None-Match", "") == etag) {
        res.set_content("", "");
        res.headers.erase("Content-Type");
        res.status = 304;
	g_stats.etagHits++;
      }
    }
    g_stats.hits++;
    if(res.status >= 200 && res.status < 300)
      g_stats.http2xx++;
    else if(res.status >= 300 && res.status < 400)
      g_stats.http3xx++;
    else if(res.status >= 400 && res.status < 500)
      g_stats.http4xx++;

    if(res.status >= 500 && res.status < 600)
      g_stats.http5xx++;
  });

  sws.d_svr.set_payload_max_length(1024 * 1024); // 1MB
  
  string root = "./html/";
  if(argc > 2)
    root = argv[2];
  sws.d_svr.set_mount_point("/", root);
  int port = 8089;
  if(argc > 1)
    port = atoi(argv[1]);
  
  fmt::print("Listening on port {} serving html from {}, using {} threads\n",
	     port, root, CPPHTTPLIB_THREAD_POOL_COUNT);

  sws.d_svr.set_socket_options([](socket_t sock) {
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
	       reinterpret_cast<const void *>(&yes), sizeof(yes));
  });
  
  sws.d_svr.listen("0.0.0.0", port);
  fmt::print("Exiting - {}\n", strerror(errno));
}



/*
static string getPDFForDocx(const std::string& id)
{
  if(isPresentNonEmpty(id, "doccache", ".pdf") && cacheIsNewer(id, "doccache", ".pdf", "docs")) {
    string fname = makePathForId(id, "doccache", ".pdf");
    string ret = getContentsOfFile(fname);
    if(!ret.empty()) {
      fmt::print("Had a cache hit for {} PDF\n", id);
      return ret;
    }
    // otherwise fall back to normal process
  }
  // 
  string fname = makePathForId(id);
  string command = fmt::format("pandoc -s --metadata \"margin-left:1cm\" --metadata \"margin-right:1cm\" -V fontfamily=\"dejavu\"  --variable mainfont=\"DejaVu Serif\" --variable sansfont=Arial --pdf-engine=xelatex -f docx -t pdf '{}'",
			  fname);
  FILE* pfp = popen(command.c_str(), "r");
  if(!pfp)
    throw runtime_error("Unable to perform conversion for '"+command+"': "+string(strerror(errno)));
  
  shared_ptr<FILE> fp(pfp, pclose);
  char buffer[4096];
  string ret;
  for(;;) {
    int len = fread(buffer, 1, sizeof(buffer), fp.get());
    if(!len)
      break;
    ret.append(buffer, len);
  }
  if(ferror(fp.get()))
    throw runtime_error("Unable to perform pandoc: "+string(strerror(errno)));

  string rsuffix ="."+to_string(getRandom64());
  string oname = makePathForId(id, "doccache", "", true);
  {
    auto out = fmt::output_file(oname+rsuffix);
    out.print("{}", ret);
  }
  if(rename((oname+rsuffix).c_str(), (oname+".pdf").c_str()) < 0) {
    unlink((oname+rsuffix).c_str());
    fmt::print("Rename of cached PDF failed\n");
  }
  
  return ret;
}
*/


  /*
  */
