#pragma once
#include <string>

std::string makePathForId(const std::string& id);
bool isPresentNonEmpty(const std::string& id);
bool isPDF(const std::string& fname);
bool isDocx(const std::string& fname);
