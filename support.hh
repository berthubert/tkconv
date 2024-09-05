#pragma once
#include <string>
#include <chrono>

struct DTime
{
  void start()
  {
    d_start =   std::chrono::steady_clock::now();
  }
  uint32_t lapUsec()
  {
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()- d_start).count();
    start();
    return usec;
  }

  std::chrono::time_point<std::chrono::steady_clock> d_start;
};

std::string makePathForId(const std::string& id);
bool isPresentNonEmpty(const std::string& id);
bool isPDF(const std::string& fname);
bool isDocx(const std::string& fname);
