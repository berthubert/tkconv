#pragma once

#include <functional>
#include <string>

// Parse and transform the given query, or return it unmodified on error.
std::string transformQuery(const std::string& in,
  std::function<std::string(const std::string&)> bareWord = std::identity(),
  std::function<std::string(const std::string&)> quotedWord = std::identity());

// Parse the query, adding quotes to fit FTS5 syntax, or return unmodified.
std::string convertToSQLiteFTS5(const std::string& in);
