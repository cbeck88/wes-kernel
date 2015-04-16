#include "game_data.hpp"
#include "kernel.hpp"
#include "kernel_types.hpp"
#include "string_utils.hpp"

#include "eris/lauxlib.h"
#include "eris/lua.h"
#include "eris/lualib.h"

// For null ostream...
#include "boost/iostreams/stream.hpp"
#include "boost/iostreams/device/null.hpp"

#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace wesnoth {

////
// Declare and define the implementation class
// This is the body of the handle-body idiom.
////

class kernel::impl {
public:
	impl(kernel::Ctor_it begin, kernel::Ctor_it end);

	void set_external_log(std::ostream* ext) const { log_.external_log_ = ext; }

	std::string log() const { return log_.log_.str(); }

	friend class kernel;

private:
	lua_State* lua_;

	game_data game_data_;

	std::string my_name() { return "wesnoth-kernel v 0.0.0, (Eris Lua 5.2.3)"; }

	struct command_log {
		std::stringstream log_;
		mutable std::ostream* external_log_;

		command_log() : log_(), external_log_(NULL) {}

		inline command_log& operator<<(const std::string& str) {
			log_ << str;
			if (external_log_) {
				(*external_log_) << str;
			}
			return *this;
		}

		inline command_log& operator<<(char const* str) {
			if (str != NULL) {
				log_ << str;
				if (external_log_) {
					(*external_log_) << str;
				}
			}
			return *this;
		}
	};

	mutable command_log log_;

	void load_C_object_metatables();

	bool load_string(const std::string&);
	bool protected_call(int nargs, int nrets);

	int intf_print(lua_State* L);

	bool are_allied(int side1, int side2);
};

namespace {

	// Getter which takes a lua_State and recovers the pointer to its owner from the lua EXTRA_SPACE region.
	// (The owner object stores its 'this' pointer there at construction time).
	kernel::impl*& get_kernel_impl(lua_State* L) { return *reinterpret_cast<kernel::impl**>(reinterpret_cast<char*>(L) - WESNOTH_KERNEL_OFFSET); }

	// Template which allows to push member functions of the kernel into lua as C functions, using a shim
	typedef kernel::impl kimpl;
	typedef int (kimpl::*member_callback)(lua_State* L);

	template <member_callback method>
	int dispatch(lua_State* L) {
		return ((*get_kernel_impl(L)).*method)(L);
	}


} // end namespace detail

////
// Implement the functions of kernel_impl
////

//#define DBG_LUA std::cout

int kernel::impl::intf_print(lua_State* L) {
	//	boost::iostreams::stream< boost::iostreams::null_sink > DBG_LUA( ( boost::iostreams::null_sink() ) );

	size_t nargs = lua_gettop(L);

	for (size_t i = 1; i <= nargs; ++i) {
		const char* str = lua_tostring(L, i);
		if (!str) {
			str = "";
		}
		log_ << str;
		//		DBG_LUA << "'" << str << "'\n";
	}

	log_ << "\n";
	//	DBG_LUA << "\n";

	return 0;
}

