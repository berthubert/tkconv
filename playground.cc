#include "inja.hpp"
#include <iostream>
using namespace std;

int main()
{
  nlohmann::json j = nlohmann::json::array();
  nlohmann::json o = nlohmann::json::object();

  o["id"]=1;
  o["user"]="bert";
  j.push_back(o);

  o["id"]=2;
  o["user"]="jaap & bert";
  j.push_back(o);

  nlohmann::json data;
  data["users"]=j;
  
  inja::render_to(cout, "Hello {{id}} {{user}}\n", o);

  inja::Environment e;
  e.set_html_autoescape(true);
  
  e.render_to(cout, R"(<table>
## for u in users
	<tr><td>{{ loop.index1 }}</td><td>{{ u.user }}</td></tr>
## endfor
</table>
)", data);

  
}
