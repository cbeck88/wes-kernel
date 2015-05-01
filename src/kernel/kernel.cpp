/************

Design Criteria:

The class wesnoth::kernel should be a C++ object which represents a Wesnoth Gamestate in its full detail.
It should contain all the data which should be saved and reloaded in a game. It should be serializable
to a string, and recoverable from that string. Also it should be possible to load strings corresponding
to scenario files.
It should have methods along the lines "fire_event" "do_command" "end_turn" "play_ai_turn". There should
be accessors which can be used to access game state information, or generate games state "reports" for use
in theme items.
The game state object should be copyable, and optionally bound to / detached from a display. It should be
possible to ask questions about the game state "what if this happened, this move was made... ?" by direct
experiment and not by hard-coding the programmer's expectations. Most likely AI should also be
implemented this way.

Design Decisions:
My assumption is that we want to continue to support scenarios written at least partially in lua in some
form. If we do, a reasonable option for this class is to try to implement most of the gamestate logic in
lua, including event queues etc. -- essentially everything except pathfinding which would be inefficient
in lua. This also has the advantage that more of the maintanence and bug fix work is accessible to people
who don't have C++ skills.

There are a few basic problems to overcome in this plan:

1.) Lua is a C library, and therefore cannot hold pointers to C++ member functions.

Many people advocate using some kind of library like LuaBind or tolua++ to generate
lua bindings. In my opinion this is not a good idea. These libraries tend to use a
large amount of templates in a somewhat opaque way to make entire C++ classes
available to lua via the same layout as they have in C++. This might be good in
some applications but it seems very bad in wesnoth, where we expect users to
ultimately use the lua who have no knowledge of the C++, and we might want to
change the C++ implementation without exposing those changes to lua. Moreover it
would lead to more classes in lua than are necessary. In my opinion the lua syntax
for classes (using : instead of . ) is not very good or safe, and would be
confusing for users. It's particularly bad in my opinion that using : when you
needed . or vice versa is not a syntax error. Therefore, we will avoid the : syntax,
and the lua objects we provide will be "mixins", tables containing a variety of
functions which will be organized in an ad-hoc manner separately from the C++
class layout.

In wesnoth 1.x the solution was that all the functions pushed to lua are explicitly
C-style functions of the correct type declared at file-scope. Since they don't have
any parts of the engine in scope, they have to access the game engine data by using 
a series of global pointers in "resources.hpp". In my opinion this is very bad design
and it hinders the goal that the gamestate should be copyable and that many gamestate
objects should be able to exist independently of one another. Therefore this is ruled
out.

A first idea to work around it is to use light userdata with a metatable which
provides a call operator, and use that as a stand-in for a function. However, for
various reasons this is not very good. One is that we would need a different
metatable for each class whose methods we are pushing to lua, since the __call
operator implementation has to cast the pointer to the correct type. Another is
that when the game state is copied, we need a way to update the pointer so that
we don't get dangling pointers.

A second idea is to simply use lua "full userdata" to hold C++ objects of type
boost::function<int(lua_State*)>, give them an appropriate call operator, and
a __gc metamethod which destroys them appropriately. I wrote a class like this
for wesnoth months ago, and used it for some time for this purpose.

https://github.com/wesnoth/wesnoth/blob/master/src/scripting/lua_cpp_function.cpp

This seems like a quite good solution -- it's not too complex, and can be used
to make mixins out of arbitrary C++ methods from arbitrary classes since the 
boost::function class acts as a nice adapter to hide the details.

However, it doesn't solve the problem of stale pointers. If the C++ lua-container
object passes objects of the form "boost::bind(&my_class::my_method, this, _1 )"
to lua, the "this" pointers become stale when we copy and they must somehow be
corrected.

A possible solution is to have a common boost::shared_ptr<my_class> which is
initiailzed to "this" and shared among all the function objects, and updated
when we use the copy ctor. However this adds much complexity.
 
Another idea is to store pointers like these in the lua registry. That would
work, but an even better way than that makes use of a feature of lua called
"Extra Space". 

http://lua-users.org/lists/lua-l/2010-03/msg00189.html

The Extra Space feature is a way that lua can be configured, by giving a value
to a preprocessor symbol "LUAI_EXTRASPACE" in luaconf.h, so that lua_State objects
have some "empty space" of some pretermined size in their object layout where
external information can be stored. We use it to hold a pointer back to the C++
wrapper object which owns that particular lua State.

An example of using this in the wesnoth 1.13 cycle is here:
https://github.com/wesnoth/wesnoth/commit/6655b186b1bd5301f7171cd13d3307af3f700bd7

This is valuable because now not only can we move effortlessly from a
kernel::impl object to a lua_State* (its member variable), but we can also
recover the kernel::impl from the lua_State* pointer just as easily.

In our code below we use it at this line:
	kernel::impl*& get_kernel_impl(lua_State* L) { return *reinterpret_cast<kernel::impl**>(reinterpret_cast<char*>(L) - WESNOTH_KERNEL_OFFSET); }

In the kernel::impl ctor the "Extra space" pointer is initialized here:
	get_kernel_impl(lua_) = this;

When we push C++ member functions to lua, we do it as simple C functions using the 
following template:

	typedef kernel::impl kimpl;
	typedef int (kimpl::*member_function)();

	template <member_function func>
	int dispatch(lua_State* L) {
		return ((*get_kernel_impl(L)).*func)();
	}

...

	lua_pushcfunction(lua_, &dispatch<&kernel::impl::intf_print>);

This implicitly creates a "shim" function of type int(lua_State *) which is
a C-style function compatible with lua, but which immediately calls the 
appropriate member function of the owner of the lua_State.

(The `intf_print` method can be implemented the way usual lua C api 
functions are written, using the member variable lua_ as the pointer to the
lua state.)

This makes the binding between kernel::impl and lua_State * very simple
and tight -- it is just some pointer arithmetic to go from a pointer to one
to a pointer to the other. This kind of binding will look very simple in
a debugger, simpler than if we were using the boost::function full userdata
approach. There are no possibilties of exceptions being thrown like
boost::bad_function_call, or try-catch blocks necessary to handle that.

It also means that, when we copy a kernel::impl, we only need to ensure that
the extra_space pointer gets appropriately set with the new "this" of the
copy, and that otherwise the lua states can be essentially identical and it
will work out correctly. So the handful of lines above is substantially all
of our binding code for lua <-> C++.

2.) How to set up a nice lua environment for the game.

In wesnoth 1.x the lua environment does not have the friendliest syntax.
All the callbacks to the wesnoth engine are stored in a single table called
"wesnoth", so the user constantly types "wesnoth.X" to do anything. To make
it easier to casually use WML variables there are fancy metatables, but
setting these up in the first place requires some gross boilerplate.

To the extent possible, I would like for the lua environment for the game
to be based on simple assignment syntax. Rather than a "scripting API", the
idea is that a scripter should be able to think of e.g. the variable "Map"
as representing the true map of the current game that is displayed on screen,
for Units to represent the set of units, etc. etc., so that modifying these
variables and their children or fields directly results in changes in the
game. Thus `Map = "filename"` would load a file at any time, and things like
`Sides[1] =  
{
  id = ... 
  name = ...
  controller = ...
  ...
}`,

`Sides[1].recruit = "Foo, Bar, Baz"`

could be used to configure a side at any time.

In lua by default assigning a value to a field of a table does not trigger
a callback, therefore we have to use some metatable trickery so that code
like the above will fire callbacks to the external engine (anura).

Essentally we want to use a simplified version of the following lua idiom:
http://lua-users.org/wiki/ObjectProperties

So, as part of the setup of the lua environment, we want to initialize some
objects e.g. "Map, Units, Sides" for which assignment will actually change
the in-game data. The user can use any other variables if they like, for
temporary values. The idea is that these data types for the temporaries
should be as simple as possible -- e.g. Map should simply be a table whose
keys are Map Locations, and whose values are terrain code strings. Units
should be a table, which can be referenced using either Map Locations or
Ids, and yields either a Unit object or nil. Sides should be an array of
Side reference objects. These tables will use metatables to prevent entries
with the wrong keys and values from being entered.

We can develop the patterns we want to use in lua as follows:

function make_watched_table(table, watcher)
  local private = table
  local proxy_mt = {
    __index = function(self, key) return private[key] end
    __newindex = function(self, key, value) private[key] = value; watcher(table, key, value); end
    __metatable = "not allowed"
  }
  local self = setmetatable({}, proxy_mt)
  return self
end

function make_checked_table(table, key_checker, value_checker)
  local private = table
  local proxy_mt = {
    __index = function(self, key) return private[key] end
    __newindex = function(self, key, value) if (not key_checker or key_checker(key)) and (not value_checker or value_checker(value)) then private[key] = value end
    __metatable = "not allowed"
  }
  local self = setmetatable({}, proxy_mt)
  return self
end

A checked table is a table which is restricted to only contain certain keys
and values. We can use this to enforce the table layout of key tables.

A watched table has an attached "watcher" function which gets called when
the table entries are changed. The version above doesn't work recursively,
any subtables need their own watchers.

For some of the items we want to support, this is all we really need.
For instance, for the "Villages" table, and the "Labels" table, at least in this
implementation those objects are simple enough that they don't really need to be
bound to FFL objects. A village is just a table with a location, and an int which
indicates the owner -- a label has an owner and a string.

In developing the idea, there are a few more things we want to support.
1. The user should ideally only manipulate pure table values, not userdata, or
tables with fancy metatables, unless they want to. 

The external reference variables Map, Units, Sides, etc. in general will have 
fancy metatables, and could perhaps even be user data. But, the user will
essentially just use these as outputs and not need to know the details of these
metatables. The idea is that they should generally just assign to them, or
assign to their fields, and it should just work. We would like them also to be
able to read from these tables though, and when they do they should get a simple
value type and not a reference. In lua, setting "identifier = X" always assigns
identifier to a reference to X when X is a table or userdata type, and there is
no built-in copy operation. This can only be changed in general by giving the
table containing identifier a metatable, which isn't feasible. Therefore the user
if the user e.g. wants to get a simple table corresponding to "Sides[1]", they
will have to use some syntax like "X = Sides[1].copy()" and we will have to
provide a copy function.

Besides this, the "checked tables" we construct should be able to do things like,
when a table is assigned to some index, they should be able to automatically
construct a userdata type (corresponding to some FFL object). Therefore besides
being just type checked we also need to be able to override the entire __newindex
method so that it can cast the incoming value appropriately.

Therefore the full template we want to use is something like this:

function copy(X)
  local t = type(X)
  if (t == "table") then
    local ret = {}
    for k,v in pairs(X) do
      ret[k] = copy(v)
    end
    return ret
  elseif (t == "userdata") then
    return X.copy()
  else
    return X
  end
end

function make_game_table(table, methods, assignment)
  if not methods.copy then methods.copy = copy end

  local private = table
  local index = function(self, key)
    if methods[key] then return function(...) methods[key](private, ...) else return private[key] end
  end
  local newindex = function(self, key, value)
    if methods[key] then
      error("Cannot override a method " .. tostring(key))
    else
      assignment(private, key, value)
    end
  end

  if not methods.add then methods.add = function(_, key, value) newindex(_, key, value) end
  local proxy_mt = {
    __index = index
    __newindex = newindex
    __metatable = "not allowed"
  }
  local self = setmetatable({}, proxy_mt)
  return self
end


We also want to add some basic type checking functions:

function check_layout(input, pattern)
  if type(pattern) ~= "table" then error("pattern was not a table") end
  if type(input) ~= "table" then error("input was not a table") end
  local pat = copy(pattern)
  for k,v in pairs(input) do
    if not pattern[k] or (not pattern[k]:find(type(v)) and not pattern[k] == "any") then
      error("key " .. tostring(k) .. " had type ".. type(v) .. ", expected type " .. (pattern[k] or "nil"))
    end
    pat[k] = nil
  end
  if #pat > 0 then
    local error_message = ""
    for k,v in pairs(pat) do
      if v then
        error_message = error_message .. tostring(k) .. " (" ..  type(v) .. "), "
      end
    end
    if size(error_message) > 0 then
      error("found unexpected keys: " .. error_mesage .. " in input table")
    end
  end
end


With these definitions, we can construct some of the game tables the following way:

Villages = make_game_table({}, { copy = copy },
  function (table, key, value)
    if not engine.is_map_location(key) then error ("village must be keyed to a location, not " .. tostring(key)) end
    value.location = key
    if value then check_layout(value, { location = "any", owner = "number" }) end
    table[key] = value
    engine.update_village(key, value)
  end
)

Labels = make_game_table({}, { copy = copy },
  function (table, key, value)
    if not engine.is_map_location(key) then error ("label must be keyed to a location, not " .. tostring(key)) end
    value.location = key
    if value then check_layout(value, { location = "any", owner = "number", text = "string" }) end
    table[key] = value
    engine.update_label(key, value)
  end
)

Map = make_game_table({}, { copy = copy }, 
  function (table, key, value)
    if not engine.is_map_location(key) then error ("terrain must be keyed to a location, not " .. tostring(key)) end
    if value and not type(value) == "string" then error ("terrain map must be assigned terrain ids (strings) or nil") end
    table[key] = value
    engine.update_terrain(key, value)
  end
)

Sides = make_game_table({}, { copy = copy },
  function (table, key, value)
    if not (type(key) == "number" and key > 0) then error ("sides are keyed with positive integers") end
    table[key] = engine.construct_side(key, value) -- this will update external (anura) so we don't have to call an addtional update function
  end
)

Schedule = make_game_table({}, { copy = copy },
  function (table, key, value)
    if not (type(key) == "number" and key > 0) then error ("schedule is keyed with positive integers") end
    if value then check_layout(value, { id = "string", lawful_bonus = "number"}) end
    table[key] = value
  end
)

TimeAreas = make_game_table({}, { copy = copy },
  function (table, key, value)
    if not (type(key) == "number" and key > 0) then error ("time areas are keyed with positive integers") end
    if value then check_layout(value, { id = "string", lawful_bonus = "number", area = "function"}) end
    table[key] = value
  end
)

Units = make_game_table({}, { copy = copy,
add = function(table, unit)
  local u = unit
  if type(u) == "table" then
    u = engine.construct_unit(u)
  elseif u
    engine.update_unit(u)
  end
  table[u.id] = u
  table[u.location] = u
end},
  function()
    error("Please don't assign to the Units table, use Units.add for clarity.")
  end
)


And etc.

These functons could all be written using the C api instead of lua code as above, however I find
this to be less readable. Therefore, the initialization phase consists of alternating steps, switching
as is convenient between defining objects using the C++ api (mostly functions and userdata) and lua.

a.) The lua environment is initialized by adding standard libraries, then by adding all of the C++ 
engine callbacks that we will use (in table `engine`), and metatables for the userdata types that will
directly wrap anura objects (units, sides) and a few utilties like an RNG object and some similar ones
(in function `load_C_object_metatables`).

b.) A lua script `data/kernel/init.lua` assembles the engine functions into the metatables of the core
game tables.

c.) C++ cleans up after this script, by
* Deleting the "engine" table (which the user isn't meant to use directly). This leaves behind the
function references which we pushed earlier, so that they are only accessible via the metatables of the
core game objects.
* Modifying the global table _G to ensure that the user can't trash the core game tables, and that
reassigning them will trigger appropriate reconstructons and update to anura / external.
* Adds a "strict globals mode" which flags an error when the user mistypes a variable name, by treating
dereference of previously unassigned global variables as an error. Normally they silently become nil in
lua :/

3.) Lua is a pure C library, and therefore it is unsafe to allow C++ exceptions to propogate to it,
this potentially causing corruption of the lua stack and internal implementation variables.

C.f.: http://lua-users.org/lists/lua-l/2007-10/msg00473.html
http://lua-users.org/lists/lua-l/2007-10/msg00478.html

We handle this by catching any exception and turning it into a lua error. If it is necessary to pass
exceptions through lua anyways, it is possible to rig up a mechanism where they are hidden away
somewhere until we exit lua and then they are rethrown, however I would rather not do that if it can
be avoided.

************/


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

	int intf_print();

	int intf_construct_side();
	int intf_construct_unit();
	int intf_is_map_location();
	int intf_update_label();
	int intf_update_terrain();
	int intf_update_unit();
	int intf_update_village();

	bool are_allied(int side1, int side2);
};

