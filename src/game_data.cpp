#include <game_data.hpp>

namespace wesnoth {

bool sides::are_allied(int a, int b) {
	auto it = ally_cache_.find(std::make_pair(a,b));
	if (it != ally_cache_.end()) {
		return it->second;
	}
	return ally_cache_[std::make_pair(a,b)] = ally_calculator_(a, b);
}

} // end namespace wesnoth
