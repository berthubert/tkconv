//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once

#include <algorithm>
#include <any>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if !defined(__cplusplus) || __cplusplus < 201703L
#error "Requires complete C++17 support"
#endif

namespace fst {

//-----------------------------------------------------------------------------
// variable byte encoding
//-----------------------------------------------------------------------------

template <typename Val> inline size_t vb_encode_value_length(Val n) {
  auto len = 0u;
  while (n >= 128) {
    len++;
    n >>= 7;
  }
  len++;
  return len;
}

template <typename Val> inline size_t vb_encode_value(Val n, char *out) {
  auto len = 0u;
  while (n >= 128) {
    out[len] = static_cast<char>(n & 0x7f);
    len++;
    n >>= 7;
  }
  out[len] = static_cast<char>(n + 128);
  len++;
  return len;
}

template <typename Val, typename Cont> void vb_encode_value(Val n, Cont &out) {
  while (n >= 128) {
    out.push_back(static_cast<typename Cont::value_type>(n & 0x7f));
    n >>= 7;
  }
  out.push_back(static_cast<typename Cont::value_type>(n + 128));
}

template <typename Val>
inline size_t vb_encode_value_reverse(Val n, char *out) {
  auto len = vb_encode_value(n, out);
  for (auto i = 0u; i < len / 2; i++) {
    std::swap(out[i], out[len - i - 1]);
  }
  return len;
}

template <typename Val>
inline size_t vb_encode_value_reverse(Val n, std::ostream &os) {
  char buf[16];
  auto len = vb_encode_value_reverse(n, buf);
  os.write(buf, len);
  return len;
}

template <typename Val>
inline size_t vb_decode_value_reverse(const char *data, Val &n) {
  auto p = reinterpret_cast<const uint8_t *>(data);
  auto i = 0;
  n = 0;
  auto cnt = 0u;
  while (p[i] < 128) {
    n += (static_cast<Val>(p[i--]) << (7 * cnt++));
  }
  n += (static_cast<Val>(p[i--]) - 128) << (7 * cnt);
  return i * -1;
}

//-----------------------------------------------------------------------------
// lower_bound_index
//-----------------------------------------------------------------------------

template <class T>
inline size_t lower_bound_index(size_t first, size_t last, T less) {
  auto len = last - first;

  while (len > 0) {
    auto half = len >> 1;
    auto middle = first + half;

    if (less(middle)) {
      first = middle;
      first++;
      len = len - half - 1;
    } else {
      len = half;
    }
  }

  return first;
}

//-----------------------------------------------------------------------------
// MurmurHash64B - 64-bit MurmurHash2 for 32-bit platforms
//
// URL:: https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
// License: Public Domain
//-----------------------------------------------------------------------------

inline uint64_t MurmurHash64B(const void *key, size_t len, uint64_t seed) {
  const auto m = uint32_t(0x5bd1e995);
  const auto r = 24u;

  auto h1 = static_cast<uint32_t>(seed) ^ static_cast<uint32_t>(len);
  auto h2 = static_cast<uint32_t>(seed >> 32);

  auto data = reinterpret_cast<const uint32_t *>(key);

  while (len >= 8) {
    auto k1 = *data++;
    k1 *= m;
    k1 ^= k1 >> r;
    k1 *= m;
    h1 *= m;
    h1 ^= k1;
    len -= 4;

    auto k2 = *data++;
    k2 *= m;
    k2 ^= k2 >> r;
    k2 *= m;
    h2 *= m;
    h2 ^= k2;
    len -= 4;
  }

  if (len >= 4) {
    auto k1 = *data++;
    k1 *= m;
    k1 ^= k1 >> r;
    k1 *= m;
    h1 *= m;
    h1 ^= k1;
    len -= 4;
  }

  switch (len) {
  case 3: h2 ^= reinterpret_cast<const unsigned char *>(data)[2] << 16;
  case 2: h2 ^= reinterpret_cast<const unsigned char *>(data)[1] << 8;
  case 1: h2 ^= reinterpret_cast<const unsigned char *>(data)[0]; h2 *= m;
  };

  h1 ^= h2 >> 18;
  h1 *= m;
  h2 ^= h1 >> 22;
  h2 *= m;
  h1 ^= h2 >> 17;
  h1 *= m;
  h2 ^= h1 >> 19;
  h2 *= m;

  auto h = static_cast<uint64_t>(h1);
  h = (h << 32) | h2;
  return h;
}

//-----------------------------------------------------------------------------
// char_to_string
//-----------------------------------------------------------------------------

inline std::string char_to_string(char arc) {
  std::stringstream ss;
  if (arc < 0x20) {
    ss << std::hex << std::setfill('0') << std::setw(2) << (int)(uint8_t)arc
       << std::dec;
  } else {
    ss << arc;
  }
  return ss.str();
}

//-----------------------------------------------------------------------------
// get_common_prefix_length
//-----------------------------------------------------------------------------

inline size_t get_common_prefix_length(std::string_view s1,
                                       std::string_view s2) {
  auto i = 0u;
  while (i < s1.size() && i < s2.size() && s1[i] == s2[i]) {
    i++;
  }
  return i;
}

//-----------------------------------------------------------------------------
// get_prefix_length
//-----------------------------------------------------------------------------

inline bool get_prefix_length(std::string_view s1, std::string_view s2,
                              size_t &l) {
  l = 0;
  while (l < s1.size() && l < s2.size()) {
    auto ch1 = static_cast<uint8_t>(s1[l]);
    auto ch2 = static_cast<uint8_t>(s2[l]);
    if (ch1 < ch2) { break; }
    if (ch1 > ch2) { return false; }
    l++;
  }
  return true;
}

//-----------------------------------------------------------------------------
// hash_bytes (FNV-1a)
//-----------------------------------------------------------------------------

inline void hash_bytes(uint64_t &h, const void *data, size_t len) {
  auto p = static_cast<const uint8_t *>(data);
  for (size_t i = 0; i < len; i++) {
    h = (h ^ p[i]) * 0x100000001b3ULL;
  }
}

constexpr uint64_t kFnvBasis = 0xcbf29ce484222325ULL;

//-----------------------------------------------------------------------------
// OutputTraits
//-----------------------------------------------------------------------------

using none_t = int;

enum class OutputType { invalid = -1, none_t, uint32_t, uint64_t, string };

template <typename output_t> struct OutputTraits {};

template <> struct OutputTraits<none_t> {
  using value_type = none_t;

  static OutputType type() { return OutputType::none_t; }

  static bool empty(value_type val) { return val == 0; }

  static value_type init_value() { return 0; }

  static void hash_value(uint64_t &h, value_type val) {}

  static size_t read_byte_value(const char *p, value_type &val) { return 0; }
};

template <> struct OutputTraits<uint32_t> {
  using value_type = uint32_t;

  static OutputType type() { return OutputType::uint32_t; }

  static bool empty(value_type val) { return val == 0; }

  static value_type init_value() { return 0; }

  static std::string to_string(value_type val) { return std::to_string(val); }

  static void prepend_value(value_type &base, value_type val) { base += val; }

  static value_type get_suffix(value_type a, value_type b) { return a - b; }

  static value_type get_common_prefix(value_type a, value_type b) {
    return std::min(a, b);
  }

  template <typename T> static size_t write_value(T &buff, value_type val) {
    auto p = reinterpret_cast<const char *>(&val);
    buff.insert(buff.begin(), p, p + sizeof(val));
    return sizeof(val);
  }

  static void hash_value(uint64_t &h, value_type val) {
    hash_bytes(h, &val, sizeof(val));
  }

  static size_t get_byte_value_size(value_type val) {
    return vb_encode_value_length(val);
  }

  static void write_byte_value(std::ostream &os, value_type val) {
    vb_encode_value_reverse(val, os);
  }

  static size_t read_byte_value(const char *p, value_type &val) {
    return vb_decode_value_reverse(p, val);
  }
};

template <> struct OutputTraits<uint64_t> {
  using value_type = uint64_t;

  static OutputType type() { return OutputType::uint64_t; }

  static bool empty(value_type val) { return val == 0; }

  static value_type init_value() { return 0; }

  static std::string to_string(value_type val) { return std::to_string(val); }

  static void prepend_value(value_type &base, value_type val) { base += val; }

  static value_type get_suffix(value_type a, value_type b) { return a - b; }

  static value_type get_common_prefix(value_type a, value_type b) {
    return std::min(a, b);
  }

  template <typename T> static size_t write_value(T &buff, value_type val) {
    auto p = reinterpret_cast<const char *>(&val);
    buff.insert(buff.begin(), p, p + sizeof(val));
    return sizeof(val);
  }

  static void hash_value(uint64_t &h, value_type val) {
    hash_bytes(h, &val, sizeof(val));
  }

  static size_t get_byte_value_size(value_type val) {
    return vb_encode_value_length(val);
  }

  static void write_byte_value(std::ostream &os, value_type val) {
    vb_encode_value_reverse(val, os);
  }

  static size_t read_byte_value(const char *p, value_type &val) {
    return vb_decode_value_reverse(p, val);
  }
};

template <> struct OutputTraits<std::string> {
  using value_type = std::string;

  static OutputType type() { return OutputType::string; }

  static bool empty(const value_type &val) { return val.empty(); }

  static value_type init_value() { return std::string(); }

  static value_type to_string(const value_type &val) { return val; }

  static void prepend_value(value_type &base, const value_type &val) {
    base.insert(0, val);
  }

  static value_type get_suffix(const value_type &a, const value_type &b) {
    return a.substr(b.size());
  }

  static value_type get_common_prefix(const value_type &a,
                                      const value_type &b) {
    return a.substr(0, get_common_prefix_length(a, b));
  }

  template <typename T> static size_t write_value(T &buff, value_type val) {
    buff.insert(buff.begin(), val.data(), val.data() + val.size());
    return val.size();
  }

  static void hash_value(uint64_t &h, const value_type &val) {
    hash_bytes(h, val.data(), val.size());
  }

  static size_t get_byte_value_size(const value_type &val) {
    return vb_encode_value_length(val.size()) + val.size();
  }

  static void write_byte_value(std::ostream &os, const value_type &val) {
    os.write(val.data(), val.size());
    vb_encode_value_reverse(static_cast<uint32_t>(val.size()), os);
  }

  static size_t read_byte_value(const char *p, value_type &val) {
    uint32_t str_len = 0;
    auto vb_len = vb_decode_value_reverse(p, str_len);

    val.resize(str_len);
    memcpy(val.data(), p - vb_len - str_len + 1, str_len);

    return vb_len + str_len;
  }
};

//-----------------------------------------------------------------------------
// State
//-----------------------------------------------------------------------------

template <typename output_t> class State {
public:
  struct Transition {
    size_t id;
    bool final;
    output_t state_output;
    output_t output;

    bool operator==(const Transition &rhs) const {
      if (this != &rhs) {
        return id == rhs.id && final == rhs.final &&
               state_output == rhs.state_output && output == rhs.output;
      }
      return true;
    }
  };

  class Transitions {
  public:
    std::vector<char> arcs;
    std::vector<Transition> states_and_outputs;

    bool operator==(const Transitions &rhs) const {
      if (this != &rhs) {
        return arcs == rhs.arcs && states_and_outputs == rhs.states_and_outputs;
      }
      return true;
    }

    size_t size() const { return arcs.size(); }

    bool empty() const { return !size(); }

    const output_t &output(char arc) const {
      auto idx = get_index(arc);
      assert(idx != -1);
      return states_and_outputs[idx].output;
    }

    template <typename Functor> void for_each(Functor fn) const {
      for (auto i = 0u; i < arcs.size(); i++) {
        fn(arcs[i], states_and_outputs[i]);
      }
    }

  private:
    void clear() {
      arcs.clear();
      states_and_outputs.clear();
    }

    // The minimization loop always rewires the most recently added arc.
    void update_last_transition(State<output_t> *state) {
      auto &t = states_and_outputs.back();
      t.id = state->id;
      t.final = state->final;
      t.state_output = state->state_output;
    }

    // The tail initialization always adds a new arc.
    void add_transition(char arc, State<output_t> *state) {
      arcs.push_back(arc);
      auto &t = states_and_outputs.emplace_back(Transition());
      t.id = state->id;
      t.final = state->final;
      t.state_output = state->state_output;
    }

    void set_transition(char arc, State<output_t> *state) {
      auto idx = get_index(arc);
      if (idx == -1) {
        idx = static_cast<int>(arcs.size());
        arcs.push_back(arc);
        states_and_outputs.emplace_back(Transition());
      }
      states_and_outputs[idx].id = state->id;
      states_and_outputs[idx].final = state->final;
      states_and_outputs[idx].state_output = state->state_output;
    }

    void set_output(char arc, const output_t &val) {
      auto idx = get_index(arc);
      states_and_outputs[idx].output = val;
    }

    void insert_output(char arc, const output_t &val) {
      auto idx = get_index(arc);
      auto &output = states_and_outputs[idx].output;
      OutputTraits<output_t>::prepend_value(output, val);
    }

    int get_index(char arc) const {
      for (auto i = 0u; i < arcs.size(); i++) {
        if (arcs[i] == arc) { return static_cast<int>(i); }
      }
      return -1;
    }

    friend class State;
  };

  State(size_t id) : id(id) {}

  const output_t &output(char arc) const { return transitions.output(arc); }

  bool operator==(const State &rhs) const {
    if (this != &rhs) {
      return final == rhs.final && transitions == rhs.transitions &&
             state_output == rhs.state_output;
    }
    return true;
  }

  uint64_t hash() const;

  void set_final(bool final) { this->final = final; }

  void set_transition(char arc, State<output_t> *state) {
    transitions.set_transition(arc, state);
  }

  void update_last_transition(State<output_t> *state) {
    transitions.update_last_transition(state);
  }

  void add_transition(char arc, State<output_t> *state) {
    transitions.add_transition(arc, state);
  }

  void set_output(char arc, const output_t &output) {
    transitions.set_output(arc, output);
  }

  void prepend_suffix_to_output(char arc, const output_t &suffix) {
    transitions.insert_output(arc, suffix);
  }

  void push_to_state_outputs(const output_t &output) { state_output = output; }

  void prepend_suffix_to_state_outputs(const output_t &suffix) {
    OutputTraits<output_t>::prepend_value(state_output, suffix);
  }

  void reuse(size_t state_id) {
    id = state_id;
    set_final(false);
    transitions.clear();
    state_output = output_t{};
  }

  size_t id = -1;
  bool final = false;
  Transitions transitions;
  output_t state_output = output_t{};

private:
  State(const State &) = delete;
  State(State &&) = delete;
};

template <typename output_t> inline uint64_t State<output_t>::hash() const {
  auto h = kFnvBasis;

  transitions.for_each([&](char arc, const State::Transition &t) {
    hash_bytes(h, &arc, sizeof(arc));

    auto val = static_cast<uint32_t>(t.id);
    hash_bytes(h, &val, sizeof(val));

    if (!OutputTraits<output_t>::empty(t.output)) {
      OutputTraits<output_t>::hash_value(h, t.output);
    }
  });

  if (final && !OutputTraits<output_t>::empty(state_output)) {
    OutputTraits<output_t>::hash_value(h, state_output);
  }

  // Final mixing improves the bucket distribution in the dictionary.
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  return h;
}

//-----------------------------------------------------------------------------
// StatePool
//-----------------------------------------------------------------------------

template <typename output_t> class StatePool {
public:
  ~StatePool() {
    for (auto p : object_pool_) {
      delete p;
    }
  }

  State<output_t> *New(size_t state_id = -1) {
    if (!free_list_.empty()) {
      auto p = free_list_.back();
      free_list_.pop_back();
      p->reuse(state_id);
      return p;
    }
    auto p = new State<output_t>(state_id);
    object_pool_.push_back(p);
    return p;
  }

  // Recycled states keep their transition vector capacities, which saves
  // a large number of allocations during the build.
  void Delete(State<output_t> *p) { free_list_.push_back(p); }

private:
  std::vector<State<output_t> *> object_pool_;
  std::vector<State<output_t> *> free_list_;
};

//-----------------------------------------------------------------------------
// Dictionary
//-----------------------------------------------------------------------------

template <typename output_t> class Dictionary {
public:
  // With 'keep_all', the dictionary keeps every minimized state in a
  // growable open addressing table, so no duplicate states are created.
  // Otherwise it works as a fixed size 3-way LRU cache, which bounds the
  // memory usage but produces duplicate states on eviction.
  Dictionary(StatePool<output_t> &state_pool, bool keep_all)
      : state_pool_(state_pool), keep_all_(keep_all) {
    if (keep_all_) {
      table_.resize(kInitialTableSize, {0, nullptr});
    } else {
      buckets_.resize(kBucketCount,
                      {{0, nullptr}, {0, nullptr}, {0, nullptr}});
    }
  }

  State<output_t> *get(uint64_t key, State<output_t> *state) {
    if (keep_all_) {
      auto mask = table_.size() - 1;
      auto i = key & mask;
      while (table_[i].second) {
        // Compare the hash keys first to avoid expensive state comparisons.
        if (table_[i].first == key && *table_[i].second == *state) {
          return table_[i].second;
        }
        i = (i + 1) & mask;
      }
      return nullptr;
    }

    auto id = bucket_id(key);
    auto [first, second, third] = buckets_[id];
    // Compare the hash keys first to avoid expensive state comparisons.
    if (first.second && first.first == key && *first.second == *state) {
      return first.second;
    }
    if (second.second && second.first == key && *second.second == *state) {
      buckets_[id] = std::tuple(second, first, third);
      return second.second;
    }
    if (third.second && third.first == key && *third.second == *state) {
      buckets_[id] = std::tuple(third, first, second);
      return third.second;
    }
    return nullptr;
  }

  void put(uint64_t key, State<output_t> *state) {
    if (keep_all_) {
      if (count_ * 10 >= table_.size() * 7) { grow_table(); }
      auto mask = table_.size() - 1;
      auto i = key & mask;
      while (table_[i].second) {
        i = (i + 1) & mask;
      }
      table_[i] = {key, state};
      count_++;
      return;
    }

    auto id = bucket_id(key);
    auto [first, second, third] = buckets_[id];
    if (third.second) { state_pool_.Delete(third.second); }
    buckets_[id] = std::tuple(Entry{key, state}, first, second);
  }

private:
  StatePool<output_t> &state_pool_;
  bool keep_all_;

  static const auto kBucketCount = 10000u;
  static const auto kInitialTableSize = 1u << 16;

  size_t bucket_id(uint64_t key) const { return key % kBucketCount; }

  using Entry = std::pair<uint64_t, State<output_t> *>;

  void grow_table() {
    std::vector<Entry> old_table(table_.size() * 2, {0, nullptr});
    table_.swap(old_table);
    auto mask = table_.size() - 1;
    for (const auto &entry : old_table) {
      if (!entry.second) { continue; }
      auto i = entry.first & mask;
      while (table_[i].second) {
        i = (i + 1) & mask;
      }
      table_[i] = entry;
    }
  }

  std::vector<std::tuple<Entry, Entry, Entry>> buckets_;
  std::vector<Entry> table_;
  size_t count_ = 0;
};

//-----------------------------------------------------------------------------
// find_minimized
//-----------------------------------------------------------------------------

template <typename output_t>
inline std::pair<bool, State<output_t> *>
find_minimized(State<output_t> *state, Dictionary<output_t> &dictionary) {
  auto h = state->hash();

  auto st = dictionary.get(h, state);
  if (st) { return std::pair(true, st); }

  dictionary.put(h, state);
  return std::pair(false, state);
};

//-----------------------------------------------------------------------------
// get_common_prefix_and_word_suffix
//-----------------------------------------------------------------------------

template <typename output_t>
inline void get_common_prefix_and_word_suffix(const output_t &current_output,
                                              const output_t &output,
                                              output_t &common_prefix,
                                              output_t &word_suffix) {
  common_prefix =
      OutputTraits<output_t>::get_common_prefix(output, current_output);
  word_suffix = OutputTraits<output_t>::get_suffix(output, common_prefix);
}

//-----------------------------------------------------------------------------
// build_fst_core
//-----------------------------------------------------------------------------

enum class Result { Success, EmptyKey, UnsortedKey, DuplicateKey };

template <typename output_t, typename Input, typename Writer>
inline std::pair<Result, size_t>
build_fst_core(const Input &input, Writer &writer, bool need_output,
               bool keep_all_states = false) {
  StatePool<output_t> state_pool;

  Dictionary<output_t> dictionary(state_pool, keep_all_states);
  auto next_state_id = 0u;
  auto error_input_index = 0u;
  auto result = Result::Success;

  // Main algorithm ported from the technical paper
  std::vector<State<output_t> *> temp_states;
  std::string previous_word;
  temp_states.push_back(state_pool.New(next_state_id++));

  input([&](const auto &current_word, const auto &_current_output,
            size_t input_index) {
    auto current_output = _current_output;

    if (current_word.empty()) {
      result = Result::EmptyKey;
      error_input_index = input_index;
      return false;
    }

    // The following loop caluculates the length of the longest common
    // prefix of 'current_word' and 'previous_word'
    size_t prefix_length;
    if (!get_prefix_length(previous_word, current_word, prefix_length)) {
      result = Result::UnsortedKey;
      error_input_index = input_index;
      return false;
    }

    if (previous_word.size() == current_word.size() &&
        previous_word == current_word) {
      result = Result::DuplicateKey;
      error_input_index = input_index;
      return false;
    }

    // We minimize the states from the suffix of the previous word
    for (auto i = previous_word.size(); i > prefix_length; i--) {
      auto [found, state] =
          find_minimized<output_t>(temp_states[i], dictionary);

      auto arc = previous_word[i - 1];

      if (found) {
        next_state_id--;
      } else {
        writer.write(*state, arc);

        // Ownership of the object in temp_states[i] has been moved to the
        // dictionary...
        temp_states[i] = state_pool.New();
      }

      temp_states[i - 1]->update_last_transition(state);
    }

    // This loop initializes the tail states for the current word
    for (auto i = prefix_length + 1; i <= current_word.size(); i++) {
      assert(i <= temp_states.size());
      if (i == temp_states.size()) {
        temp_states.push_back(state_pool.New(next_state_id++));
      } else {
        temp_states[i]->reuse(next_state_id++);
      }
      auto arc = current_word[i - 1];
      temp_states[i - 1]->add_transition(arc, temp_states[i]);
    }

    if (current_word != previous_word) {
      auto state = temp_states[current_word.size()];
      state->set_final(true);
    }

    if (need_output) {
      for (auto j = 1u; j <= prefix_length; j++) {
        auto prev_state = temp_states[j - 1];
        auto arc = current_word[j - 1];

        const auto &output = prev_state->output(arc);

        auto common_prefix = output_t{};
        auto word_suffix = output_t{};
        get_common_prefix_and_word_suffix(current_output, output, common_prefix,
                                          word_suffix);

        prev_state->set_output(arc, common_prefix);

        if (!OutputTraits<output_t>::empty(word_suffix)) {
          auto state = temp_states[j];

          for (auto arc : state->transitions.arcs) {
            state->prepend_suffix_to_output(arc, word_suffix);
          }

          if (state->final) {
            state->prepend_suffix_to_state_outputs(word_suffix);
          }
        }

        current_output =
            OutputTraits<output_t>::get_suffix(current_output, common_prefix);
      }

      if (current_word == previous_word) {
        auto state = temp_states[current_word.size()];
        state->push_to_state_outputs(current_output);
      } else {
        auto state = temp_states[prefix_length];
        auto arc = current_word[prefix_length];
        state->set_output(arc, current_output);
      }
    }

    previous_word = current_word;
    return true;
  });

  if (result != Result::Success) {
    return std::pair(result, error_input_index);
  }

  // Here we are minimizing the states of the last word
  State<output_t> *root = nullptr;
  for (auto i = static_cast<int>(previous_word.size()); i >= 0; i--) {
    auto [found, state] = find_minimized<output_t>(temp_states[i], dictionary);

    auto arc = (i > 0) ? previous_word[i - 1] : 0;

    if (found) {
      next_state_id--;
    } else {
      writer.write(*state, arc);
    }

    if (i > 0) {
      temp_states[i - 1]->update_last_transition(state);
    } else {
      root = state;
    }
  }

  writer.finish(*root);

  return std::pair(Result::Success, error_input_index);
}

//-----------------------------------------------------------------------------
// build_fst
//-----------------------------------------------------------------------------

template <typename output_t, typename Input, typename Writer>
inline std::pair<Result, size_t> build_fst(const Input &input, Writer &writer,
                                           bool need_output, bool sorted,
                                           bool keep_all_states = false) {
  return build_fst_core<output_t>(
      [&](const auto &feeder) {
        if (sorted) {
          size_t input_index = 0;
          for (const auto &item : input) {
            const auto &word = item.first;
            const auto &output = item.second;
            if (!feeder(word, output, input_index)) { break; }
            input_index++;
          }
        } else {
          std::vector<size_t> sorted_indexes(input.size());
          {
            std::iota(sorted_indexes.begin(), sorted_indexes.end(), 0);
            std::sort(sorted_indexes.begin(), sorted_indexes.end(),
                      [&](const auto &a, const auto &b) {
                        return input[a].first < input[b].first;
                      });
          }

          for (auto input_index : sorted_indexes) {
            const auto &[word, output] = input[input_index];
            if (!feeder(word, output, input_index)) { break; }
          }
        }
      },
      writer, need_output, keep_all_states);
}

template <typename Input, typename Writer>
inline std::pair<Result, size_t> build_fst(const Input &input, Writer &writer,
                                           bool need_output, bool sorted,
                                           bool keep_all_states = false) {
  return build_fst_core<uint32_t>(
      [&](const auto &feeder) {
        if (sorted) {
          size_t input_index = 0;
          for (const auto &word : input) {
            if (!feeder(word, static_cast<uint32_t>(input_index),
                        input_index)) {
              break;
            }
            input_index++;
          }
        } else {
          std::vector<size_t> sorted_indexes(input.size());
          {
            std::iota(sorted_indexes.begin(), sorted_indexes.end(), 0);
            std::sort(sorted_indexes.begin(), sorted_indexes.end(),
                      [&](const auto &a, const auto &b) {
                        return input[a] < input[b];
                      });
          }

          for (auto input_index : sorted_indexes) {
            const auto word = input[input_index];
            if (!feeder(word, static_cast<uint32_t>(input_index),
                        input_index)) {
              break;
            }
          }
        }
      },
      writer, need_output, keep_all_states);
}

//-----------------------------------------------------------------------------
// compile
//-----------------------------------------------------------------------------

union FstOpe {
  struct {
    unsigned no_address : 1;
    unsigned last_transition : 1;
    unsigned final : 1;
    unsigned has_output : 1;
    unsigned has_state_output : 1;
    unsigned label_index : 3;
  } data;

