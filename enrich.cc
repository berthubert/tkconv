#include "support.hh"
#include <regex>


using namespace std;

std::string enrichHTML(const std::string& html, SQLiteWriter& sqlw)
{
  string rep= html;
  if(rep.find("\xc2\xa0") != string::npos) 
    rep = regex_replace(rep, regex("\xc2\xa0"), " ");
  
  std::regex generic(R"((([123][0-9]{4}|[123][0-9]\s[0-9]{3}))([ -][0-9A-Z]{1,4})?[,nr\.\s-]{1,6}([1-9][0-9]{0,3}))");

  auto gen_begin = std::sregex_iterator(rep.begin(), rep.end(), generic);
  auto gen_end = std::sregex_iterator();

  if(gen_begin == gen_end)
    return html;
  
  string result;
  auto out = std::back_inserter(result);
  std::smatch m;

  //  cout<<"Had "<<distance(gen_begin, gen_end)<<" hits, " << (gen_begin != gen_end) <<endl;
  for (auto i = gen_begin; i != gen_end; ++i) {
    m = *i;
    out = std::copy(m.prefix().first, m.prefix().second, out);

    /*
    cout<<"Match: '"<<m.str()<<"'"<<endl;
    int counter=0;
    for(const auto& sm: m) {
      cout<<counter<<": "<<sm<<endl;
      counter++;
    }
    cout<<"Copied in prefix, now copying in our replacement"<<endl;
    */
    
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
  // copies in either the whole document or only the remainder
  out = m.ready()
              ? std::copy(m.suffix().first, m.suffix().second, out)
    : std::copy(rep.begin(), rep.end(), out);
  
  return result; 
}
