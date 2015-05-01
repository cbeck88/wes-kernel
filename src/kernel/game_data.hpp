#pragma once

#include <boost/range/adaptor/map.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/optional.hpp>

#include "kernel.hpp"

namespace wesnoth {

using wesnoth::map_location;
using std::make_pair;

////
// Unit Map
////

struct unit_rec
{
	unit_rec(uint32_t id_arg, map_location loc_arg, unit unit_arg)
		: id_(id_arg)
		, loc_(loc_arg)
		, unit_(unit_arg)
	{
	}

	int id_;
	map_location loc_;
	unit unit_;

	// This data is essential for pathfinding -- we cache it from the lua state, and update periodically, using the dirty_ flag to mark when update is needed.
	mutable int side_;
	mutable bool hidden_;
	mutable bool emits_zoc_;
	mutable bool dirty_;
	void update() const;
};

struct by_id {};
struct by_loc {};

using boost::multi_index::hashed_unique;
using boost::multi_index::ordered_unique;
using boost::multi_index::indexed_by;
using boost::multi_index::tag;
using boost::multi_index::member;

typedef boost::multi_index_container<
	unit_rec,
	indexed_by<
		//ordered by ids
		ordered_unique<tag<by_id>, member<unit_rec,const int,&unit_rec::id_> >,
		//hashed by location
		ordered_unique<tag<by_loc>, member<unit_rec,const map_location,&unit_rec::loc_> >
	>
> unit_map;

////
// associated data types keyed by terrain_id
////

// Map product is useful for turning the game map into a map of move costs at each hex for some unit
template<typename A, typename B, typename C>
std::map<A, C> map_product_def( std::map<A,B> m, std::map<B,C> n, C missing_value = C()) {
	std::map<A,C> result;

	BOOST_FOREACH( const auto & v, m) {
		auto it = n.find(v.second);
		if (it != n.end()) {
			result.push_back(v.first, it.second);
		} else {
			result.push_back(v.first, missing_value);
		}
	}
	return result;
}

// The no-default version is less likely to be useful but who knows.
template<typename A, typename B, typename C>
std::map<A, C> map_product( std::map<A,B> m, std::map<B,C> n) {
	std::map<A,C> result;

	BOOST_FOREACH( const auto & v, m) {
		auto it = n.find(v.second);
		if (it != n.end()) {
			result.push_back(v.first, it.second);
		}
	}
	return result;
}

////
// Data corresponding to [movetype]
////

/*
struct unit_movetype {
	std::map<terrain_id, size_t> move_cost;
	bool skirmisher;
	bool exerts_zoc;
};


struct unit_characteristics {
	unit_movetype movetype_;

	std::map<terrain_id, int> defense;

	//std::map<damage_type, int> resistance;	
};
*/


////
// associative data types keyed by location
////

template<typename T>
using loc_map = std::map<map_location, T>;

typedef loc_map<terrain_id> terrain_map;

typedef std::map<terrain_id, size_t> terrain_movecosts;

typedef boost::function<size_t(terrain_id)> terrain_cost_fcn;
typedef boost::function<size_t(map_location)> move_cost_fcn;

////
// graph data
////

typedef std::vector<map_location> path;
struct pathing_node {
	size_t moves_left;
	size_t turns_left;
	map_location pred;

	pathing_node(size_t ml, size_t tl, map_location p)
		: moves_left(ml)
		, turns_left(tl)
		, pred(p)
	{}
};
typedef std::map<map_location, pathing_node> shortest_path_tree;

typedef loc_map<loc_set > neighbor_map;
typedef loc_set neighbor_function(map_location);
typedef std::map<std::pair<map_location, map_location>, size_t> metric;

class sides;

class pathfind_context {
public:
	pathfind_context(const geometry & t)
		: geom_(t)
		, tunnels_()
		, heuristic_cache_()
	{
	}

	loc_set neighbors(map_location a) {
		loc_set result = geom_.neighbors(a);
		auto it = tunnels_.find(a);
		if (it != tunnels_.end()) {
			result.insert(it->second.begin(), it->second.end());
		}
		return result;
	}

	bool adjacent(map_location a, map_location b) {
		if (geom_.adjacent(a, b)) {
			return true;
		}
		auto it = tunnels_.find(b);
		if (it == tunnels_.end()) {
			return false;
		}
		auto set = it->second;
		auto it2 = set.find(a);
		if (it2 == set.end()) {
			return false;
		}

		return true;
	}

