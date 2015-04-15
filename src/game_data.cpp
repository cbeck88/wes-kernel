#include <game_data.hpp>

namespace wesnoth {

static map_location _helper(int x, int y) {
	map_location ret;
	ret.x = x;
	ret.y = y;
	return ret;
}

// TODO: Fix this to correct for C++ vs WML 0 - 1 difference
std::set<map_location> hex::neighbors(map_location a) {
	std::set<map_location> res;
	bool b = (a.x & 1) == 0;
	res.emplace(_helper(a.x, a.y-1));
	res.emplace(_helper(a.x+1, a.y - (b ? 1 : 0)));
	res.emplace(_helper(a.x+1, a.y + (b ? 0 : 1)));
	res.emplace(_helper(a.x, a.y+1));
	res.emplace(_helper(a.x-1, a.y + (b ? 0 : 1)));
	res.emplace(_helper(a.x-1, a.y + (b ? 1 : 0)));
	return res;
}

} // end namespace wesnoth