  struct {
    unsigned no_address : 1;
    unsigned last_transition : 1;
    unsigned final : 1;
    unsigned has_output : 1;
    unsigned label_index : 4;
  } data_no_state_output;

  struct {
    unsigned no_address : 1;
    unsigned last_transition : 1;
    unsigned final : 1;
    unsigned label_index : 5;
  } data_no_output;

  uint8_t byte = 0;

  FstOpe() = default;
  explicit FstOpe(uint8_t byte) : byte(byte) {}

  size_t label_index(bool need_output, bool need_state_output) const {
    if (!need_output) {
      return data_no_output.label_index;
    } else if (need_state_output) {
      return data.label_index;
    } else {
      return data_no_state_output.label_index;
    }
  }

  void set_label_index(bool need_output, bool need_state_output, size_t index) {
    if (!need_output) {
      data_no_output.label_index = index;
    } else if (need_state_output) {
      data.label_index = index;
    } else {
      data_no_state_output.label_index = index;
    }
  }

  // For char index
  static constexpr size_t char_index_size(bool need_output,
                                          bool need_state_output) {
    return !need_output ? 32 : (need_state_output ? 8 : 16);
  }

  // For jump table
  bool has_jump_table() const { return byte == 0xff || byte == 0xfe; }

  size_t jump_table_element_size() const {
    return (byte == 0xff || byte == 0xfd) ? 2 : 1;
  }

