#include "inja.hpp"
#include <iostream>
#include "siphash.h"
#include <fmt/printf.h>
#include <fmt/os.h>
#include "support.hh"


using namespace std;

int main(int argc, char**argv)
{
  if(argc > 1) {
    for(int n=1 ; n < argc; ++n) {
      fmt::print("mkdir -p op/{} ; mv op/{}.odt op/{}/\n",
		 getSubdirForExternalID(argv[n]),
		 argv[n], getSubdirForExternalID(argv[n]));
    }
  }
}