namespace {

	// Getter which takes a lua_State and recovers the pointer to its owner from the lua EXTRA_SPACE region.
	// (The owner object stores its 'this' pointer there at construction time).
	kernel::impl*& get_kernel_impl(lua_State* L) { return *reinterpret_cast<kernel::impl**>(reinterpret_cast<char*>(L) - WESNOTH_KERNEL_OFFSET); }

	// Template which allows to push member functions of the kernel into lua as C functions, using a shim
	typedef kernel::impl kimpl;
	typedef int (kimpl::*member_function)();

	template <member_function func>
	int dispatch(lua_State* L) {
		return ((*get_kernel_impl(L)).*func)();
	}


} // end namespace detail

////
// Implement the functions of kernel_impl
////

//#define DBG_LUA std::cout

int kernel::impl::intf_print() {
	//	boost::iostreams::stream< boost::iostreams::null_sink > DBG_LUA( ( boost::iostreams::null_sink() ) );

	size_t nargs = lua_gettop(lua_);

	for (size_t i = 1; i <= nargs; ++i) {
		const char* str = lua_tostring(lua_, i);
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

	static luaL_Reg const engine_callbacks[] = {
		{ "construct_side",	&dispatch<&kernel::impl::intf_construct_side>},
		{ "construct_unit",	&dispatch<&kernel::impl::intf_construct_unit>},
		{ "is_map_location",	&dispatch<&kernel::impl::intf_is_map_location>},
		{ "update_label",	&dispatch<&kernel::impl::intf_update_label>},
		{ "update_terrain",	&dispatch<&kernel::impl::intf_update_terrain>},
		{ "update_unit",	&dispatch<&kernel::impl::intf_update_unit>},
		{ "update_village",	&dispatch<&kernel::impl::intf_update_village>},
		{ NULL, NULL }
	};

	luaL_register(lua_, "engine", engine_callbacks);

	load_C_object_metatables();

	std::string script(begin, end); // this can be done differently later

	if (load_string(script.c_str())) {
		protected_call(0, 0);
	}

	lua_settop(lua_, 0); // forcibly clear the stack

	lua_newtable(lua_);
	lua_setglobal(lua_, "engine");
}

void kernel::impl::load_C_object_metatables() {
	//lua_terrain_map::load_table();
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

int kernel::impl::intf_construct_side() {
	return 0;
}

int kernel::impl::intf_construct_unit() {
	return 0;
}
int kernel::impl::intf_is_map_location() {
	return 0;
}
int kernel::impl::intf_update_label() {
	return 0;
}
int kernel::impl::intf_update_terrain() {
	return 0;
}
int kernel::impl::intf_update_unit() {
	return 0;
}
int kernel::impl::intf_update_village() {
	return 0;
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