  static uint8_t jump_table_tag(bool need_two_bytes) {
    return need_two_bytes ? 0xff : 0xfe;
  }
};

template <typename output_t, bool need_state_output> struct FstRecord {
  FstOpe ope;

  char label = 0;
  size_t delta = 0;
  bool need_output = false;
  bool omit_label = false; // label is stored in the state's jump table
  const output_t *output = nullptr;
  const output_t *state_output = nullptr;

  size_t byte_size() const {
    auto sz = 1u;
    if (!omit_label &&
        ope.label_index(need_output, need_state_output) == 0) {
      sz += 1;
    }
    if (!ope.data.no_address) { sz += vb_encode_value_length(delta); }
    if (need_output) {
      if (ope.data.has_output) {
        sz += OutputTraits<output_t>::get_byte_value_size(*output);
      }
      if (need_state_output) {
        if (ope.data.has_state_output) {
          sz += OutputTraits<output_t>::get_byte_value_size(*state_output);
        }
      }
    }
    return sz;
  }

  void write(std::ostream &os) {
    if (need_output) {
      if (need_state_output) {
        if (ope.data.has_state_output) {
          OutputTraits<output_t>::write_byte_value(os, *state_output);
        }
      }
      if (ope.data.has_output) {
        OutputTraits<output_t>::write_byte_value(os, *output);
      }
    }
    if (!ope.data.no_address) {
      OutputTraits<uint32_t>::write_byte_value(os,
                                               static_cast<uint32_t>(delta));
    }
    if (!omit_label &&
        ope.label_index(need_output, need_state_output) == 0) {
      os.write(&label, 1);
    }
    os.write(reinterpret_cast<const char *>(&ope.byte), sizeof(ope.byte));
  }
};

struct FstHeader {
  union {
    struct {
      unsigned output_type : 3;
      unsigned need_state_output : 1;
      unsigned jump_table_labels : 1; // jump tables carry a label array
      unsigned hub_table : 1; // hub state addresses are stored in a table
      unsigned reserved : 2;
    } data;

    uint8_t byte;
  } flags;

  uint32_t start_address = 0;
  char char_index[32] = {0};

  uint32_t hub_count = 0;
  const char *hub_table = nullptr;

  bool need_output = false;
  bool need_state_output = false;

  FstHeader() = default;

  FstHeader(OutputType output_type, bool need_state_output,
            size_t start_address, const std::vector<size_t> &char_index_table,
            size_t hub_count = 0)
      : flags{}, start_address(static_cast<uint32_t>(start_address)),
        hub_count(static_cast<uint32_t>(hub_count)),
        need_output{output_type != OutputType::none_t},
        need_state_output{need_state_output} {

    flags.data.output_type = static_cast<uint8_t>(output_type);
    flags.data.need_state_output = need_state_output;
    flags.data.jump_table_labels = 1;
    flags.data.hub_table = hub_count > 0;

    auto size = char_index_size();
    for (auto ch = 0u; ch < 256; ch++) {
      auto index = char_index_table[ch];
      if (0 < index && index < size) {
        char_index[index] = static_cast<char>(ch);
      }
    }
  }

  bool read(const char *byte_code, size_t byte_code_size) {
    auto remaining = byte_code_size;
    if (remaining < sizeof(uint8_t)) { return false; }

    auto p = byte_code + (byte_code_size - sizeof(uint8_t));
    flags.byte = *p--;

    // For performance
    need_output = // needed before char_index_size()
        static_cast<OutputType>(flags.data.output_type) != OutputType::none_t;
    need_state_output = flags.data.need_state_output;

    remaining -= sizeof(uint8_t);

    if (flags.data.hub_table) {
      if (remaining < sizeof(uint32_t)) { return false; }
      memcpy(&hub_count, p - (sizeof(uint32_t) - 1), sizeof(hub_count));
      p -= sizeof(uint32_t);
      remaining -= sizeof(uint32_t);
    }

    if (remaining < sizeof(uint32_t)) { return false; }

    memcpy(&start_address, p - (sizeof(uint32_t) - 1), sizeof(start_address));
    p -= sizeof(uint32_t);

    remaining -= sizeof(uint32_t);
    auto size = char_index_size();
    if (remaining < size) { return false; }

    memcpy(char_index, p - (size - 1), size);
    p -= size;
    remaining -= size;

    if (flags.data.hub_table) {
      if (remaining < hub_count * sizeof(uint32_t)) { return false; }
      hub_table = p - (hub_count * sizeof(uint32_t) - 1);
    }
    return true;
  }

