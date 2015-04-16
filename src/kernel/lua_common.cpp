#include "lua_common.hpp"

#include "eris/lauxlib.h"
#include "eris/lua.h"

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
