#pragma once

#include <string>
#include <vector>

namespace string_utils {

extern const std::string unicode_minus;
extern const std::string unicode_en_dash;
extern const std::string unicode_em_dash;
extern const std::string unicode_figure_dash;
extern const std::string unicode_multiplication_sign;
extern const std::string unicode_bullet;

bool isnewline(const char c);
bool portable_isspace(const char c);
bool notspace(char c);

/** Remove whitespace from the front and back of the string 'str'. */
std::string &strip(std::string &str);

/** Remove whitespace from the back of the string 'str'. */
std::string &strip_end(std::string &str);

enum { REMOVE_EMPTY = 0x01,	/**< REMOVE_EMPTY : remove empty elements. */
	  STRIP_SPACES  = 0x02	/**< STRIP_SPACES : strips leading and trailing blank spaces. */
};

/// Splits a (comma-)separated string into a vector of pieces.
std::vector< std::string > split(std::string const &val, const char c = ',', const int flags = REMOVE_EMPTY | STRIP_SPACES);

} // end namespace string_utils
