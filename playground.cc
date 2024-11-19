#include "thingpool.hh"
#include "inja.hpp"
#include <iostream>
#include <fmt/printf.h>
#include <fmt/os.h>
#include "support.hh"
#include "sqlwriter.hh"
#include <deque>
#include <mutex>

using namespace std;


int main(int argc, char**argv)
{
  cout << getLargeId() << endl;
  cout << getLargeId() << endl;
  cout << getLargeId() << endl;

}
