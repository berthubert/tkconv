#include "thingpool.hh"
#include "inja.hpp"
#include <iostream>
#include "siphash.h"
#include <fmt/printf.h>
#include <fmt/os.h>
#include "support.hh"
#include "sqlwriter.hh"
#include <deque>
#include <mutex>

using namespace std;

int main(int argc, char**argv)
{
  SQLiteWriter sqlw("tk.sqlite3", SQLWFlag::ReadOnly);

  auto sfracties=sqlw.queryT("select afkorting from Fractie where datumInactief=''");
  set<string> fracties;
  for(auto& sf : sfracties)
    fracties.insert(eget(sf, "afkorting"));

  //   id         afko     sec
  map<string, map<string, unsigned int>> all;
  auto sprekers=sqlw.queryT("select datum, vergaderingId,titel, afkorting, sum(woordvoerderSeconden + interrumpantSeconden) as sec from VergaderingSpreker, vergadering,Persoon,fractiezetelpersoon,fractiezetel,fractie  where vergadering.id= vergaderingspreker.vergaderingid and persoon.id=vergaderingspreker.persoonid and fractiezetelpersoon.persoonid  = persoon.id and  fractiezetel.id=fractiezetelpersoon.fractiezetelid and fractie.id=fractiezetel.fractieid and (fractiezetelpersoon.totenmet='' or (fractiezetelpersoon.van < datum and datum <= fractiezetelpersoon.totenmet)) and datum > '2023-12-06' group by vergaderingid,afkorting order by datum asc");

  for(auto& s : sprekers) {
    all[eget(s, "vergaderingId")][eget(s, "afkorting")] = get<int64_t>(s["sec"]);
  }

  for(auto& [verg, spreekfracties] : all) {
    for(const auto& f : fracties) {
      if(!spreekfracties.count(f))
	spreekfracties[f]=0;
    }
  }

  fmt::print("vergadering;afko;sec;perc\n");
  for(auto& [verg, spreekfracties] : all) {
    unsigned int sum=0;
    for(auto& [afko,sec] : spreekfracties)
      sum += sec;
    
    for(auto& [afko,sec] : spreekfracties)
      fmt::print("{};{};{};{}\n", verg, afko, sec, 100.0*sec/sum);
  }
}
