#include "lua_common.hpp"
#include "wml.hpp"

#include "eris/lauxlib.h"
#include "eris/lua.h"

#include <boost/foreach.hpp>

#define return_misformed() \
  do { lua_settop(L, initial_top); return false; } while (0)

bool luaW_toconfig(lua_State*L, int index, wml::config & cfg) {
	if (!lua_checkstack(L, LUA_MINSTACK))
		return false;

	// Get the absolute index of the table.
	int initial_top = lua_gettop(L);
	if (-initial_top <= index && index <= -1)
		index = initial_top + index + 1;

	switch (lua_type(L, index))
	{
		case LUA_TTABLE:
			break;
		case LUA_TNONE:
		case LUA_TNIL:
			return true;
		default:
			return false;
	}

	// First convert the children (integer indices).
	for (int i = 1, i_end = lua_rawlen(L, index); i <= i_end; ++i)
	{
		lua_rawgeti(L, index, i);
		if (!lua_istable(L, -1)) return_misformed();
		lua_rawgeti(L, -1, 1);
		char const *m = lua_tostring(L, -1);
		if (!m) return_misformed();

		struct wml::body child;
		child.name = m;

		lua_rawgeti(L, -2, 2);

		if (!luaW_toconfig(L, -1, child.children))
			return_misformed();

		cfg.push_back(child); // todo -- reduce needless copying here

		lua_pop(L, 3);
	}

	// Then convert the attributes (string indices).
	for (lua_pushnil(L); lua_next(L, index); lua_pop(L, 1))
	{
		if (lua_isnumber(L, -2)) continue;
		if (!lua_isstring(L, -2)) return_misformed();
		const char * key = lua_tostring(L, -2);
		if (!key) return_misformed();
		if (!lua_isstring(L, -1)) return_misformed();
		const char * value = lua_tostring(L, -2);
		if (!value) return_misformed();

/*		const char * v;
		switch (lua_type(L, -1)) {
			case LUA_TBOOLEAN:
			case LUA_TNUMBER:
			case LUA_TSTRING:
				v = lua_tostring(L, -1);
				break;
			default:
				return_misformed();
		}*/
		wml::Pair p = std::make_pair(key, value);
		cfg.push_back(p);
	}

	lua_settop(L, initial_top);
	return true;
}

struct body_pusher {
	body_pusher(lua_State *l)
		: L(l)
		, node_count(0)
	{}

	void operator()(const wml::body & b);

	lua_State * L;
	int node_count;
};

struct node_pusher : boost::static_visitor<> {
	node_pusher(lua_State * l)
		: L(l)
	{}

	lua_State * L;

	void operator()(const wml::Pair & p) {
		lua_pushstring(L, p.second.c_str());
		lua_setfield(L, -2, p.first.c_str());
	}

	void operator()(const wml::body & b) {
		body_pusher p(L);
		p(b);
	}
};

	void body_pusher::operator()(const wml::body & b) {
		++node_count;

		lua_createtable(L, 2, 0);

		lua_pushstring(L, b.name.c_str());
		lua_rawseti(L, -2, 1);

		lua_newtable(L);
		node_pusher new_pusher(L);
		BOOST_FOREACH(const auto & x, b.children) {
			boost::apply_visitor(new_pusher, x);
		}
		lua_rawseti(L, -2, 2);

		lua_rawseti(L, -2, node_count);
	}

void luaW_pushconfig(lua_State*L, const wml::config & cfg) {
	lua_newtable(L);
	node_pusher n_pusher(L);
	BOOST_FOREACH(const wml::node & x, cfg) {
		boost::apply_visitor(n_pusher, x);
	}
}

void luaW_pushbody(lua_State *L, const wml::body & body) {
	body_pusher b_pusher(L);
	b_pusher(body);
}

#ifdef __GNUC__
__attribute__((sentinel))
#endif
bool luaW_getglobal(lua_State *L, ...)
{
	lua_pushglobaltable(L);
	va_list ap;
	va_start(ap, L);
	while (const char *s = va_arg(ap, const char *))
	{
		if (!lua_istable(L, -1)) goto discard;
		lua_pushstring(L, s);
		lua_rawget(L, -2);
		lua_remove(L, -2);
	}

	if (lua_isnil(L, -1)) {
		discard:
		va_end(ap);
		lua_pop(L, 1);
		return false;
	}
	va_end(ap);
	return true;
}

bool luaW_toboolean(lua_State *L, int n)
{
	return lua_toboolean(L,n) != 0;
}
