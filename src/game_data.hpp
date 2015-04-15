#pragma once

#include <boost/range/adaptor/map.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/foreach.hpp>

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

	uint32_t id_;
	map_location loc_;
	unit unit_;
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
		ordered_unique<tag<by_id>, member<unit_rec,const uint32_t,&unit_rec::id_> >,
		//hashed by location
		hashed_unique<tag<by_loc>, member<unit_rec,const map_location,&unit_rec::loc_> >
	>
> unit_map;

typedef unit_map::index<by_loc>::type unit_map_by_loc;
typedef unit_map_by_loc::iterator unit_map_loc_it;

typedef unit_map::index<by_id>::type unit_map_by_id;
typedef unit_map_by_id::iterator unit_map_id_it;


////
// associated data types keyed by terrain_id
////

typedef std::string terrain_id;

struct unit_movetype {
	std::map<terrain_id, int> move_cost;
	bool skirmisher;
	bool exerts_zoc;
};

// Map product is useful for turning the game map into a map of move costs at each hex for some unit
template<typename A, typename B, typename C>
std::map<A, C> map_product_def( std::map<A,B> m, std::map<B,C> n, C missing_value = C()) {
	std::map<A,C> result;

	BOOST_FOREACH( const auto & v, m) {
		auto it = n.find(v.second);
		if (it != n.end()) {
			C c = n.at(v.second);
			result.push_back(v.first, it.second);
		} else {
			result.push_back(v.first, missing_value);
		}
	}
	return result;
}

////
// Data corresponding to [movetype]
////

struct unit_characteristics {
	unit_movetype movetype_;

	std::map<terrain_id, int> defense;

	//std::map<damage_type, int> resistance;	
};

////
// associative data types keyed by location
////

template<typename T>
using loc_map = std::map<map_location, T>;

typedef loc_map<terrain_id> terrain_map;

typedef std::map<terrain_id, int> terrain_movecosts;
typedef loc_map<int> cost_map;

////
// graph data
////

typedef loc_map<std::set<map_location> > neighbor_map;
typedef std::set<map_location> neighbor_function(map_location);
typedef std::map<std::pair<map_location, map_location>, int> metric;

class topology {
public:
	virtual std::set<map_location> neighbors(map_location) {
		assert(false);
//		return std::set<map_location>();
	}
	virtual bool adjacent(map_location a, map_location b) {
		auto n = neighbors(b);
		if(n.find(a) != n.end()) {
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

	int shortest_path_distance(map_location start, map_location end, cost_map cm = cost_map());
	std::vector<map_location> shortest_path(map_location start, map_location end, cost_map cm);
	std::vector<std::vector<map_location> > all_paths(map_location start, cost_map cm = cost_map());

	int heuristic_distance(map_location a , map_location b) {
		auto it = heuristic_cache_.find(make_pair(a,b));
		if (it != heuristic_cache_.end()) {
			return it->second;
		}

		int answer = shortest_path_distance(a, b);
		heuristic_cache_.emplace(make_pair(a,b), answer);
		return answer;
	}

private:
	neighbor_map tunnels_;
	mutable metric heuristic_cache_;
};

class hex : public topology {
	std::set<map_location> neighbors(map_location a);
};

////
// Side data structures
////

// This is only to cache and speed up vision calculations. All the real info about a side is in the lua table and manipulated in lua.
class sides {
	std::map<int, bool> share_maps_;
	std::map<int, bool> share_vision_;

	typedef std::map< std::pair<int,int>, bool> ally_cache;
	ally_cache ally_cache_;

	bool are_allied(int, int);

	typedef loc_map<bool> fog_override;
	std::map<int, fog_override> fog_override_table_;

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
			if (ally_cache_[std::make_pair(s,t)] && share_vision_[t] && !override_adjusted_fog(l, t))
				return false;
		}
		return true;
	}

	std::map<int, loc_map<bool> > shroud_table_;

	bool true_shroud(map_location l, int s) {
		return shroud_table_[s][l];
	}

	bool ally_adjusted_shroud(map_location l, int s) {
		if (!true_shroud(l,s)) return false;
		BOOST_FOREACH(int t, share_maps_ | boost::adaptors::map_keys ) {
			if (ally_cache_[std::make_pair(s,t)] && share_maps_[t] && !true_shroud(l, t))
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
	topology topo_;
	sides sides_;
};

} // end namespace wesnoth
