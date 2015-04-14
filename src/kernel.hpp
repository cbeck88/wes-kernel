#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/scoped_ptr.hpp>

namespace wesnoth {

typedef boost::property_tree::ptree config;

struct map_location {
	int x;
	int y;
};

class kernel {

	//****
	// INITIALIZATION
	//****
public:
	typedef std::string::iterator Ctor_it;
	kernel(Ctor_it begin, Ctor_it end); // Pass Lua script to load
	~kernel();

private:
	kernel(const kernel&); // private copy ctor unimplemented for now

	//****
	// WRITE ACCESS
	//****
public:
	////
	// Fire an event in the game.
	////

	struct event_result {
		boost::optional<std::string> error; // If an error occured, contains a message.

		// Conservative appraisals of the outcome:
		bool game_state_changed; // There may be false positives but no false negatives.
		bool undoable;           // There may be false negatives but no false positives.

		event_result() : error(), game_state_changed(false), undoable(true) {}
	};

	event_result fire_event(const std::string& name);

	////
	// Execute a command in the game.
	////

	event_result do_command(const config& command);

	////
	// Execute lua code in the game.
	////

	event_result execute(const std::string& lua); // Todo: Return a variant?

	////
	// Execute an AI turn in the game for the current player.
	////

	event_result execute_ai_turn(); // Signals an error if the current player is not AI.

	////
	// End the turn of the current player. Fires many associated events.
	////

	event_result end_turn();

	//****
	// READ-ONLY ACCESS
	//****

	int turn_number() const;
	int current_side_playing() const;
	int nteams() const;

	bool can_end_turn() const;

	enum PHASE { INITIAL, PRELOAD, PRESTART, START, PLAY, END };

	PHASE get_phase() const;

	enum SIDE_RESULT { VICTORY, DEFEAT, NONE };

	SIDE_RESULT get_side_result(int side) const;

	enum CONTROLLER { HUMAN, AI, NETWORK, NETWORK_AI, EMPTY };

	CONTROLLER get_side_controller(int side) const;

	// Locations and vision

	bool is_on_map(map_location loc) const;
	bool is_adjacent(map_location loc1, map_location loc2) const;

	bool is_fogged(map_location loc, int viewing_team) const;
	bool is_shrouded(map_location loc, int viewing_team) const;

	////
	// Evaluate a gamestate "report" for use in themes.
	////

	config read_report(const std::string& name, int viewing_team) const;

	////
	// Evaluate lua code, with the game in a read-only state.
	////

	config evaluate(const std::string& lua, int viewing_team) const;

	////
	// Get the logs

	std::string log() const;

	void set_external_log(std::ostream*) const;

	////
	// PIMPL idiom
	////
	class impl;

private:
	boost::scoped_ptr<impl> impl_;

	////
	// Detail: Allow intrusive pointer to the handle.
	////
	mutable int ref_count_;

public:
	void add_ref() const;
	void del_ref() const;
};

inline void intrusive_ptr_add_ref(const kernel* obj) {
	obj->add_ref();
}

inline void intrusive_ptr_release(const kernel* obj) {
	obj->del_ref();
}

} // end namespace wesnoth
