#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <algorithm> // std::move() and friends
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h> //unlink(), usleep()
#include <unordered_map>
#include "doctest.h"
#include "search.hh"
#include <chrono>
#include <fmt/chrono.h>
#include <fmt/printf.h>
#include "nlohmann/json.hpp"

#include "support.hh"

using namespace std;

TEST_CASE("qp test") {
  CHECK(toQuotedPrintable("tkserv") == "tkserv");
  CHECK(toQuotedPrintable("tkserv=tof") == "tkserv=3Dtof");
  CHECK(toQuotedPrintable("overheidscoöperatie Wigo4it") == "overheidsco=C3=B6peratie Wigo4it");

  CHECK(toQuotedPrintable("Dit is een lekker lange zin die meer dan 76 tekens is en die voorzien moet worden van een soft linebreak") == "Dit is een lekker lange zin die meer dan 76 tekens is en die voorzien moet =\r\nworden van een soft linebreak");

  CHECK(toQuotedPrintable("Regel 1.\nRegel 2.\nRegel 3.\n") == "Regel 1.\r\nRegel 2.\r\nRegel 3.\r\n");

  CHECK(toQuotedPrintable("Korte regel.\nIets minder korte regel.\nEn dan hier weer zo'n lange zin die een soft linebreak moet krijgen ergens een beetje aan het einde denk ik.\nEn hier weer een korte regel\n") == "Korte regel.\r\nIets minder korte regel.\r\nEn dan hier weer zo'n lange zin die een soft linebreak moet krijgen ergens =\r\neen beetje aan het einde denk ik.\r\nEn hier weer een korte regel\r\n");
}

TEST_CASE("FTS5")
{
  CHECK(convertToSQLiteFTS5("bert hubert") == "bert hubert");
  CHECK(convertToSQLiteFTS5("D66") == "D66");
  CHECK(convertToSQLiteFTS5("Fox-IT") == "\"Fox-IT\"");
  CHECK(convertToSQLiteFTS5("Fox-IT AND bert hubert") == "\"Fox-IT\" AND bert hubert");
  CHECK(convertToSQLiteFTS5("command.com AND wp.exe NOT windows95") == "\"command.com\" AND \"wp.exe\" NOT windows95");
  CHECK(convertToSQLiteFTS5("NEAR(bert hubert)") == "NEAR ( bert hubert )");
  CHECK(convertToSQLiteFTS5("'s-Gravenhage") == "\"'s-Gravenhage\"");
  CHECK(convertToSQLiteFTS5("D'66") == "D'66"); // I know, I know

  CHECK(convertToSQLiteFTS5("Geschenken, champagne") == "\"Geschenken,\" champagne"); 

  CHECK(convertToSQLiteFTS5("materiële F-35 \"problemen\"") == "materiële \"F-35\" \"problemen\"");
  CHECK(convertToSQLiteFTS5("materiële vereisten") == "materiële vereisten");
  CHECK(convertToSQLiteFTS5("materiële-vereisten") == "\"materiële-vereisten\"");
}

TEST_CASE("Escape")
{
  CHECK(htmlEscape("bert & ernie") == "bert &amp; ernie");
  CHECK(urlEscape("salt & pepper") == "salt%20%26%20pepper");
  CHECK(urlEscape("kaasbroodje") == "kaasbroodje");
}

TEST_CASE("deHTML")
{
  CHECK(deHTML("<html><body>Hallo allemaal!</body></html>") ==
	"  Hallo allemaal!  ");
}


TEST_CASE("Search" * doctest::skip())
{
  SQLiteWriter sqw("tkindex.sqlite3", SQLWFlag::ReadOnly);
  sqw.query("ATTACH DATABASE 'tk.sqlite3' as meta");

  SearchHelper sh(sqw);
  auto ret = sh.search("Werkbezoek Knooppunten internationaal", {"Activiteit", "Toezegging"}, "2024-11-01");
  for(const auto& r : ret) {
    cout<<r.nummer<<" ("<<r.datum<<"): "<<r.onderwerp<<" / " << r.titel<<"\n";
  }
  CHECK(ret.size() == 1);
  
}


TEST_CASE("Get zaken from document" * doctest::skip())
{
  // 2025D02550 -> 2024Z20955
  auto res = getZakenFromDocument("8064a13e-3827-4f30-a657-f0ff85f5a344");
  REQUIRE(res.size() == 1);
  CHECK(res.begin()->first == "2024Z20955");
  CHECK(res.begin()->second == "71009781-8b4c-41ce-a363-6f00ba9fc3ea");
}

TEST_CASE("Test timestamps")
{
  time_t then = getTstampRSSFormat("Fri, 17 Jan 2025 06:07:07 GMT");
  CHECK(then == 1737094027);  
}



TEST_CASE("Send email" * doctest::skip())
{
  string text("* Zoekopdracht motie paulusma:\nhttp://berthub.eu/tkconv/get/2024D49539: Voortgangsbrief beschikbaarheid geneesmiddelen\n\nDit was een bericht van https://berthub.eu/tkconv, ook bekend als OpenTK");

  string html(R"(<meta name="x-apple-disable-message-reformatting" /> 
<div>
<ul>
      
      <li>
        Zoekopdracht motie paulusma:
        <ul>
          <li><a href="https://berthub.eu/tkconv/document.html?nummer=2024D49539">2024D49539</a>: Voortgangsbrief beschikbaarheid geneesmiddelen</li>
        </ul>
      </li>
      
    </ul>
Dit was een bericht van <a href="https://berthub.eu/tkconv/">OpenTK</a>.
</div>)");

  sendEmail("10.0.0.2", "opentk@hubertnet.nl", "bert@hubertnet.nl", "[opentk] nog een test", text, html);
  sendEmail("10.0.0.2", "opentk@hubertnet.nl", "bert@hubertnet.nl", "[opentk] Monitor lid Ines Kostić", text, html);
  

}