  void write(std::ostream &os) {
    os.write(char_index, char_index_size());
    os.write(reinterpret_cast<char *>(&start_address), sizeof(start_address));
    if (flags.data.hub_table) {
      os.write(reinterpret_cast<char *>(&hub_count), sizeof(hub_count));
    }
    os.write(reinterpret_cast<char *>(&flags.byte), sizeof(flags.byte));
  }

  uint32_t hub_address(size_t index) const {
    uint32_t address;
    memcpy(&address, hub_table + index * sizeof(uint32_t), sizeof(address));
    return address;
  }

  size_t char_index_size() const {
    return FstOpe::char_index_size(need_output, need_state_output);
  }
};

template <typename output_t, bool need_state_output> class FstWriter {
public:
  // With 'single_pass' (which requires building with 'keep_all_states'),
  // the writer collects the states during the build and emits all records
  // in finish(), where the most referenced states (hubs) are known and
  // placed in an address table, so that references to them are encoded as
  // short table indexes instead of long deltas.
  template <typename Input>
  FstWriter(std::ostream &os, bool need_output, bool dump, bool verbose,
            const Input &input, bool single_pass = false)
      : os_(os), need_output_(need_output), dump_(dump), verbose_(verbose),
        single_pass_(single_pass && !dump) {

    initialize_char_index_table(input);

    if (dump_) {
      os << "Address\tArc\tN F L\tNxtAddr";
      if (need_output_) { os << "\tOutput\tStOuts"; }
      os << "\tSize" << std::endl;

      os << "-------\t---\t-----\t-------";
      if (need_output_) { os << "\t------\t------"; }
      os << "\t----" << std::endl;
    }
  }

  ~FstWriter() {
    if (address_table_.empty()) { return; }

    auto start_byte_adress = address_table_.back();

    auto output_type =
        need_output_ ? OutputTraits<output_t>::type() : OutputType::none_t;

    if (!dump_) {
      for (auto id : hub_ids_) {
        auto address =
            static_cast<uint32_t>(address_table_[record_index_map_[id]]);
        os_.write(reinterpret_cast<char *>(&address), sizeof(address));
      }
    }

    FstHeader header(output_type, need_state_output, start_byte_adress,
                     char_index_table_, hub_ids_.size());

    if (!dump_) { header.write(os_); }

    if (verbose_) {
      const size_t char_index_size  = FstOpe::char_index_size(need_output_, need_state_output);
      const size_t hub_table_size =
          hub_ids_.empty() ? 0
                           : (hub_ids_.size() + 1) * sizeof(uint32_t);
      const size_t total_size = address_ + hub_table_size + char_index_size + sizeof(uint32_t) + sizeof(uint8_t);
      const auto unique_char_count =
          std::count_if(std::begin(char_count_), std::end(char_count_),
                        [](auto count) { return count > 0; });
      std::cerr << "# unique char count: " << unique_char_count << std::endl;
      std::cerr << "# state count: " << written_state_count_ << std::endl;
      std::cerr << "# record count: " << address_table_.size() << std::endl;
      std::cerr << "# total size: " << total_size << std::endl;
    }
  }

  void write(const State<output_t> &state, char prev_arc) {
    if (single_pass_) {
      // Just collect the state; the records are emitted in finish() once
      // the reference counts are known. The state object stays alive
      // because the build keeps all states.
      if (state.id >= states_by_id_.size()) {
        states_by_id_.resize(state.id + state.id / 2 + 16, nullptr);
      }
      states_by_id_[state.id] = &state;
      return;
    }

    write_state_records(state, prev_arc);
  }

  void finish(const State<output_t> &root) {
    if (!single_pass_) { return; }

    initialize_hub_ranks();

    // Write the records by a post-order traversal, which reproduces the
    // order of the incremental write: a target state is always written
    // before the states that reference it.
    struct Frame {
      const State<output_t> *state;
      size_t transition_index;
      char arc;
    };

    std::vector<bool> visited(states_by_id_.size(), false);
    std::vector<Frame> stack;

    visited[root.id] = true;
    stack.push_back({&root, 0, 0});

    while (!stack.empty()) {
      auto &frame = stack.back();
      if (frame.transition_index < frame.state->transitions.size()) {
        auto i = frame.transition_index++;
        const auto &t = frame.state->transitions.states_and_outputs[i];
        auto child = states_by_id_[t.id];
        if (child && !visited[t.id] && !child->transitions.empty()) {
          visited[t.id] = true;
          stack.push_back({child, 0, frame.state->transitions.arcs[i]});
        }
      } else {
        write_state_records(*frame.state, frame.arc);
        stack.pop_back();
      }
    }
  }

  void write_state_records(const State<output_t> &state, char prev_arc) {
    auto transition_count = state.transitions.size();
    const auto &[arcs, states_and_outputs] = state.transitions;

    auto char_index_size =
        FstOpe::char_index_size(need_output_, need_state_output);

    std::vector<size_t> jump_table(transition_count);
    auto need_jump_table = transition_count >= 8;

    // Only states with a jump table carry a label array; avoid the
    // allocation for the small states that make up the majority.
    std::vector<char> jump_table_labels;
    if (need_jump_table) { jump_table_labels.resize(transition_count); }

    size_t indexes_sorted_by_bigram_count[256];

    if (!need_jump_table) {
      uint16_t keys[256];
      for (auto i = 0u; i < arcs.size(); i++) {
        indexes_sorted_by_bigram_count[i] = i;
        keys[i] = bigram_key(prev_arc, arcs[i]);
      }

      std::sort(&indexes_sorted_by_bigram_count[0],
                &indexes_sorted_by_bigram_count[arcs.size()],
                [&](auto i1, auto i2) {
                  return bigram_count_[keys[i1]] > bigram_count_[keys[i2]];
                });
    }

    for (auto ri = arcs.size(); ri > 0; ri--) {
      auto i = ri - 1;

      auto arc_i = !need_jump_table ? indexes_sorted_by_bigram_count[i] : i;
      auto arc = arcs[arc_i];
      const auto &t = states_and_outputs[arc_i];

      auto record_index = record_index_of(t.id);
      auto has_address = record_index >= 0;
      auto last_transition = transition_count - 1 == i;
      auto no_address =
          last_transition && has_address &&
          record_index == static_cast<int64_t>(address_table_.size() - 1);

      // If the state has 6 or more transitions, then generate jump table.
      auto generate_jump_table = (i == 0) && need_jump_table;

      FstRecord<output_t, need_state_output> rec;
      rec.need_output = need_output_;
      rec.ope.data.no_address = no_address;
      rec.ope.data.last_transition = last_transition;
      rec.ope.data.final = t.final;

      rec.delta = 0;
      auto next_address = 0u;
      if (!no_address) {
        if (has_address) {
          auto delta = address_ - address_table_[record_index];
          next_address = address_ - delta;
          rec.delta = delta;

          if (!hub_ids_.empty()) {
            // Real deltas are doubled; references to hub states are encoded
            // as (rank * 2 + 1) when that is not longer than the delta.
            rec.delta = delta * 2;
            auto hub_rank = hub_rank_of(t.id);
            if (hub_rank >= 0) {
              auto index_value = static_cast<size_t>(hub_rank) * 2 + 1;
              if (vb_encode_value_length(index_value) <=
                  vb_encode_value_length(rec.delta)) {
                rec.delta = index_value;
              }
            }
          }
        }
      }

      if (need_output_) {
        rec.ope.data.has_output = false;
        if (!OutputTraits<output_t>::empty(t.output)) {
          rec.ope.data.has_output = true;
          rec.output = &t.output;
        }

        if (need_state_output) {
          rec.ope.data.has_state_output = false;
          if (!OutputTraits<output_t>::empty(t.state_output)) {
            rec.ope.data.has_state_output = true;
            rec.state_output = &t.state_output;
          }
        }
      }

      if (need_jump_table) {
        // The label is stored in the state's jump table instead of the
        // record itself. label_index 0 also keeps the ope byte away from
        // the jump table tag values (0xff/0xfe).
        rec.omit_label = true;
        rec.ope.set_label_index(need_output_, need_state_output, 0);
        jump_table_labels[i] = arc;
      } else {
        auto label_index = 0u;
        auto index = char_index_table_[static_cast<uint8_t>(arc)];
        if (index < char_index_size) {
          label_index = index;
        } else {
          rec.label = arc;
        }
        rec.ope.set_label_index(need_output_, need_state_output, label_index);

        // When the ope byte happens to be the same as jump tag byte, change to
        // use '.label' field instead.
        if (rec.ope.has_jump_table()) {
          rec.label = arc;
          rec.ope.set_label_index(need_output_, need_state_output, 0);
        }
      }

      auto byte_size = rec.byte_size();
      auto accessible_address = address_ + byte_size - 1;
      address_table_.push_back(accessible_address);
      address_ += byte_size;

      if (!dump_) {
        rec.write(os_);

        if (need_jump_table) { jump_table[i] = accessible_address; }

        if (generate_jump_table) {
          auto jump_table_element_size = 1;

          for (auto &val : jump_table) {
            val = accessible_address - val;
            if (val > 0xff) { jump_table_element_size = 2; }
          }

          auto jump_table_byte_size =
              1 + vb_encode_value_length(jump_table.size()) +
              jump_table.size() * jump_table_element_size +
              jump_table_labels.size();

          auto need_two_bytes = jump_table_element_size == 2;

          auto jump_table_tag = FstOpe::jump_table_tag(need_two_bytes);

          byte_size += jump_table_byte_size;
          address_table_[address_table_.size() - 1] += jump_table_byte_size;
          address_ += jump_table_byte_size;

          os_.write(jump_table_labels.data(), jump_table_labels.size());

          if (need_two_bytes) {
            write_jump_table<uint16_t>(os_, jump_table);
          } else {
            write_jump_table<uint8_t>(os_, jump_table);
          }

          vb_encode_value_reverse(jump_table.size(), os_);

          os_.write((char *)&jump_table_tag, 1);
        }
      } else {
        os_ << address_table_.back() << "\t";
        os_ << char_to_string(arc) << "\t";

        os_ << (no_address ? "↑" : " ") << ' ' << (t.final ? '*' : ' ') << ' '
            << (last_transition ? "‾" : " ") << "\t";

        if (!no_address) {
          if (next_address > 0) {
            os_ << next_address;
          } else {
            os_ << "x";
          }
        }

        if (need_output_) {
          os_ << "\t";

          if (!OutputTraits<output_t>::empty(t.output)) { os_ << t.output; }
          os_ << "\t";

          if (!OutputTraits<output_t>::empty(t.state_output)) {
            os_ << t.state_output;
          }
        }

        os_ << "\t" << byte_size << std::endl;
      }
    }

    if (!state.transitions.empty()) {
      if (state.id >= record_index_map_.size()) {
        record_index_map_.resize(state.id + state.id / 2 + 16, -1);
      }
      record_index_map_[state.id] =
          static_cast<int64_t>(address_table_.size() - 1);
      written_state_count_++;
    }
  }

private:
  template <typename Input>
  void initialize_char_index_table(const Input &input) {
    char_index_table_.assign(256, 0);

    input([&](const auto &word) {
      char prev = 0;
      for (auto ch : word) {
        char_count_[static_cast<uint8_t>(ch)]++;
        bigram_count_[bigram_key(prev, ch)]++;
        prev = ch;
      }
    });

    struct second_order {
      bool operator()(const std::pair<char, size_t> &x,
                      const std::pair<char, size_t> &y) const {
        return x.second < y.second;
      }
    };

    std::priority_queue<std::pair<char, size_t>,
                        std::vector<std::pair<char, size_t>>, second_order>
        que;

    for (auto ch = 0u; ch < 256; ch++) {
      if (char_count_[ch] > 0) {
        que.push(std::pair(static_cast<char>(ch), char_count_[ch]));
      }
    }

    auto index = 1u;
    while (!que.empty()) {
      auto [ch, count] = que.top();
      char_index_table_[static_cast<uint8_t>(ch)] = index++;
      que.pop();
    }
  }

  template <typename T>
  void write_jump_table(std::ostream &os,
                        const std::vector<size_t> &jump_table) {
    std::vector<T> table(jump_table.size());
    for (auto i = 0u; i < jump_table.size(); i++) {
      table[i] = static_cast<T>(jump_table[i]);
    }
    os_.write((char *)table.data(), table.size() * sizeof(T));
  }

  void initialize_hub_ranks() {
    std::vector<uint32_t> ref_counts(states_by_id_.size(), 0);
    for (auto state : states_by_id_) {
      if (!state) { continue; }
      for (const auto &t : state->transitions.states_and_outputs) {
        ref_counts[t.id]++;
      }
    }

    std::vector<std::pair<size_t, size_t>> candidates; // (count, id)
    for (auto id = 0u; id < states_by_id_.size(); id++) {
      if (ref_counts[id] >= 2 && states_by_id_[id] &&
          !states_by_id_[id]->transitions.empty()) {
        candidates.emplace_back(ref_counts[id], id);
      }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const auto &a, const auto &b) {
                return a.first == b.first ? a.second < b.second
                                          : a.first > b.first;
              });

    if (candidates.size() > kMaxHubCount) { candidates.resize(kMaxHubCount); }

    hub_rank_by_id_.assign(states_by_id_.size(), -1);
    hub_ids_.reserve(candidates.size());
    for (auto i = 0u; i < candidates.size(); i++) {
      hub_rank_by_id_[candidates[i].second] = static_cast<int32_t>(i);
      hub_ids_.push_back(candidates[i].second);
    }
  }

