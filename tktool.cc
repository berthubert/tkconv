#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <fmt/os.h>
#include <fmt/chrono.h>
#include "support.hh"
#include "sqlwriter.hh"

#include "argparse/argparse.hpp"
#include <regex>
#include <signal.h>
using namespace std;

int main(int argc, char** argv)
{
  argparse::ArgumentParser args("tktool", "0.0");
  
  argparse::ArgumentParser user_command("user");
  user_command.add_description("Manage users");

  argparse::ArgumentParser user_mute_command("mute");
  user_mute_command.add_description("Mute a user");
  user_mute_command.add_argument("email").help("email address of the user").required();
  user_command.add_subparser(user_mute_command);

  argparse::ArgumentParser user_unmute_command("unmute");
  user_unmute_command.add_description("Mute a user");
  user_unmute_command.add_argument("email").help("email address of the user").required();
  user_command.add_subparser(user_unmute_command);

  argparse::ArgumentParser user_show_command("show");
  user_show_command.add_description("Show information about a user");
  user_show_command.add_argument("email").help("email address of the user").required();
  user_command.add_subparser(user_show_command);

  argparse::ArgumentParser user_search_command("search");
  user_search_command.add_description("Search for users");
  user_search_command.add_argument("substr").help("substring to search on").required();
  user_command.add_subparser(user_search_command);

  
  
  args.add_subparser(user_command);


  try {
    args.parse_args(argc, argv);
  }
  catch (const std::runtime_error& err) {
    std::cout << err.what() << std::endl << args;
    std::exit(1);
  }
  if(args.is_subcommand_used(user_command)) {
    SQLiteWriter sqlw("user.sqlite3");
    bool mute =user_command.is_subcommand_used(user_mute_command);
    if(mute || user_command.is_subcommand_used(user_unmute_command)) {
      string email = mute ? user_mute_command.get("email") : user_unmute_command.get("email");
      cout << "Need to " << (mute ? "" : "un") << "mute email '"<< email << "'\n";

      auto res = sqlw.queryT("select * from users where email=?", {email});
      if(res.empty()) {
	cout << "No user with email address '"<<email<<"'\n";
      }
      else {
	sqlw.queryT("update users set muted=? where email=?", {mute ? 1 : 0, email});
      }
    }
    else if(user_command.is_subcommand_used(user_show_command)) {
      string email = user_show_command.get("email");
      auto res = sqlw.queryT("select * from users where email=?", {email});
      if(res.empty()) {
	cout << "No user with email address '"<<email<<"'\n";
      }
      else {
	bool muted = iget(res[0], "muted");
	fmt::print("User {} exists, and is {}muted\n", email, muted ? "" : "not ");
	fmt::print("Sessions:\n");
	auto res2 = sqlw.queryT("select * from sessions where user=? order by lastUseTstamp", {eget(res[0], "user")});
	map<time_t, string> sess;
	for(const auto& s : res2) {
	  fmt::print("\t{:%a, %d %b %Y %H:%M:%S %z}: {} (created {:%a, %d %b %Y %H:%M:%S %z})\n",
		     fmt::localtime(iget(s, "lastUseTstamp")),
		     eget(s, "agent"),
		     fmt::localtime(iget(s, "createTstamp"))
		     );
	  sess[iget(s, "lastUseTstamp")] = eget(s, "id");
	}
	if(!sess.empty()) {
	  fmt::print("Most recent session id {}\n", sess.rbegin()->second);
	}
      }
    }
    else if(user_command.is_subcommand_used(user_search_command)) {
      string substr = "%" + user_search_command.get("substr") + "%";
      auto res = sqlw.queryT("select email from users where email like ?", {substr});
      fmt::print("Users matching search string:\n");
      for(const auto& r : res) {
	fmt::print("{}\n", eget(r, "email"));
      }
    }

  }
}
