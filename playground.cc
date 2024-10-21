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
  ThingPool<SQLiteWriter> tp("tk.sqlite3", SQLWFlag::ReadOnly);
  
  int lim = atoi(argv[1]);
  vector<thread> threads;
  for(int n=0; n < lim; ++n) {
    threads.emplace_back([&tp]() {
      for(int n = 0 ; n < 1000; ++n) {
	auto l = tp.getLease();
	l->queryT("select count(1) from Document");
      }
    });
  }
  
  for(auto& t : threads)
    t.join();
}