kernel::impl::impl(kernel::Ctor_it begin, kernel::Ctor_it end)
	: lua_(luaL_newstate())
	, game_data_(hex(), boost::bind(&impl::are_allied, this, _1, _2))
	, log_()
{
	get_kernel_impl(lua_) = this;

	lua_State * L = lua_;

	log_ << "Adding standard libs...\n";

	static const luaL_Reg safe_libs[] = {
		{ "",       luaopen_base   },
		{ "table",  luaopen_table  },
		{ "string", luaopen_string },
		{ "math",   luaopen_math   },
		{ "coroutine",   luaopen_coroutine   },
		{ "debug",  luaopen_debug  },
		{ "os",     luaopen_os     },
		{ "bit32",  luaopen_bit32  }, // added in Lua 5.2
		{ NULL, NULL }
	};
	for (luaL_Reg const *lib = safe_libs; lib->func; ++lib)
	{
		luaL_requiref(L, lib->name, lib->func, 1);
		lua_pop(L, 1);  /* remove lib */
	}

	// Disable functions from os which we don't want.
	lua_getglobal(L, "os");
	lua_pushnil(L);
	while(lua_next(L, -2) != 0) {
		lua_pop(L, 1);
		char const* function = lua_tostring(L, -1);
		if(strcmp(function, "clock") == 0 || strcmp(function, "date") == 0
			|| strcmp(function, "time") == 0 || strcmp(function, "difftime") == 0) continue;
		lua_pushnil(L);
		lua_setfield(L, -3, function);
	}
	lua_pop(L, 1);

	// Disable functions from debug which we don't want.
	lua_getglobal(L, "debug");
	lua_pushnil(L);
	while(lua_next(L, -2) != 0) {
		lua_pop(L, 1);
		char const* function = lua_tostring(L, -1);
		if(strcmp(function, "traceback") == 0 || strcmp(function, "getinfo") == 0) continue;	//traceback is needed for our error handler
		lua_pushnil(L);										//getinfo is needed for ilua strict mode
		lua_setfield(L, -3, function);
	}
	lua_pop(L, 1);

	// Delete dofile and loadfile.
	lua_pushnil(L);
	lua_setglobal(L, "dofile");
	lua_pushnil(L);
	lua_setglobal(L, "loadfile");

/*
	// Store the error handler.
	log_ << "Adding error handler...\n";

	lua_pushlightuserdata(L
			, executeKey);
	lua_getglobal(L, "debug");
	lua_getfield(L, -1, "traceback");
	lua_remove(L, -2);
	lua_rawset(L, LUA_REGISTRYINDEX);
	lua_pop(L, 1);
*/

	// Redirect print
	log_ << "Redirecting print...\n";
	lua_pushcfunction(lua_, &dispatch<&kernel::impl::intf_print>);
	lua_setglobal(lua_, "print");

	log_ << "Initializing " << my_name() << "...\n";

	load_C_object_metatables();

	std::string script(begin, end); // this can be done differently later

	if (load_string(script.c_str())) {
		protected_call(0, 0);
	}
}

void kernel::impl::load_C_object_metatables() {
}

bool kernel::impl::load_string(const std::string& str) {
	lua_State* L = lua_;

	int errcode = luaL_loadstring(L, str.c_str());
	if (errcode != LUA_OK) {
		char const* msg = lua_tostring(L, -1);
		std::string message = msg ? msg : "null string";

		std::string context = "When parsing a string to lua, ";

		if (errcode == LUA_ERRSYNTAX) {
			context += " a syntax error";
		} else if (errcode == LUA_ERRMEM) {
			context += " a memory error";
		} else if (errcode == LUA_ERRGCMM) {
			context += " an error in garbage collection metamethod";
		} else {
			context += " an unknown error";
		}

		lua_pop(L, 1);

		// eh(message.c_str(), context.c_str())
		std::cerr << " --- ERROR ---\n";
		std::cerr << context << ":\n" << message << std::endl;
		std::cerr << " -------------\n";
		return false;
	}
	return true;
}

bool kernel::impl::protected_call(int nargs, int nrets) {
	lua_State* L = lua_;

	int errcode = lua_pcall(L, 0, 0, 0);

	if (errcode != LUA_OK) {
		char const* msg = lua_tostring(L, -1);
		std::string message = msg ? msg : "null string";

		std::string context = "When executing, ";
		if (errcode == LUA_ERRRUN) {
			context += "Lua runtime error";
		} else if (errcode == LUA_ERRERR) {
			context += "Lua error in attached debugger";
		} else if (errcode == LUA_ERRMEM) {
			context += "Lua out of memory error";
		} else if (errcode == LUA_ERRGCMM) {
			context += "Lua error in garbage collection metamethod";
		} else {
			context += "unknown lua error";
		}

		lua_pop(L, 1);

		// eh(message.c_str(), context.c_str())
		std::cerr << " --- ERROR ---\n";
		std::cerr << context << ":\n" << message << std::endl;
		std::cerr << " -------------\n";

		return false;
	}
	return true;
}