  int64_t record_index_of(size_t id) const {
    return id < record_index_map_.size() ? record_index_map_[id] : -1;
  }

  int32_t hub_rank_of(size_t id) const {
    return id < hub_rank_by_id_.size() ? hub_rank_by_id_[id] : -1;
  }

  std::ostream &os_;
  size_t need_output_ = true;
  size_t dump_ = true;
  size_t verbose_ = true;

  size_t char_count_[256] = {0};
  std::vector<size_t> char_index_table_;

  uint16_t bigram_key(char prev, char cur) const {
    return static_cast<uint16_t>(prev) << 8 | static_cast<uint16_t>(cur);
  }
  std::vector<size_t> bigram_count_ = std::vector<size_t>(65536, 0);

  std::vector<int64_t> record_index_map_; // by state id, -1 = absent
  size_t written_state_count_ = 0;

  size_t address_ = 0;
  std::vector<size_t> address_table_;

  static constexpr size_t kMaxHubCount = 1024;
  std::vector<int32_t> hub_rank_by_id_; // by state id, -1 = not a hub
  std::vector<size_t> hub_ids_;

  bool single_pass_ = false;
  std::vector<const State<output_t> *> states_by_id_;
};

template <typename output_t, typename Input>
inline std::pair<Result, size_t> compile(const Input &input, std::ostream &os,
                                         bool sorted, bool verbose = false) {
  FstWriter<output_t, true> writer(os, true, false, verbose,
                                   [&](const auto &feeder) {
                                     for (const auto &[word, _] : input) {
                                       feeder(word);
                                     }
                                   },
                                   /*single_pass=*/true);
  return build_fst<output_t>(input, writer, true, sorted,
                             /*keep_all_states=*/true);
}

template <typename Input>
inline std::pair<Result, size_t> compile(const Input &input, std::ostream &os,
                                         bool need_output, bool sorted,
                                         bool verbose = false) {
  FstWriter<uint32_t, false> writer(os, need_output, false, verbose,
                                    [&](const auto &feeder) {
                                      for (const auto &word : input) {
                                        feeder(word);
                                      }
                                    },
                                    /*single_pass=*/true);
  return build_fst(input, writer, need_output, sorted,
                   /*keep_all_states=*/true);
}

template <typename output_t, typename Input>
inline std::pair<Result, size_t> dump(const Input &input, std::ostream &os,
                                      bool sorted, bool verbose = false) {
  FstWriter<output_t, true> writer(os, true, true, verbose,
                                   [&](const auto &feeder) {
                                     for (const auto &[word, _] : input) {
                                       feeder(word);
                                     }
                                   });
  return build_fst<output_t>(input, writer, true, sorted);
}

template <typename Input>
inline std::pair<Result, size_t> dump(const Input &input, std::ostream &os,
                                      bool need_output, bool sorted,
                                      bool verbose = false) {
  FstWriter<uint32_t, true> writer(os, need_output, true, verbose,
                                   [&](const auto &feeder) {
                                     for (const auto &word : input) {
                                       feeder(word);
                                     }
                                   });
  return build_fst(input, writer, need_output, sorted);
}

//-----------------------------------------------------------------------------
// dot
//-----------------------------------------------------------------------------

template <typename output_t> class DotWriter {
public:
  DotWriter(std::ostream &os) : os_(os) {
    os_ << "digraph{" << std::endl;
    os_ << "  rankdir = LR;" << std::endl;
  }

  ~DotWriter() { os_ << "}" << std::endl; }

  void write(const State<output_t> &state, char prev_arc) {
    if (state.final) {
      auto state_output = OutputTraits<output_t>::init_value();
      ;
      if (!OutputTraits<output_t>::empty(state.state_output)) {
        state_output = state.state_output;
      }
      os_ << "  s" << state.id << " [ shape = doublecircle, xlabel = \""
          << state_output << "\" ];" << std::endl;
    } else {
      os_ << "  s" << state.id << " [ shape = circle ];" << std::endl;
    }

    state.transitions.for_each(
        [&](auto arc, const typename State<output_t>::Transition &t) {
          std::string label;
          label += arc;
          os_ << "  s" << state.id << "->s" << t.id << " [ label = \"" << label;
          if (!OutputTraits<output_t>::empty(t.output)) {
            os_ << " (" << t.output << ")";
          }
          os_ << "\" fontcolor = red ];" << std::endl;
        });
  }

  void finish(const State<output_t> &root) {}

private:
  std::ostream &os_;
};

template <typename output_t, typename Input>
inline std::pair<Result, size_t> dot(const Input &input, std::ostream &os,
                                     bool sorted) {

  DotWriter<output_t> writer(os);
  return build_fst<output_t>(input, writer, true, sorted);
}

template <typename Input>
inline std::pair<Result, size_t> dot(const Input &input, std::ostream &os,
                                     bool need_output, bool sorted) {
  DotWriter<uint32_t> writer(os);
  return build_fst(input, writer, need_output, sorted);
}

//-----------------------------------------------------------------------------
// get_output_type
//-----------------------------------------------------------------------------

inline OutputType get_output_type(const char *byte_code,
                                  size_t byte_code_size) {
  FstHeader header;
  if (!header.read(byte_code, byte_code_size)) { return OutputType::invalid; }
  return static_cast<OutputType>(header.flags.data.output_type);
}

template <typename T> OutputType get_output_type(const T &byte_code) {
  return get_output_type(byte_code.data(), byte_code.size());
}

//-----------------------------------------------------------------------------
// levenshtein_distance
//-----------------------------------------------------------------------------

inline double cost_replace(std::string_view from, size_t i, std::string_view to,
                           size_t j) {
  auto c1 = from[i];
  auto c2 = to[j];

  if (c1 == c2) return 0;

  // one char similar sound...
  {
    const char *similers[] = {
        "ao",
        "ae",
        "iy",
        "ou",
    };

    for (auto s : similers) {
      if ((c1 == s[0] && c2 == s[1]) || (c1 == s[1] && c2 == s[0])) return 0.25;
    }
  }

  if (i + 1 < from.size() && j + 1 < to.size()) {
    auto cn1 = from[i + 1];
    auto cn2 = to[j + 1];

    // Transposed chars...
    if (c1 == cn2 && c2 == cn1) return 0;

    // two chars similar sound...
    {
      const char *similers[] = {
          "irer",
          "urer",
          "irur",
          "erar",
      };

      for (auto s : similers) {
        if ((c1 == s[0] && cn1 == s[1] && c2 == s[2] && cn2 == s[3]) ||
            (c1 == s[2] && cn1 == s[3] && c2 == s[0] && cn2 == s[1]))
          return 0;
      }
    }
  }

  return 1.0;
}

inline double cost_insert(std::string_view to, size_t j) {
  if (j + 1 < to.size() && to[j] == to[j + 1]) return 0.5;
  return 1.0;
}

inline double cost_delete(std::string_view from, size_t i) {
  if (i + 1 < from.size() && from[i] == from[i + 1]) return 0.5;
  return 1.0;
}

inline double levenshtein_distance(std::string_view from, std::string_view to) {
  std::vector<std::vector<double>> m(from.size() + 1);

  for (size_t i = 0; i < m.size(); i++)
    m[i].assign(to.size() + 1, 0);

  for (size_t i = 0; i < m.size(); i++)
    m[i][0] = i;
  for (size_t j = 0; j < m[0].size(); j++)
    m[0][j] = j;

  for (size_t i = 0; i < from.size(); i++)
    for (size_t j = 0; j < to.size(); j++) {
      m[i + 1][j + 1] =
          std::min(m[i][j + 1] + cost_insert(to, j),                  // insert
                   std::min(m[i + 1][j] + cost_delete(from, i),       // delete
                            m[i][j] + cost_replace(from, i, to, j))); // replace
    }

  auto d = m.back().back();
  return 1.0 - (d / std::max<double>(from.size(), to.size()));
}

inline size_t max_range(std::string_view s1, std::string_view s2) {
  return (std::max(s1.length(), s2.length()) / 2) - 1;
}

inline bool common_string(std::string_view s1, std::string_view s2,
                          std::string &cs) {
  auto r = max_range(s1, s2);

  for (size_t i = 0; i < s1.length(); i++) {
    auto beg = std::max<int>(0, (int)i - (int)r);
    auto end = std::min(s2.length(), (i + r + 1));

    auto c1 = s1[i];
    for (size_t j = beg; j < end; j++) {
      if (c1 == s2[j]) {
        cs += c1;
        break;
      }
    }
  }

  return !cs.empty();
}

