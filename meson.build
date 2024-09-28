project('tkconv', 'cpp', default_options : ['cpp_std=c++20'])

sqlitedep = dependency('sqlite3', version : '>3')
thread_dep = dependency('threads')
json_dep = dependency('nlohmann_json')
fmt_dep = dependency('fmt', version: '>10', static: true)
pugi_dep = dependency('pugixml')

cpphttplib = dependency('cpp-httplib')
sqlitewriter_dep = dependency('sqlitewriter', static: true)
doctest_dep=dependency('doctest')
argparse_dep = dependency('argparse', version: '>=3')

vcs_ct=vcs_tag(command: ['git', 'describe', '--tags', '--always', '--dirty', '--abbrev=9'], 
      input:'git_version.h.in',
      output:'git_version.h',
      replace_string:'@GIT_VERSION@')

vcs_dep= declare_dependency (sources: vcs_ct)

executable('tkconv', 'tkconv.cc', 
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])

executable('tkparse', 'tkparse.cc', 'support.cc',
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])


executable('tkdisco', 'tkdisco.cc', 
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])

executable('tkgetxml', 'tkgetxml.cc', 
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])


executable('tkbot', 'tkbot.cc', 
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])

executable('tkindex', 'tkindex.cc', 'support.cc',
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])

executable('tkpull', 'tkpull.cc', 'support.cc', 
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])



executable('tkserv', 'tkserv.cc', 'support.cc', 
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])

executable('playground', 'playground.cc', 'support.cc', 
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])


#executable('testrunner', 'testrunner.cc', 'support.cc', 'serv.cc',
#	dependencies: [sqlitedep, json_dep, fmt_dep, bcryptcpp_dep, argparse_dep, sqlitewriter_dep, cpphttplib, doctest_dep, simplesockets_dep])