bool kernel::impl::are_allied(int side1, int side2) {
	std::string teams1;
	std::string teams2;

	{
		lua_getglobal(lua_, "Sides");
		lua_pushvalue(lua_, -1); //copy it for later

		lua_pushinteger(lua_, side1);
		lua_rawget(lua_, -2);
		lua_pushstring(lua_, "teams");
		lua_rawget(lua_, -2);
		if (const char * str = lua_tostring(lua_, -1)) {
			teams1 = str;
		}
		lua_pop(lua_, 1);

		lua_pushinteger(lua_, side2);
		lua_rawget(lua_, -2);
		lua_pushstring(lua_, "teams");
		lua_rawget(lua_, -2);
		if (const char * str = lua_tostring(lua_, -1)) {
			teams2 = str;
		}
		lua_pop(lua_, 1);
	}

	auto t1_vec = string_utils::split(teams1);
	auto t2_vec = string_utils::split(teams2);
	BOOST_FOREACH(const std::string & t1, t1_vec) {
		if (std::find(t2_vec.begin(), t2_vec.end(), t1) != t2_vec.end()) {
			return true;
		}
	}
	return false;
}

////
// Implement HANDLE methods (class kernel)
////

kernel::kernel(kernel::Ctor_it begin, kernel::Ctor_it end) : impl_(new kernel::impl(begin, end)), ref_count_(0) {
}

// Needed to be defined in the .cpp so that boost::scoped_ptr can be used for the impl
kernel::~kernel() {
}

kernel::event_result kernel::fire_event(const std::string&) {
	return {};
}
kernel::event_result kernel::do_command(const config&) {
	return {};
}
kernel::event_result kernel::execute_ai_turn() {
	return {};
}
kernel::event_result kernel::end_turn() {
	return {};
}

int kernel::turn_number() const {
	return 0;
}
int kernel::current_side_playing() const {
	return 0;
}
int kernel::nteams() const {
	return 0;
}
bool kernel::can_end_turn() const {
	return true;
}
kernel::PHASE kernel::get_phase() const {
	return kernel::INITIAL;
}
kernel::SIDE_RESULT kernel::get_side_result(int side) const {
	return NONE;
}
kernel::CONTROLLER kernel::get_side_controller(int side) const {
	return EMPTY;
}

bool kernel::is_on_map(map_location loc) const {
	return true;
}
bool kernel::is_adjacent(map_location loc1, map_location loc2) const {
	return true;
}

bool kernel::is_fogged(map_location loc, int viewing_team) const {
	return true;
}
bool kernel::is_shrouded(map_location loc, int viewing_team) const {
	return true;
}

config kernel::read_report(const std::string& name, int viewing_team) const {
	return evaluate("wesnoth.theme_items." + name, viewing_team);
}
config kernel::evaluate(const std::string& prog, int viewing_team) const {
	return config();
}

kernel::event_result kernel::execute(const std::string& prog) {
	kernel::event_result result;

	bool good = impl_->load_string(prog);
	if (!good) {
		result.error = "parse error";
		return result;
	}

	good = impl_->protected_call(0, 0);
	if (!good) {
		result.error = "runtime error";
		return result;
	}

	return result;
}

std::string kernel::log() const {
	return impl_->log();
}

void kernel::set_external_log(std::ostream* str) const {
	impl_->set_external_log(str);
}

void kernel::add_ref() const {
	++ref_count_;
}

void kernel::del_ref() const {
	if (--ref_count_ == 0) {
		delete this;
	}
}

} // end namespace wesnoth