inline size_t commn_prefix_len(std::string_view s1, std::string_view s2) {
  auto len = std::min(s1.length(), s2.length());
  size_t i = 0;
  for (; i < len && s1[i] == s2[i]; i++)
    ;
  return i;
}

//-----------------------------------------------------------------------------
// jaro_winkler_distance
//-----------------------------------------------------------------------------

inline double jaro_distance(std::string_view s1, std::string_view s2) {
  std::string cs1;
  if (!common_string(s1, s2, cs1)) return 0;

  std::string cs2;
  if (!common_string(s2, s1, cs2)) return 0;

  double t = 0;
  auto end = std::min(cs1.length(), cs2.length());
  for (size_t i = 0; i < end; i++)
    if (cs1[i] != cs2[i]) t += 1;
  t /= 2;

  auto m = static_cast<double>(cs1.length());

  return ((m / s1.length()) + (m / s2.length()) + ((m - t) / m)) / 3;
}

inline double jaro_winkler_distance(std::string_view s1, std::string_view s2) {
  double dj = jaro_distance(s1, s2);
  if (dj) {
    auto l = static_cast<double>(commn_prefix_len(s1, s2));
    const auto p = 0.1;
    return dj + (l * p * (1.0 - dj));
  }
  return 0.0;
}

//-----------------------------------------------------------------------------
// matcher
//-----------------------------------------------------------------------------

template <typename output_t> class matcher {
public:
  using output_type = output_t;

  matcher(const char *byte_code, size_t byte_code_size)
      : byte_code_(byte_code), byte_code_size_(byte_code_size) {

    if (!header_.read(byte_code, byte_code_size)) { return; }

    if (static_cast<OutputType>(header_.flags.data.output_type) !=
        OutputTraits<output_t>::type()) {
      return;
    }

    is_valid_ = true;

    build_root_dispatch();
  }

  operator bool() const { return is_valid_; }

  void set_trace(bool on) { trace_ = on; }

  bool contains(std::string_view sv) const {
    return matcher<output_t>::match(sv.data(), sv.size());
  }

protected:
  // The callbacks are template parameters (with std::nullptr_t defaults), so
  // no std::function is constructed per query and the calls are inlined.
  template <typename OutputsFn = std::nullptr_t,
            typename PrefixesFn = std::nullptr_t>
  bool match(const char *str, size_t len, OutputsFn outputs = nullptr,
             PrefixesFn prefixes = nullptr) const {
    constexpr auto has_outputs = !std::is_null_pointer_v<OutputsFn>;
    constexpr auto has_prefixes = !std::is_null_pointer_v<PrefixesFn>;

    if (trace_) {
      std::cout << "Char\tAddress\tArc\tN F L\tNxtAddr\tOutput\tStOuts\tSize"
                << std::endl;
      std::cout << "----\t-------\t---\t-----\t-------\t------\t------\t----"
                << std::endl;
    }

    auto ret = false;
    auto output = output_t{};

    auto address = header_.start_address;
    auto i = 0u;
    auto arc_in_jump_table = false;

    // Every query passes through the root state; resolve the first
    // character with the precomputed dispatch table instead of the root's
    // jump table binary search.
    if (has_root_dispatch_ && len > 0) {
      address = root_dispatch_[static_cast<uint8_t>(str[0])];
      if (!address) { return false; }
      arc_in_jump_table = true;
    }

    while (i < len) {
      auto ch = static_cast<uint8_t>(str[i]);
      auto state_output = output_t{};

      auto end = byte_code_ + address;
      auto p = end;

      auto ope = FstOpe(*p--);

      if (ope.has_jump_table()) {
        auto jump_table_element_size = ope.jump_table_element_size();
        size_t jump_table_count = 0;
        auto vb_len = vb_decode_value_reverse(p, jump_table_count);
        p -= vb_len;
        p -= jump_table_count * jump_table_element_size;

        auto jump_table = p;

        if (header_.flags.data.jump_table_labels) {
          // The labels are stored contiguously next to the jump table, so
          // the binary search only touches sequential memory.
          auto labels = reinterpret_cast<const uint8_t *>(p) + 1 -
                        jump_table_count;

          auto jump_table_byte_size =
              1 + vb_len + jump_table_count * jump_table_element_size +
              jump_table_count;

          auto found = lower_bound_index(
              0, jump_table_count, [&](auto i) { return labels[i] < ch; });

          if (found < jump_table_count && labels[found] == ch) {
            auto offset =
                lookup_jump_table(jump_table, found, jump_table_element_size);
            address -= offset + jump_table_byte_size;
            arc_in_jump_table = true;
          } else {
            break;
          }
          continue;
        }

        auto jump_table_byte_size =
            1 + vb_len + jump_table_count * jump_table_element_size;

        auto base_address = byte_code_ + address - jump_table_byte_size;

        auto get_arc = [&](auto i) -> uint8_t {
          auto p = base_address -
                   lookup_jump_table(jump_table, i, jump_table_element_size);
          auto ope = FstOpe(*p--);
          return read_arc(ope, p);
        };

        auto found = lower_bound_index(0, jump_table_count,
                                       [&](auto i) { return get_arc(i) < ch; });

        if (found < jump_table_count && get_arc(found) == ch) {
          auto offset =
              lookup_jump_table(jump_table, found, jump_table_element_size);
          address -= offset + jump_table_byte_size;
        } else {
          break;
        }
        continue;
      }

      uint8_t arc;
      if (arc_in_jump_table) {
        // The record was reached through a jump table hit, so its label is
        // already verified and the record itself carries no label byte.
        arc = ch;
        arc_in_jump_table = false;
      } else {
        arc = read_arc(ope, p);
      }

      uint32_t delta, hub_next_address;
      bool has_hub_next_address;
      read_delta(ope, p, delta, hub_next_address, has_hub_next_address);

      auto output_suffix = output_t{};
      if (ope.data.has_output) {
        p -= OutputTraits<output_t>::read_byte_value(p, output_suffix);
      }

      if (header_.need_state_output) {
        if (ope.data.has_state_output) {
          p -= OutputTraits<output_t>::read_byte_value(p, state_output);
        }
      }

      auto byte_size = std::distance(p, end);

      auto next_address = 0u;
      if (!ope.data.no_address) {
        if (has_hub_next_address) {
          next_address = hub_next_address;
        } else if (delta) {
          next_address = address - byte_size - delta + 1;
        }
      } else {
        next_address = address - byte_size;
      }

      if (trace_) {
        std::cout << char_to_string(ch) << "\t";
        std::cout << address << "\t";
        std::cout << arc << "\t";
        std::cout << (ope.data.no_address ? "↑" : " ") << ' '
                  << (ope.data.final ? '*' : ' ') << ' '
                  << (ope.data.last_transition ? "‾" : " ") << "\t";

        // Next Address
        if (next_address) {
          std::cout << next_address;
        } else {
          std::cout << "x";
        }
        std::cout << "\t";

        if (ope.data.has_output) { std::cout << output_suffix; }
        std::cout << "\t";

        if (header_.need_state_output) {
          if (ope.data.has_state_output) { std::cout << state_output; }
        }
        std::cout << "\t";

        std::cout << byte_size;
        std::cout << std::endl;
      }

      if (ch == arc) {
        output += output_suffix;
        i++;
        if (ope.data.final) {
          if constexpr (has_prefixes) {
            if (OutputTraits<output_t>::empty(state_output)) {
              prefixes(i, output);
            } else {
              prefixes(i, output + state_output);
            }
            ret = true;
          }
          if (i == len) {
            if constexpr (has_outputs) {
              if (OutputTraits<output_t>::empty(state_output)) {
                outputs(output);
              } else {
                outputs(output + state_output);
              }
            }
            ret = true;
            break;
          }
        }
        if (!next_address) { break; }
        address = next_address;
      } else {
        if (ope.data.last_transition) { break; }
        address -= byte_size;
      }
    }

    return ret;
  }

  template <typename T, typename U>
  void depth_first_visit(uint32_t address, const std::string &partial_word,
                         const output_t &partial_output, const T &transit,
                         U accept,
                         std::string_view prefix = std::string_view()) const {

    const char *jump_table_labels = nullptr;
    size_t jump_table_label_index = 0;

    while (true) {
      auto state_output = output_t{};

      auto end = byte_code_ + address;
      auto p = end;

      auto ope = FstOpe(*p--);

      if (ope.has_jump_table()) {
        auto jump_table_element_size = ope.jump_table_element_size();
        size_t jump_table_count = 0;
        auto vb_len = vb_decode_value_reverse(p, jump_table_count);
        p -= vb_len;
        p -= jump_table_count * jump_table_element_size;

        if (header_.flags.data.jump_table_labels) {
          // The records of this state carry no label bytes; remember the
          // label array and read the labels from it while iterating.
          jump_table_labels = p + 1 - jump_table_count;
          jump_table_label_index = 0;
          p -= jump_table_count;
        }

        address -= std::distance(p, end);
        continue;
      }

      char arc;
      if (jump_table_labels) {
        arc = jump_table_labels[jump_table_label_index++];
      } else {
        arc = read_arc(ope, p);
      }

      uint32_t delta, hub_next_address;
      bool has_hub_next_address;
      read_delta(ope, p, delta, hub_next_address, has_hub_next_address);

      auto output_suffix = output_t{};
      if (ope.data.has_output) {
        p -= OutputTraits<output_t>::read_byte_value(p, output_suffix);
      }

      if (header_.need_state_output) {
        if (ope.data.has_state_output) {
          p -= OutputTraits<output_t>::read_byte_value(p, state_output);
        }
      }

      auto byte_size = std::distance(p, end);

      auto next_address = 0u;
      if (!ope.data.no_address) {
        if (has_hub_next_address) {
          next_address = hub_next_address;
        } else if (delta) {
          next_address = address - byte_size - delta + 1;
        }
      } else {
        next_address = address - byte_size;
      }

      auto atm = transit; // copy
      atm.step(arc);

      auto word = partial_word + arc;
      auto output = partial_output + output_suffix;

      if (ope.data.final) {
        if (atm.is_match()) {
          if (prefix.empty() || (prefix.size() == 1 && prefix.front() == arc)) {
            auto should_append_state_output = false;
            if (OutputTraits<output_t>::type() != OutputType::none_t) {
              if (!OutputTraits<output_t>::empty(state_output)) {
                should_append_state_output = true;
              }
            }
            accept(word,
                   should_append_state_output ? output + state_output : output);
          }
        }
      }

      if (next_address) {
        if ((prefix.empty() || prefix.front() == arc) && atm.can_match()) {
          depth_first_visit(next_address, word, output, atm, accept,
                            prefix.empty() ? prefix : prefix.substr(1));
        }
      }

      if (ope.data.last_transition) { break; }
      address -= byte_size;
    }
  }

  char read_arc(FstOpe ope, const char *&p) const {
    auto index =
        ope.label_index(header_.need_output, header_.need_state_output);
    return index == 0 ? *p-- : header_.char_index[index];
  }

  void read_delta(FstOpe ope, const char *&p, uint32_t &delta,
                  uint32_t &hub_next_address,
                  bool &has_hub_next_address) const {
    delta = 0;
    hub_next_address = 0;
    has_hub_next_address = false;
    if (!ope.data.no_address) {
      p -= vb_decode_value_reverse(p, delta);
      if (header_.flags.data.hub_table) {
        if (delta & 1) {
          // Odd values are hub table indexes.
          hub_next_address = header_.hub_address(delta >> 1);
          has_hub_next_address = true;
          delta = 0;
        } else {
          delta >>= 1;
        }
      }
    }
  }

  size_t lookup_jump_table(const char *p, size_t index,
                           size_t element_size) const {
    if (element_size == 2) {
      return reinterpret_cast<const uint16_t *>(p + 1)[index];
    } else {
      return reinterpret_cast<const uint8_t *>(p + 1)[index];
    }
  }

  // If the root state has a jump table with labels, precompute a direct
  // 256 entry 'label -> record address' table for it. The byte code is
  // not affected; this only trades 1KB of memory per matcher for the
  // root's binary search on every query.
  void build_root_dispatch() {
    auto address = header_.start_address;
    auto p = byte_code_ + address;

    auto ope = FstOpe(*p--);
    if (!ope.has_jump_table() || !header_.flags.data.jump_table_labels) {
      return;
    }

    auto element_size = ope.jump_table_element_size();
    size_t count = 0;
    auto vb_len = vb_decode_value_reverse(p, count);
    p -= vb_len;
    p -= count * element_size;

    auto jump_table = p;
    auto labels = reinterpret_cast<const uint8_t *>(p) + 1 - count;
    auto jump_table_byte_size = 1 + vb_len + count * element_size + count;

    root_dispatch_.fill(0);
    for (size_t i = 0; i < count; i++) {
      auto offset = lookup_jump_table(jump_table, i, element_size);
      root_dispatch_[labels[i]] =
          static_cast<uint32_t>(address - (offset + jump_table_byte_size));
    }
    has_root_dispatch_ = true;
  }

  const char *byte_code_;
  const size_t byte_code_size_;

  FstHeader header_;
  bool is_valid_ = false;
  bool trace_ = false;

  bool has_root_dispatch_ = false;
  std::array<uint32_t, 256> root_dispatch_{};

  // Suggestion
  template <typename T>
  decltype(auto) suggest_core(std::string_view word, const T &matcher) const {
    using R =
        typename std::conditional<T::has_output,
                                  std::tuple<double, std::string, output_t>,
                                  std::pair<double, std::string>>::type;

    std::vector<R> suggestions;

    size_t min_edits = 2;
    size_t max_edits = 6;

    for (size_t edits = min_edits; edits <= max_edits; edits++) {
      auto results = matcher.edit_distance_search(word, edits);

      if (results.size() >= 2) {
        for (const auto &result : results) {
          std::string candidate;
          if constexpr (T::has_output) {
            candidate = result.first;
          } else {
            candidate = result;
          }
          if (candidate != word) {
            auto jw = jaro_winkler_distance(word, candidate);
            auto le = levenshtein_distance(word, candidate);
            auto similarity = jw * le;
            if constexpr (T::has_output) {
              suggestions.emplace_back(
                  std::tuple(similarity, candidate, result.second));
            } else {
              suggestions.emplace_back(std::pair(similarity, candidate));
            }
          }
        }

        if (!suggestions.empty()) {
          std::sort(suggestions.begin(), suggestions.end(),
                    [](const auto &a, const auto &b) {
                      return std::get<0>(a) == std::get<0>(b)
                                 ? std::get<1>(a) < std::get<1>(b)
                                 : std::get<0>(a) > std::get<0>(b);
                    });
          break;
        }
      }
    }

    return suggestions;
  }
};

