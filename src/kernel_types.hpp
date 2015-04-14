#pragma once

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
	};

	struct gamemap {
		virtual ~gamemap() = 0;

		virtual std::string get_terrain(map_location) const = 0;
		virtual bool set_terrain(map_location, std::string) = 0;

		virtual void lock_drawing() = 0;
		virtual void unlock_drawing() = 0;
	};

/*	enum DEFEAT_CONDITION {
		NO_LEADER_LEFT,
		NO_UNITS_LEFT,
		NEVER,
		ALWAYS
	};*/

	struct side {
		virtual ~side() = 0;
		virtual bool set_attribute(std::string, value) = 0;
		virtual value get_attribute(std::string) const = 0;

/*		std::string set_name() const;
		SIDE_RESULT set_side_result() const;*/
	};

	struct unit {
		virtual ~unit() = 0;
/*		boost::optional<map_location> get_location() const;
		bool set_location(boost::optional<map_location>);

		int get_side();*/

		virtual bool set_attribute(std::string, value) = 0;
		virtual value get_attribute(std::string) const = 0;
		
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
