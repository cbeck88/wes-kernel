#pragma once

#include <set>
#include <string>
#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/variant.hpp>

// Defines types that the external graphics engine provides us to stand for the various wesnoth objects.

typedef boost::property_tree::ptree config;

namespace wesnoth {

	typedef boost::variant<bool, int, std::string, std::vector<std::string> > value;

	// TODO: allow more general, it should be possible (but maybe slow)
	struct map_location {
		int x;
		int y;

		bool operator<(const map_location& a) const { return x < a.x || (x == a.x && y < a.y); }
	};

	typedef std::set<map_location> loc_set;

	class topology {
	public:
		virtual loc_set neighbors(map_location) {
			assert(false && "Your topology must override the default neighbor function");
		}

		// You should override this also for efficiency, but it might be hard to do better for some exotic topologies.
		virtual bool adjacent(map_location a, map_location b) {
			auto n = neighbors(b);
			if(n.find(a) != n.end()) {
				return true;
			}
			return false;
		}
	};

	// The wesnoth topology.
	class hex : public topology {
		std::set<map_location> neighbors(map_location a);
		bool adjacent(map_location a, map_location b);
	};


	// Structure which represents a map. These update functions should update the graphical display.

	typedef std::string terrain_id;

	struct gamemap {
		virtual ~gamemap() {}

		virtual terrain_id get_terrain(map_location) const { assert(false); }
		virtual bool set_terrain(map_location, terrain_id) { assert(false); }

		virtual void lock_drawing() { assert(false); }
		virtual void unlock_drawing() { assert(false); }
	};

/*	enum DEFEAT_CONDITION {
		NO_LEADER_LEFT,
		NO_UNITS_LEFT,
		NEVER,
		ALWAYS
	};*/

	struct side {
		virtual ~side() {}
		virtual bool set_attribute(std::string, value) { assert(false); }
		virtual value get_attribute(std::string) const { assert(false); }

/*		std::string set_name() const;
		SIDE_RESULT set_side_result() const;*/
	};

	struct unit {
		virtual ~unit() {}
/*		boost::optional<map_location> get_location() const;
		bool set_location(boost::optional<map_location>);

		int get_side();*/

		virtual bool set_attribute(std::string, value) { assert(false); }
		virtual value get_attribute(std::string) const { assert(false); }
	};

	// Bundle of functions to allocate externally managed objects
	// The assumption is that "external" has their own class unit which
	// tracks animations and clicks and such.
	// They want to be notified when properties of the units change, so that they can perform animations etc.
	//
	// To interface with wesnoth kernel, you should
	// -- Write a wrapper class around boost::intrusive_ptr<my_unit> where my_unit is your class
	//	(or your smart pointer of choice)
	// -- (Similarly for map and side)
	// -- Derive class factory:
	// 	-- unit_size() should return size_of(boost::intrusive_ptr<unit>)
	//	-- construct_unit should call "placement new" at the given pointer location constructing a unit of given type using given config data
	//	-- (Similarly for map and side)
	
	class interface {
		virtual size_t map_size() = 0;
		virtual void construct_map(void *, const config &) = 0;

		virtual size_t side_size() = 0;
		virtual void construct_side(void *, const config &) = 0;

		virtual size_t unit_size() =0 ;
		virtual void construct_unit(void *, const config &) = 0;

		// Called when wesnoth engine requests a dialog to launch
		virtual bool show_dialog(const std::string &, const config &) = 0;
	};

} // end namespace kernel