//-----------------------------------------------------------------------------
// LevenshteinAutomaton
//-----------------------------------------------------------------------------

class LevenshteinAutomaton {
public:
  LevenshteinAutomaton(std::string_view sv, size_t max_edits,
                       size_t insert_cost, size_t delete_cost,
                       size_t replace_cost)
      : s_(std::make_shared<const std::u32string>(decode(sv))),
        max_edits_(max_edits), insert_cost_(insert_cost),
        delete_cost_(delete_cost), replace_cost_(replace_cost),
        banded_(insert_cost >= 1 && delete_cost >= 1) {
    state_.init(s_->size() + 1);
    // Clamped as it is filled, so that a cell the band has not reached yet
    // already holds cap, the value the recurrence would have left there.
    // Clamping changes no result on its own -- a cell at or above cap is
    // indistinguishable once step() clamps, which is also why this is
    // harmless when !banded_ -- it is what makes "outside the band" and
    // "equal to cap" the same statement.
    auto cap = max_edits_ + 1;
    size_t i = 0;
    for (auto &cell : state_) {
      cell = std::min(i++, cap);
    }
  }

  // depth_first_visit copies the automaton once per arc it visits, so this
  // has to stay cheap: s_ is shared (the decoded query never changes after
  // construction), leaving only the DP row and a small fixed-size
  // pending-codepoint buffer (no allocation) to actually copy.
  LevenshteinAutomaton(const LevenshteinAutomaton &rhs) = default;

  void step(char c) {
    // Bytes accumulate until a whole codepoint is available, so that a
    // multi-byte character is never scored as several edits. The buffer stops
    // at 4 bytes because decode_codepoint only ever inspects the lead byte's
    // shape: once 4 bytes have failed to decode, no further byte can make it
    // succeed, and the automaton is permanently non-matching either way.
    // Refusing the extra bytes just keeps that state bounded, instead of
    // accumulating the rest of a binary key.
    if (u8len_ < sizeof(u8buf_)) { u8buf_[u8len_++] = c; }
    char32_t cp;
    if (!decode_codepoint(std::string_view(u8buf_, u8len_), cp)) { return; }
    u8len_ = 0;

    // The DP row is updated in place, left to right: at iteration i,
    // row[..i - 1] already hold their new values and row[i..] still hold the
    // old ones, with prev_old carrying the old row[i - 1] that the replace
    // term needs after that cell was overwritten. This avoids
    // allocating a scratch row on every codepoint, which step() otherwise
    // spends most of its time on. Clamping to max_edits_ + 1 as values are
    // written (rather than in one pass at the end) yields identical rows:
    // min(min(u, cap) + insert_cost_, ..., cap) ==
    // min(u + insert_cost_, ..., cap) for any non-negative cost.
    //
    // The row minimum is accumulated here rather than rescanned in
    // can_match(), which the traversal calls once per arc just like step().
    //
    // row, sp and slen are all hoisted into locals: writing through row is a
    // size_t store, which the compiler must assume could alias the query
    // string's own size and data members, so it would otherwise reload them
    // on every iteration.
    const auto *sp = s_->data();
    const auto slen = s_->size();
    const auto cap = max_edits_ + 1;
    auto *row = state_.data();

    // Only the diagonal band [depth - max_edits_, depth + max_edits_] can
    // still hold a value below cap: reaching column j after consuming depth
    // codepoints takes at least |depth - j| inserts or deletes, so once both
    // of those cost at least 1, every cell outside the band is already
    // clamped and recomputing it would only write cap over cap. When either
    // cost can be 0 that bound does not hold, and the band spans the whole
    // row so that there is a single loop to reason about.
    size_t lo = 0;
    auto hi = slen;
    if (banded_) {
      depth_++;
      lo = depth_ > max_edits_ ? depth_ - max_edits_ : 0;
      hi = std::min(slen, depth_ + max_edits_);
    }

    // The cell just left of the band supplies the replace term for the band's
    // first cell, so it is read before being overwritten. At the top of the
    // row that cell is row[0] and it takes its usual + 1; once the band has
    // moved it is the cell the band just left behind, retired to cap -- the
    // loop below reads that cap straight back as its own insert term, and
    // is_match reads it once the band has moved past the end of the query.
    // lo advances one step at a time, so every cell the band leaves gets
    // retired; past the end of the row there is nothing left to retire, and
    // the loop is empty because hi never exceeds slen.
    auto first = std::max<size_t>(lo, 1);
    auto prev_old = cap;
    auto row_min = cap; // everything outside the band is clamped
    if (first <= slen + 1) {
      prev_old = row[first - 1];
      row[first - 1] = lo == 0 ? std::min(prev_old + 1, cap) : cap;
      row_min = row[first - 1];
    }

    for (auto i = first; i <= hi; i++) {
      auto cur_old = row[i];
      auto cost = (sp[i - 1] == cp) ? 0 : replace_cost_;
      auto edits = std::min(
          {row[i - 1] + insert_cost_, prev_old + cost, cur_old + delete_cost_});
      row[i] = std::min(edits, cap);
      row_min = std::min(row_min, row[i]);
      prev_old = cur_old;
    }
    min_ = row_min;
  }

  bool is_match() const {
    if (u8len_ > 0) { return false; }
    return state_.back() <= max_edits_;
  }

  bool can_match() const { return min_ <= max_edits_; }

private:
  // The DP row. depth_first_visit copies the automaton once per arc, so a
  // std::vector here would mean a malloc/free pair per arc; a query short
  // enough to fit -- nearly all of them -- keeps its row inline instead.
  // Longer queries spill to the heap so the class stays general.
  //
  // data() is recomputed from n_ rather than cached in a member pointer, so
  // there is nothing to fix up after a copy and the copy constructor can stay
  // defaulted. Copying the whole inline array unconditionally measured faster
  // than copying only the live cells: a fixed-size copy compiles to a straight
  // move sequence, while a length-dependent one does not.
  //
  // There are deliberately no move operations. A defaulted move would take
  // heap_ out of a spilled row while leaving n_ still claiming it, so data()
  // would return nullptr; LevenshteinAutomaton's user-declared copy
  // constructor suppresses its own implicit moves, which is what keeps that
  // unreachable today.
  class Row {
  public:
    // Sizes the row once, at construction. Cells are not carried across the
    // inline/heap boundary, so this is not a general resize.
    void init(size_t n) {
      n_ = n;
      if (n > kInline) { heap_.resize(n); }
    }

    // Callers in a loop should hoist data() rather than indexing repeatedly:
    // it has to branch on whether the row spilled, and the compiler cannot
    // prove that stays fixed across writes through the pointer.
    size_t *data() { return n_ <= kInline ? inline_.data() : heap_.data(); }
    const size_t *data() const {
      return n_ <= kInline ? inline_.data() : heap_.data();
    }

    size_t *begin() { return data(); }
    size_t *end() { return data() + n_; }
    size_t back() const { return data()[n_ - 1]; }

  private:
    // 16 cells hold a query of 15 codepoints, which covers 97% of
    // /usr/share/dict/words; anything longer falls back to the heap, as it
    // did before this buffer existed. Value-initialized because the defaulted
    // copy constructor reads the whole array.
    static constexpr size_t kInline = 16;
    size_t n_ = 0;
    std::array<size_t, kInline> inline_{};
    std::vector<size_t> heap_; // empty unless n_ > kInline
  };