	bool add_tunnel(map_location a, map_location b) {
		auto s = tunnels_[a]; //don't mind to make empty, this is not called often
		auto ret = s.emplace(b);
		if (!ret.second) {
			heuristic_cache_ = metric(); //toss the cache, since it is dirty now
		}
		return ret.second; //ret.second is a boolean flag explaining if the emplace operation succeeded in creating a new entry
	}
	bool remove_tunnel(map_location a, map_location b) {
		auto s = tunnels_[a]; //don't mind to make empty, this is not called often
		auto ret = s.emplace(b);
		if (!ret.second) {
			heuristic_cache_ = metric(); //toss the cache, since it is dirty now
		}
		return ret.second; //ret.second is a boolean flag explaining if the emplace operation succeeded in creating a new entry
	}

	size_t shortest_path_distance(map_location start, map_location end, boost::optional<move_cost_fcn> = boost::none);
	path shortest_path(map_location start, map_location end, boost::optional<move_cost_fcn> = boost::none);

	size_t heuristic_distance(map_location a , map_location b) {
		auto it = heuristic_cache_.find(make_pair(a,b));
		if (it != heuristic_cache_.end()) {
			return it->second;
		}

		int answer = shortest_path_distance(a, b);
		heuristic_cache_.emplace(make_pair(a,b), answer);
		return answer;
	}

	struct pathing_query {
		map_location start;
		boost::optional<move_cost_fcn> cost_map;
		size_t moves;
		size_t turns;
		size_t max_moves;

		boost::optional<move_cost_fcn> first_turn_override_cost_map; //for handling slowed units

		// How to handle other units
		// Ignore them? Set no moving_team then.
		// Only handle the units visible to a certain team? Set the viewing_team
		boost::optional<int> moving_side;
		boost::optional<int> viewing_side;
		bool ignore_zoc;

		// resources...
		terrain_map * tmap_;
		unit_map * units;
		sides * sides_;
	};

	size_t shortest_path_distance(map_location end, const pathing_query &);
	path shortest_path(map_location end, const pathing_query &);

	loc_set reachable_hexes(const pathing_query &);
	std::vector<path> reachable_hexes_with_paths(const pathing_query &);
	shortest_path_tree compute_tree(const pathing_query &, boost::optional<map_location> dest = boost::none);

private:
	geometry geom_;
	neighbor_map tunnels_;
	mutable metric heuristic_cache_;
};


////
// Side data structures
////

// This is only to cache and speed up vision calculations. All the real info about a side is in the lua table and manipulated in lua.
class sides {
public:
	typedef boost::function<bool(int, int)> ally_calc_function;

	sides( const ally_calc_function & a ) 
		: ally_calculator_(a)
	{
	}

	void update_ally_calculator(const ally_calc_function & f) {
		ally_calculator_ = f;
	}

private:

	ally_calc_function ally_calculator_;

	std::map<int, bool> share_maps_;
	std::map<int, bool> share_vision_;

	typedef std::map< std::pair<int,int>, bool> ally_cache;
	ally_cache ally_cache_;

public:
	bool are_allied(int a, int b);

private:
	typedef loc_map<bool> fog_override;
	std::map<int, fog_override> fog_override_table_;

	std::map<int, loc_map<bool> > shroud_table_;

public:
	bool true_fog(map_location, int);
	boost::optional<bool> get_fog_override(map_location l , int t) {
		auto tab = fog_override_table_[t];
		auto it = tab.find(l);
		if (it == tab.end() ) {
			return boost::none;
		}
		return  it->second;
	}
	bool override_adjusted_fog(map_location l, int s) {
		if (auto b = get_fog_override(l, s)) {
			return *b;
		}
		return true_fog(l, s);
	}

	bool ally_adjusted_fog(map_location l, int s) {
		if (!override_adjusted_fog(l,s)) return false;
		BOOST_FOREACH(int t, share_vision_ | boost::adaptors::map_keys ) {
			if (are_allied(s,t) && share_vision_[t] && !override_adjusted_fog(l, t))
				return false;
		}
		return true;
	}


	bool true_shroud(map_location l, int s) {
		return shroud_table_[s][l];
	}

	bool ally_adjusted_shroud(map_location l, int s) {
		if (!true_shroud(l,s)) return false;
		BOOST_FOREACH(int t, share_maps_ | boost::adaptors::map_keys ) {
			if (are_allied(s,t) && share_maps_[t] && !true_shroud(l, t))
				return false;
		}
		return true;
	}
};

////
// Game data structure
////

struct game_data {
	terrain_map terrain_map_;
	unit_map units_;
	pathfind_context map_with_tunnels_;
	sides sides_;

	game_data(const geometry & g, const boost::function<bool(int, int)> & ally_calculator)
		: terrain_map_()
		, units_()
		, map_with_tunnels_(g)
		, sides_(ally_calculator)
	{}
};

} // end namespace wesnoth
