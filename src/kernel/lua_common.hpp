#pragma once

struct lua_State;

/**
 * Pushes the value found by following the variadic names (char *), if the
 * value is not nil.
 * @return true if an element was pushed.
 */
bool luaW_getglobal(lua_State *L, ...);

bool luaW_toboolean(lua_State *l, int n);