  std::shared_ptr<const std::u32string> s_;
  size_t max_edits_;
  size_t insert_cost_;
  size_t delete_cost_;
  size_t replace_cost_; // TODO: better cost function is needed?
  Row state_;
  size_t min_ = 0;   // smallest cell of state_; the initial row starts at 0
  size_t depth_ = 0; // codepoints consumed so far; unused unless banded_
  char u8buf_[4]{};  // bytes of a not-yet-complete codepoint
  uint8_t u8len_ = 0;
  bool banded_;

  bool decode_codepoint(std::string_view s8, char32_t &cp) const {
    auto l = s8.size();
    if (l) {
      uint8_t b = s8[0];
      if ((b & 0x80) == 0) {
        cp = b;
        return true;
      } else if ((b & 0xE0) == 0xC0) {
        if (l >= 2) {
          cp = ((static_cast<char32_t>(s8[0] & 0x1F)) << 6) |
               (static_cast<char32_t>(s8[1] & 0x3F));
          return true;
        }
      } else if ((b & 0xF0) == 0xE0) {
        if (l >= 3) {
          cp = ((static_cast<char32_t>(s8[0] & 0x0F)) << 12) |
               ((static_cast<char32_t>(s8[1] & 0x3F)) << 6) |
               (static_cast<char32_t>(s8[2] & 0x3F));
          return true;
        }
      } else if ((b & 0xF8) == 0xF0) {
        if (l >= 4) {
          cp = ((static_cast<char32_t>(s8[0] & 0x07)) << 18) |
               ((static_cast<char32_t>(s8[1] & 0x3F)) << 12) |
               ((static_cast<char32_t>(s8[2] & 0x3F)) << 6) |
               (static_cast<char32_t>(s8[3] & 0x3F));
          return true;
        }
      }
    }
    return false;
  }

  std::u32string decode(std::string_view s8) const {
    std::u32string out;
    size_t i = 0;
    while (i < s8.size()) {
      auto beg = i++;
      while (i < s8.size() && (s8[i] & 0xc0) == 0x80) {
        i++;
      }
      // A group that does not decode (an invalid lead byte, or a sequence
      // the input truncated) is dropped rather than appended: cp is left
      // untouched on failure, so appending it would read an indeterminate
      // value and put garbage in the query.
      char32_t cp;
      if (decode_codepoint(s8.substr(beg, i - beg), cp)) { out += cp; }
    }
    return out;
  }
};

//-----------------------------------------------------------------------------
// DummyAutomaton
//-----------------------------------------------------------------------------

struct DummyAutomaton {
  void step(char c) {}
  bool is_match() const { return true; }
  bool can_match() const { return true; }
};

//-----------------------------------------------------------------------------
// map
//-----------------------------------------------------------------------------

template <typename output_t> class map : public matcher<output_t> {
public:
  map(const char *byte_code, size_t byte_code_size)
      : matcher<output_t>(byte_code, byte_code_size) {}

  template <typename T>
  map(const T &byte_code)
      : matcher<output_t>(byte_code.data(), byte_code.size()) {}

  static const bool has_output = true;

  output_t operator[](std::string_view sv) const { return at(sv); }

  output_t operator[](const char *s) const { return at(s); }

  output_t at(std::string_view sv) const {
    auto output = output_t{};
    auto ret = matcher<output_t>::match(sv.data(), sv.size(),
                                        [&](const auto &_) { output = _; });
    if (!ret) { throw std::out_of_range("invalid key..."); }
    return output;
  }

  bool exact_match_search(std::string_view sv, output_t &output) const {
    return matcher<output_t>::match(sv.data(), sv.size(),
                                    [&](const auto &_) { output = _; });
  }

  template <typename PrefixesFn>
  bool common_prefix_search(std::string_view sv, PrefixesFn prefixes) const {
    return matcher<output_t>::match(sv.data(), sv.size(), nullptr, prefixes);
  }

  std::vector<std::pair<size_t, output_t>>
  common_prefix_search(std::string_view sv) const {
    std::vector<std::pair<size_t, output_t>> ret;
    common_prefix_search(sv, [&](size_t length, const output_t &output) {
      ret.emplace_back(std::pair(length, output));
    });
    return ret;
  }

  size_t longest_common_prefix_search(std::string_view sv,
                                      output_t &output) const {
    size_t prefix_len = 0;
    common_prefix_search(sv, [&](size_t len, const auto &_output) {
      prefix_len = len;
      output = _output;
    });
    return prefix_len;
  }

  bool
  predictive_search(std::string_view sv,
                    std::function<void(const std::string &, const output_t &)>
                        callback) const {
    auto ret = false;
    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        DummyAutomaton(),
        [&](const auto &word, const auto &output) {
          ret = true;
          callback(word, output);
        },
        sv);
    return ret;
  }

  std::vector<std::pair<std::string, output_t>>
  predictive_search(std::string_view sv) const {
    std::vector<std::pair<std::string, output_t>> ret;
    predictive_search(sv, [&](const auto &word, const auto &output) {
      ret.emplace_back(word, output);
    });
    return ret;
  }

  std::vector<std::pair<std::string, output_t>>
  edit_distance_search(std::string_view sv, size_t max_edits,
                       size_t insert_cost = 1, size_t delete_cost = 1,
                       size_t replace_cost = 1) const {

    std::vector<std::pair<std::string, output_t>> ret;

    if (sv.empty()) { return ret; }

    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        LevenshteinAutomaton(sv, max_edits, insert_cost, delete_cost,
                             replace_cost),
        [&](const auto &word, const auto &output) {
          ret.emplace_back(std::pair(word, output));
        });

    return ret;
  }

  std::vector<std::tuple<double, std::string, output_t>>
  suggest(std::string_view word) const {
    return matcher<output_t>::suggest_core(word, *this);
  }

  // Traverses the FST with a caller-supplied automaton that implements
  // step(char), is_match() and can_match() (see LevenshteinAutomaton for an
  // example), calling `callback` for every accepted word.
  template <typename T>
  void custom_search(const T &atm,
                     std::function<void(const std::string &, const output_t &)>
                         callback) const {
    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        atm, callback);
  }

  template <typename T> void enumerate(T callback) const {
    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        DummyAutomaton(), callback);
  }
};

//-----------------------------------------------------------------------------
// set
//-----------------------------------------------------------------------------

class set : public matcher<none_t> {
public:
  set(const char *byte_code, size_t byte_code_size)
      : matcher<none_t>(byte_code, byte_code_size) {}

  template <typename T>
  set(const T &byte_code)
      : matcher<none_t>(byte_code.data(), byte_code.size()) {}

  static const bool has_output = false;

  template <typename PrefixesFn>
  bool common_prefix_search(std::string_view sv, PrefixesFn prefixes) const {
    return matcher<none_t>::match(
        sv.data(), sv.size(), nullptr,
        [&](size_t len, const none_t &) { prefixes(len); });
  }

  std::vector<size_t> common_prefix_search(std::string_view sv) const {
    std::vector<size_t> ret;
    common_prefix_search(sv, [&](size_t length) { ret.push_back(length); });
    return ret;
  }

  size_t longest_common_prefix_search(std::string_view sv) const {
    size_t prefix_len = 0;
    common_prefix_search(sv, [&](size_t len) { prefix_len = len; });
    return prefix_len;
  }

  bool
  predictive_search(std::string_view sv,
                    std::function<void(const std::string &)> callback) const {
    auto ret = false;
    matcher<none_t>::depth_first_visit(
        matcher<none_t>::header_.start_address, std::string(), none_t{},
        DummyAutomaton(),
        [&](const auto &word, const auto &) {
          ret = true;
          callback(word);
        },
        sv);
    return ret;
  }

  std::vector<std::string> predictive_search(std::string_view sv) const {
    std::vector<std::string> ret;
    predictive_search(sv, [&](const auto &word) { ret.push_back(word); });
    return ret;
  }

  std::vector<std::string> edit_distance_search(std::string_view sv,
                                                size_t max_edits,
                                                size_t insert_cost = 1,
                                                size_t delete_cost = 1,
                                                size_t replace_cost = 1) const {

    std::vector<std::string> ret;

    if (sv.empty()) { return ret; }

    matcher<none_t>::depth_first_visit(
        matcher<none_t>::header_.start_address, std::string(), none_t{},
        LevenshteinAutomaton(sv, max_edits, insert_cost, delete_cost,
                             replace_cost),
        [&](const auto &word, const auto &) { ret.emplace_back(word); });

    return ret;
  }

  std::vector<std::pair<double, std::string>>
  suggest(std::string_view word) const {
    return matcher<none_t>::suggest_core(word, *this);
  }

  // Traverses the FST with a caller-supplied automaton that implements
  // step(char), is_match() and can_match() (see LevenshteinAutomaton for an
  // example), calling `callback` for every accepted word.
  template <typename T>
  void custom_search(const T &atm,
                     std::function<void(const std::string &)> callback) const {
    matcher<none_t>::depth_first_visit(
        matcher<none_t>::header_.start_address, std::string(), none_t{}, atm,
        [&](const auto &word, const auto &) { callback(word); });
  }

  template <typename T> void enumerate(T callback) const {
    matcher<none_t>::depth_first_visit(matcher<none_t>::header_.start_address,
                                       std::string(), none_t{},
                                       DummyAutomaton(), callback);
  }
};

//-----------------------------------------------------------------------------
// decompile
//-----------------------------------------------------------------------------

inline void decompile(const char *byte_code, size_t byte_code_size,
                      std::ostream &out, bool need_output = true) {

  auto type = get_output_type(byte_code, byte_code_size);

  if (type == OutputType::uint32_t) {
    map<uint32_t> matcher(byte_code, byte_code_size);
    if (matcher) {
      matcher.enumerate([&](const auto &word, auto output) {
        if (need_output) {
          out << word << '\t' << output << std::endl;
        } else {
          out << word << std::endl;
        }
      });
    }
  } else if (type == OutputType::uint64_t) {
    map<uint64_t> matcher(byte_code, byte_code_size);
    if (matcher) {
      matcher.enumerate([&](const auto &word, auto output) {
        if (need_output) {
          out << word << '\t' << output << std::endl;
        } else {
          out << word << std::endl;
        }
      });
    }
  } else if (type == OutputType::string) {
    map<std::string> matcher(byte_code, byte_code_size);
    if (matcher) {
      matcher.enumerate([&](const auto &word, auto output) {
        if (need_output) {
          out << word << '\t' << output << std::endl;
        } else {
          out << word << std::endl;
        }
      });
    }
  } else if (type == OutputType::none_t) {
    set matcher(byte_code, byte_code_size);
    if (matcher) {
      matcher.enumerate(
          [&](const auto &word, auto output) { out << word << std::endl; });
    }
  }
}

template <typename T>
inline void decompile(const T &byte_code, std::ostream &out,
                      bool need_output = true) {
  decompile(byte_code.data(), byte_code.size(), out, need_output);
}

} // namespace fst
