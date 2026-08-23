#pragma once

#include <cstdint>
#include <string>
#include <vector>

class SQLiteWriter;

// Spelling corrections using the search index as a dictionary, preferring common terms. 
struct Suggester {
  std::string bytes;

  // Return the given word, spell-corrected or unmodified.
  std::string spell(std::string word);

  // Return the given search query, spell-corrected or unmodified.
  std::string correct_query(std::string query);
};

Suggester suggester_from_table(SQLiteWriter *tkindex_sqlw);
Suggester suggester_from_pairs(const std::vector<std::pair<std::string, uint64_t>> &term_score_pairs);
