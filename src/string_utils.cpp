#include "string_utils.hpp"

#include <string>
#include <vector>

#include <boost/array.hpp>
#include <boost/lexical_cast.hpp>

namespace string_utils {

const std::string ellipsis = "...";

const std::string unicode_minus = "−";
const std::string unicode_en_dash = "–";
const std::string unicode_em_dash = "—";
const std::string unicode_figure_dash = "‒";
const std::string unicode_multiplication_sign = "×";
const std::string unicode_bullet = "•";

bool isnewline(const char c)
{
	return c == '\r' || c == '\n';
}

// Make sure that we can use Mac, DOS, or Unix style text files on any system
// and they will work, by making sure the definition of whitespace is consistent
bool portable_isspace(const char c)
{
	// returns true only on ASCII spaces
	if (static_cast<unsigned char>(c) >= 128)
		return false;
	return isnewline(c) || isspace(c);
}

// Make sure we regard '\r' and '\n' as a space, since Mac, Unix, and DOS
// all consider these differently.
bool notspace(const char c)
{
	return !portable_isspace(c);
}

std::string &strip(std::string &str)
{
	// If all the string contains is whitespace,
	// then the whitespace may have meaning, so don't strip it
	std::string::iterator it = std::find_if(str.begin(), str.end(), notspace);
	if (it == str.end())
		return str;

	str.erase(str.begin(), it);
	str.erase(std::find_if(str.rbegin(), str.rend(), notspace).base(), str.end());

	return str;
}

std::string &strip_end(std::string &str)
{
	str.erase(std::find_if(str.rbegin(), str.rend(), notspace).base(), str.end());

	return str;
}

/**
 * Splits a (comma-)separated string into a vector of pieces.
 * @param[in]  val    A (comma-)separated string.
 * @param[in]  c      The separator character (usually a comma).
 * @param[in]  flags  Flags controlling how the split is done.
 *                    This is a bit field with two settings (both on by default):
 *                    REMOVE_EMPTY causes empty pieces to be skipped/removed.
 *                    STRIP_SPACES causes the leading and trailing spaces of each piece to be ignored/stripped.
 */
std::vector< std::string > split(std::string const &val, const char c, const int flags)
{
	std::vector< std::string > res;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2;
	if (flags & STRIP_SPACES) {
		while (i1 != val.end() && portable_isspace(*i1))
			++i1;
	}
	i2=i1;

	while (i2 != val.end()) {
		if (*i2 == c) {
			std::string new_val(i1, i2);
			if (flags & STRIP_SPACES)
				strip_end(new_val);
			if (!(flags & REMOVE_EMPTY) || !new_val.empty())
				res.push_back(new_val);
			++i2;
			if (flags & STRIP_SPACES) {
				while (i2 != val.end() && portable_isspace(*i2))
					++i2;
			}

			i1 = i2;
		} else {
			++i2;
		}
	}

	std::string new_val(i1, i2);
	if (flags & STRIP_SPACES)
		strip_end(new_val);
	if (!(flags & REMOVE_EMPTY) || !new_val.empty())
		res.push_back(new_val);

	return res;
}

} // end namespace string_utils
