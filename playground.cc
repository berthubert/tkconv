#include "inja.hpp"
#include <iostream>
using namespace std;

int main()
{

  nlohmann::json data;
  data["pagemeta"]["title"]="";
  data["og"]["title"] = "";
  data["og"]["description"] = "";
  data["og"]["imageurl"] = "";
      
  inja::Environment e;
  e.set_html_autoescape(true);

  string result = e.render_file("./partials/besluiten.html", data);
  cout << result << endl;
  
}
