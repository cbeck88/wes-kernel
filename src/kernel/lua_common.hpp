#pragma once

#include "wml.hpp"

struct lua_State;

/**
 * Converts a table at given index to a WML config (inside of a tag)
 */
bool luaW_toconfig(lua_State *L, int index, wml::config &);

/**
 * Pushes a WML config
 */
void luaW_pushconfig(lua_State *L, const wml::config &);
void luaW_pushbody(lua_State *L, const wml::body &);

/**
 * Pushes the value found by following the variadic names (char *), if the
 * value is not nil.
 * @return true if an element was pushed.
 */
bool luaW_getglobal(lua_State *L, ...);

bool luaW_toboolean(lua_State *l, int n);
