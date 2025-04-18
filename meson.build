project('tkconv', 'cpp', default_options : ['cpp_std=c++20'])

sqlitedep = dependency('sqlite3', version : '>=3.48', default_options: ['fts5=enabled'])
thread_dep = dependency('threads')
json_dep = dependency('nlohmann_json')
bcryptcpp_dep = dependency('bcryptcpp', static: true)
fmt_dep = dependency('fmt', version: '>10', static: true)
pugi_dep = dependency('pugixml')
simplesockets_dep = dependency('simplesockets', static: true)
cpphttplib = dependency('cpp-httplib')
sqlitewriter_dep = dependency('sqlitewriter', static: true)
doctest_dep=dependency('doctest')
argparse_dep = dependency('argparse', version: '>=3')

vcs_ct=vcs_tag(command: ['git', 'describe', '--tags', '--always', '--dirty', '--abbrev=9'], 
      input:'git_version.h.in',
      output:'git_version.h',
      replace_string:'@GIT_VERSION@')

vcs_dep= declare_dependency (sources: vcs_ct)

executable('tkconv', 'tkconv.cc', 'support.cc', 'siphash.cc',
	dependencies: [sqlitedep, json_dep, fmt_dep, simplesockets_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])

executable('tkparse', 'tkparse.cc', 'support.cc', 'siphash.cc', 
	dependencies: [sqlitedep, json_dep, fmt_dep, simplesockets_dep,
	cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])


executable('tkdisco', 'tkdisco.cc', 
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])

executable('tkgetxml', 'tkgetxml.cc', 
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])

#executable('roget', 'roget.cc', 'support.cc', 'siphash.cc', 
#	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep, simplesockets_dep,
#	argparse_dep, vcs_dep])


executable('tkbot', 'tkbot.cc', 'support.cc', 'siphash.cc', 'scanmon.cc', 'search.cc',
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib,
	sqlitewriter_dep, pugi_dep, simplesockets_dep,
	argparse_dep, vcs_dep])

executable('tkindex', 'tkindex.cc', 'support.cc', 'siphash.cc',
	dependencies: [sqlitedep, json_dep, fmt_dep,
	simplesockets_dep,
	cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])

executable('tkpull', 'tkpull.cc', 'support.cc', 'siphash.cc',
	dependencies: [sqlitedep, json_dep, fmt_dep, cpphttplib,
	simplesockets_dep,
	sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])

executable('oppull', 'oppull.cc', 'support.cc', 'siphash.cc',
	dependencies: [sqlitedep, json_dep,
	simplesockets_dep,
	fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])


executable('tkserv', 'tkserv.cc', 'support.cc', 'siphash.cc', 'sws.cc', 'users.cc', 'scanmon.cc', 'search.cc',
	dependencies: [sqlitedep, json_dep,
	simplesockets_dep, fmt_dep, cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep, bcryptcpp_dep])

executable('playground', 'playground.cc', 'support.cc', 'siphash.cc',
	dependencies: [sqlitedep, json_dep, fmt_dep,
	simplesockets_dep,
	cpphttplib, sqlitewriter_dep, pugi_dep,
	argparse_dep, vcs_dep])


#executable('playground2', 'playground2.cc', 'support.cc', 'siphash.cc', 'tz.cpp',
#	cpp_args : '-DUSE_OS_TZDB=1',	  
#	dependencies: [sqlitedep, json_dep, fmt_dep,
#	simplesockets_dep,
#	cpphttplib, sqlitewriter_dep, pugi_dep,
#	argparse_dep, vcs_dep])


executable('testrunner', 'testrunner.cc', 'support.cc',  'siphash.cc', 'search.cc', 
dependencies: [sqlitedep, json_dep, fmt_dep, sqlitedep, sqlitewriter_dep, doctest_dep, cpphttplib, simplesockets_dep])

