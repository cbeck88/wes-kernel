#include "game_data.hpp"
#include "kernel.hpp"

namespace wesnoth {

bool sides::are_allied(int a, int b) {
	auto it = ally_cache_.find(std::make_pair(a,b));
	if (it != ally_cache_.end()) {
		return it->second;
	}
	return ally_cache_[std::make_pair(a,b)] = ally_calculator_(a, b);
}

typedef std::pair<map_location, pathing_node> heap_entry;

struct comp {
	bool operator()(const heap_entry & a, const heap_entry & b) {
		return a.second.turns_left > b.second.turns_left ||
			(a.second.turns_left == b.second.turns_left && a.second.moves_left > b.second.moves_left);
	}
};

// Tries to find a match for a visible enemy at some position with respect to a pathing query
static const unit_rec * get_visible_enemy(map_location neighbor, const pathfind_context::pathing_query & query, bool must_exert_zoc) {
	unit_map::index<by_loc>::type & u_map = query.units->get<by_loc>();
	sides & sides = *query.sides;
	auto u_it = u_map.find(neighbor);

	if (u_it == u_map.end()) { 
		return NULL;
	}

	const unit_rec & u = *u_it;
	if (u.dirty_) {
		u.update();
	}

	if (u.loc_ == neighbor && // if it did not move during update
		(!must_exert_zoc || u.emits_zoc_) && // check the must_exert_zoc flag
		!sides.are_allied(u.side_, *query.moving_side)) // only enemy units block this unit
	{
		if (!query.viewing_side) { // the blocker is definitely visible since we see all
			return & u;
		}
		// we don't see all so check if the blocker is actually visible
		if ((!u.hidden_ || sides.are_allied(u.side_, *query.viewing_side))
				&& !sides.ally_adjusted_fog(neighbor, *query.viewing_side)) {
			return & u;
		}
	}
	return NULL;
}

shortest_path_tree pathfind_context::compute_tree(const pathfind_context::pathing_query & query) {
	shortest_path_tree result;

	std::deque<heap_entry> priority_queue(1, heap_entry(query.start, pathing_node(query.moves, query.turns, query.start)));

	comp heap_comparator;

	assert(query.sides);
	sides & sides = *query.sides;

	while (!priority_queue.empty()) {
		heap_entry he = priority_queue.front();
		const map_location & loc = he.first;

		result.insert(he);

		std::pop_heap(priority_queue.begin(), priority_queue.end(), heap_comparator); priority_queue.pop_back();

		BOOST_FOREACH(map_location neighbor, topo_.neighbors(loc)) {
			if (result.find(neighbor) != result.end()) {
				continue; //skip nodes that we already processed
			}

			assert(query.tmap_);
			if (query.tmap_->find(neighbor) == query.tmap_->end()) {
				continue; // skip nodes that are off map
			}

			if(query.viewing_side) {
				if (sides.ally_adjusted_shroud(neighbor, *query.viewing_side)) {
					continue; // skip shrouded nodes
				}
			}

			size_t cost_of_move;

			bool used_first_turn_override = false;

			if (query.first_turn_override_cost_map && (he.second.turns_left == query.turns)) {// check our node to see if it is still the first turn 
				cost_of_move = (*query.first_turn_override_cost_map)(neighbor);
				used_first_turn_override = true;
			} else {
				cost_of_move = (*query.cost_map)(neighbor);
			}

			size_t turns_left = he.second.turns_left;
			size_t moves_left = he.second.moves_left;

			if (cost_of_move > moves_left && turns_left > 0) {
				turns_left--;
				moves_left = query.max_moves;
				if (used_first_turn_override) { //recalculate cost since we had to end the turn
					cost_of_move = (*query.cost_map)(neighbor);
				}
			}

			if (cost_of_move > moves_left) {
				continue; // we can't actually afford to make the move at all, even after possibly refreshing the moves for a turn
			}
			moves_left = moves_left - cost_of_move;

			// If we should think of the move as being done by a unit as some side, and check for blockers / zocs
			if (query.moving_side) {
				if (get_visible_enemy(neighbor, query, false)) {
					continue;
				}
				// If our unit can get zocced, check if it will be by this move
				if (!query.ignore_zoc && moves_left > 0) {
					BOOST_FOREACH(map_location neighbor2, topo_.neighbors(neighbor)) {
						if (get_visible_enemy(neighbor2, query, true)) {
							moves_left = 0;
							break;
						}
					}
				}
			}

			pathing_node node(moves_left, turns_left, loc);
			priority_queue.push_back(std::make_pair(neighbor, node));
			std::push_heap(priority_queue.begin(), priority_queue.end(), heap_comparator);
		}
	}
	return result;
}

} // end namespace wesnoth
