#pragma once

#include <string>

namespace wml {
bool strip_preprocessor(std::string& str);

bool parse(const std::string& str);
bool parse_attr(const std::string& str);

void test();
} // end namespace wml
