#include "support.hh"
#include <regex>


using namespace std;

std::string enrichHTML(const std::string& html, SQLiteWriter& sqlw)
{
  string rep= html;
  if(rep.find("\xc2\xa0") != string::npos) 
    rep = regex_replace(rep, regex("\xc2\xa0"), " ");
  
  std::regex generic(R"((([1234][0-9]{4}|[1234][0-9]\s[0-9]{3}))([ -][0-9A-Z]{1,4})?[,nr\.\s-]{1,6}([1-9][0-9]{0,3}))");

  auto gen_begin = std::sregex_iterator(rep.begin(), rep.end(), generic);
  auto gen_end = std::sregex_iterator();

  // not optional, loop below doesn't work w/o hits!
  if(gen_begin != gen_end) {
    string result;
    auto out = std::back_inserter(result);
    std::smatch m;
    
    for (auto i = gen_begin; i != gen_end; ++i) {
      m = *i;
      out = std::copy(m.prefix().first, m.prefix().second, out);
      
      if(m.position() > 4 && rep.substr(m.position()-4,4)=="kst-") {
	//      cout<<"Protecting kst-12345 link!"<<endl;
	string blah = m[0].str();
	out = std::copy(blah.begin(), blah.end(), out);
	continue;
      }
      
      string rawnum = m[1].str().c_str(); // 23 234 -> 23234
      if(rawnum.size() > 5)
	rawnum = rawnum.substr(0,2) + rawnum.substr(rawnum.size()-3);
      
      int ksdnum = atoi(rawnum.c_str());
      string toevoeging = m[3].str(); // "-08"
      if(!toevoeging.empty())
	toevoeging = toevoeging.substr(1);
      int volgnummer = atoi(m[4].str().c_str());
      
      cout<<"Parsed '"<<m[0]<<"' -> "<<rawnum<<"' as ksdnum: "<<ksdnum<<", toevoeging '"<<toevoeging<<"', volgnummer: "<<volgnummer << endl;
      auto sqlret = sqlw.queryT("select document.nummer num from document,kamerstukdossier where kamerstukdossierid=kamerstukdossier.id and kamerstukdossier.nummer=? and volgnummer=? and toevoeging=?",
				{ksdnum, volgnummer, toevoeging});
      string blah;
      if(!sqlret.empty())
	blah = fmt::format(R"(<a href="./document.html?nummer={}">{}</a>)",
			   eget(sqlret[0], "num"), m[0].str());
      else {
	cout<<"NOT FOUND!"<<endl;
	blah = m[0].str();
      }
      
      out = std::copy(blah.begin(), blah.end(), out);
    }
    std::copy(m.suffix().first, m.suffix().second, out);
    rep = result;
  }

  // Zij krijgt nr. 456 (36800-XVI)
  std::regex motie(R"(([1-9]\d{0,3})\s\(([1234]\d{4})(-[0-9A-Z]+)?\))");
  //  
  gen_begin = std::sregex_iterator(rep.begin(), rep.end(), motie);
  gen_end = std::sregex_iterator();

  if(gen_begin != gen_end) {
    cout<<"Got motie hit(s)!"<<endl;

    string result;
    auto out = std::back_inserter(result);
    std::smatch m;

    int debatnummer=1;
    for (auto i = gen_begin; i != gen_end; ++i) {
      m = *i;
      out = std::copy(m.prefix().first, m.prefix().second, out);
      
      int volgnummer = atoi(m[1].str().c_str());
      int ksdnum = atoi(m[2].str().c_str());
      string toevoeging = m[3].str();
      if(!toevoeging.empty())
	toevoeging=toevoeging.substr(1); // get rid of -
      
      cout<<"Parsed '"<<m[0]<<"' as ksdnum: "<<ksdnum<<", toevoeging '"<<toevoeging<<"', volgnummer: "<<volgnummer << endl;
      auto sqlret = sqlw.queryT("select document.nummer num from document,kamerstukdossier where kamerstukdossierid=kamerstukdossier.id and kamerstukdossier.nummer=? and volgnummer=? and toevoeging=?",
				{ksdnum, volgnummer, toevoeging});
      string blah;
      if(!sqlret.empty())
	blah = fmt::format(R"(<a href="./document.html?nummer={}">{}</a> (#{}))",
			   eget(sqlret[0], "num"), m[0].str(), debatnummer);
      else {
	cout<<"NOT FOUND!"<<endl;
	blah = m[0].str();
      }
      debatnummer++;
      out = std::copy(blah.begin(), blah.end(), out);
    }
    std::copy(m.suffix().first, m.suffix().second, out);
    rep = result;
  }
  
  return rep; 
}
