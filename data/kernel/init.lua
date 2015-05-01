print("The engine is being initialized ($$$)")
print("Checking type function")
print(type(type))

-- EVENT layout:
-- { 
--	name
--	id
--	conditional
--	action
-- }

-- Events is a TABLE of ARRAYS, indexed by NAME

-- MENU_ITEM layout:
-- { 
--	id
--	show_if
--	conditional
--	action
-- }

-- engine.wml_menu_items is a TABLE indexed by id

-- Variables is a TABLE of string | numeric | table. Meant to be backwards compatible with wesnoth 1 and used for variable substitution

-- Labels is a TABLE, representing a map: map_location -> { string text, int player, bool share }

-- Villages is a TABLE, representing a map: map_location -> { int owner | 0 for none }

-- Map is a USERDATA, can use get_terrain(map_location)

-- Units is a USERDATA, can use get(map_location) or get(id), and can "put" which is like unstore
-- Units is a big heavy lifter -- pushing units is what updates the vision data structures
-- Make units with Unit(...) or LuaUnit(...)
-- Units are USERDATA also, being bound to custom objects somehow

-- Sides is a TABLE, usable as an array but with some custom functions for Allied With 
-- A side is userdata but is basically static table data typically.
-- Sides contain FOG and SHROUD override data, stored essentially as a map_location -> optional bool table. (But it's really userdata.)
-- They contain "proxy shroud data" for "true" vision and "adjusted" vision of both kinds.
-- Under Sides[k].Vision

-- Pathing is a table of functions to find distances, directions, and shortest paths
-- And get adjacent locations
-- (Topology?)

-- TimeOfDay.Schedule is the current (default) TOD schedule
-- TimeOfDay.TimeAreas is the array of registered time areas entries


---------------
-- UTILITIES --
---------------

-- Make a recursive deep copy. Ignores metatables, and doesn't attempt to deep-copy functions.
-- Really intended only for making copies of table data and userdata.
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

-- Make a table into a game table. Assignment to the table is overrided.
-- A methods table allows functions to be added, such that the syntax
-- `t.func(x,y,z)` will result in a call to `methods.func(t, x, y, z)`.
function make_game_table(table, methods, assignment)
  if not methods.copy then methods.copy = copy end

  local private = table
  local index = function(self, key)
    if methods[key] then 
      return function(...) methods[key](private, ...) end 
    else
      return private[key]
    end
  end
  local newindex = function(self, key, value)
    if methods[key] then
      error("Cannot override a method " .. tostring(key))
    else
      assignment(private, key, value)
    end
  end

  if not methods.add then methods.add = newindex end

  local proxy_mt = {
    __index = index
    __newindex = newindex
    __metatable = "not allowed"
  }
  local self = setmetatable({}, proxy_mt)
  return self
end

-- A simple sanity checking mechanism. Given an input table,
-- this function checks that the types of its fields match a "pattern"
-- table. 
--
-- The pattern table should have keys and values as strings.
-- Each key of the input should correspond to a key of the pattern,
-- and its type should match the corresponding pattern[key] string,
-- either with equality or it should be a substring (member of list).
-- The pattern can also have value "any" which permits any value.
-- A lua error is flagged if the input does not match the pattern.
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
      if v and v ~= "any" then
        error_message = error_message .. tostring(k) .. " (" ..  type(v) .. "), "
      end
    end
    if error_message:len() > 0 then
      error("found unexpected keys: " .. error_mesage .. " in input table")
    end
  end
end


-----------------
-- GAME TABLES --
-----------------

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


-- WML variables needs no typechecking -- we only support certain types, and if the user hacks it and adds
-- types that don't fit, then substitution will just be aborted for that variable. Since we don't need
-- WML variables except for backwards compatibility, it should only be used by code that we generate for
-- compatiblity, so this is acceptable imo.

Variables = {}

-- This table could be a game table but it doesn't really need to be, since we don't allow assigning to it directly.

Events = {}

-- Normally static
--MoveTypes = {}
--TerrainTypes = {}
--UnitTypes = {}

-- Some variables to handle event firing
engine.raised = {}

function engine.scoped_variables(t)
	local backup = {}
	for var,val in pairs(t)
		backup[var] = Variables[var]
		Variables[var] = val
	end
	return backup
end

function engine.restore_variables(t)
	for key,value in pairs(t)
		Variables[key] = value
	end
end

function engine.fire_handler(handler, context)
	local backup_vars = engine.scoped_variables(context)
	if not ev.conditional or (type(ev.conditional) == "function") and ev.conditional()) or (type(ev.conditional) == "string") and loadstring(ev.conditional)()) then
		if type(ev.action) == "function" then
			ev.action()
		elseif type(ev.action) == "string" then
			loadstring(ev.action)()
		else
			error("Fire: fired an event whose action was type " .. type(ev.action))
		end
	end
	engine.restore_variables(backup_vars)
end

function Event(input) 
	if not input.name then error("Event: requires a name") end
	if type(input.name) ~= "string" then error("Event: name must be a string, not a " .. type(input.name)) end

	if not Events[input.name] then 
		Events[input.name] = {}
	end

	table.insert(Events.(input.name), input)
end

function Fire(name, context)
	if type(name) ~= "string" then error("Fire: event name must be a string, not a " .. type(name)) end
	if type(context) == "nil" then context = {} end
	if type(context) ~= "table" then error("Fire: event context must be a table or nil, not a " .. type(name)) end

	table.insert(engine.raised, {name = name, context = context})

	if #engine.raised > 1 then return end -- Someone is already firing events, we don't need to start.

	while #engine.raised > 0 do

		local fname = engine.raised[1].name or error("Fire: raised something with null name")
		local fcontext = engine.raised[1].context

		table.remove(engine.raised, 1)

		local events_to_fire = Events.(fname)
		Events.(fname) = {}

		for _, ev in ipairs(events_to_fire) do
			engine.fire_handler(ev, fcontext)
		end
	end
end

function Unit(u)
	Units.add(u)
end

function Side(s)
	Sides.add(s)
end

--[[ DataDumper.lua
Copyright (c) 2007 Olivetti-Engineering SA

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
]]

local dumplua_closure = [[
local closures = {}
local function closure(t) 
  closures[#closures+1] = t
  t[1] = assert(loadstring(t[1]))
  return t[1]
end

for _,t in pairs(closures) do
  for i = 2,#t do 
    debug.setupvalue(t[1], i-1, t[i]) 
  end 
end
]]

local lua_reserved_keywords = {
  'and', 'break', 'do', 'else', 'elseif', 'end', 'false', 'for', 
  'function', 'if', 'in', 'local', 'nil', 'not', 'or', 'repeat', 
  'return', 'then', 'true', 'until', 'while' }

local function keys(t)
  local res = {}
  local oktypes = { stringstring = true, numbernumber = true }
  local function cmpfct(a,b)
    if oktypes[type(a)..type(b)] then
      return a < b
    else
      return type(a) < type(b)
    end
  end
  for k in pairs(t) do
    res[#res+1] = k
  end
  table.sort(res, cmpfct)
  return res
end

local c_functions = {}
for _,lib in pairs{'_G', 'string', 'table', 'math', 
    'io', 'os', 'coroutine', 'package', 'debug'} do
  local t = _G[lib] or {}
  lib = lib .. "."
  if lib == "_G." then lib = "" end
  for k,v in pairs(t) do
    if type(v) == 'function' and not pcall(string.dump, v) then
      c_functions[v] = lib..k
    end
  end
end

---------------
-- UTILITIES --
---------------

-- Add a "dump function"
function DataDumper(value, varname, fastmode, ident)
  local defined, dumplua = {}
  -- Local variables for speed optimization
  local string_format, type, string_dump, string_rep = 
        string.format, type, string.dump, string.rep
  local tostring, pairs, table_concat = 
        tostring, pairs, table.concat
  local keycache, strvalcache, out, closure_cnt = {}, {}, {}, 0
  setmetatable(strvalcache, {__index = function(t,value)
    local res = string_format('%q', value)
    t[value] = res
    return res
  end})
  local fcts = {
    string = function(value) return strvalcache[value] end,
    number = function(value) return value end,
    boolean = function(value) return tostring(value) end,
    ['nil'] = function(value) return 'nil' end,
    ['function'] = function(value) 
      return string_format("loadstring(%q)", string_dump(value)) 
    end,
    userdata = function() error("Cannot dump userdata") end,
    thread = function() error("Cannot dump threads") end,
  }
  local function test_defined(value, path)
    if defined[value] then
      if path:match("^getmetatable.*%)$") then
        out[#out+1] = string_format("s%s, %s)\n", path:sub(2,-2), defined[value])
      else
        out[#out+1] = path .. " = " .. defined[value] .. "\n"
      end
      return true
    end
    defined[value] = path
  end
  local function make_key(t, key)
    local s
    if type(key) == 'string' and key:match('^[_%a][_%w]*$') then
      s = key .. "="
    else
      s = "[" .. dumplua(key, 0) .. "]="
    end
    t[key] = s
    return s
  end
  for _,k in ipairs(lua_reserved_keywords) do
    keycache[k] = '["'..k..'"] = '
  end
  if fastmode then 
    fcts.table = function (value)
      -- Table value
      local numidx = 1
      out[#out+1] = "{"
      for key,val in pairs(value) do
        if key == numidx then
          numidx = numidx + 1
        else
          out[#out+1] = keycache[key]
        end
        local str = dumplua(val)
        out[#out+1] = str..","
      end
      if string.sub(out[#out], -1) == "," then
        out[#out] = string.sub(out[#out], 1, -2);
      end
      out[#out+1] = "}"
      return "" 
    end
  else 
    fcts.table = function (value, ident, path)
      if test_defined(value, path) then return "nil" end
      -- Table value
      local sep, str, numidx, totallen = " ", {}, 1, 0
      local meta, metastr = (debug or getfenv()).getmetatable(value)
      if meta then
        ident = ident + 1
        metastr = dumplua(meta, ident, "getmetatable("..path..")")
        totallen = totallen + #metastr + 16
      end
      for _,key in pairs(keys(value)) do
        local val = value[key]
        local s = ""
        local subpath = path
        if key == numidx then
          subpath = subpath .. "[" .. numidx .. "]"
          numidx = numidx + 1
        else
          s = keycache[key]
          if not s:match "^%[" then subpath = subpath .. "." end
          subpath = subpath .. s:gsub("%s*=%s*$","")
        end
        s = s .. dumplua(val, ident+1, subpath)
        str[#str+1] = s
        totallen = totallen + #s + 2
      end
      if totallen > 80 then
        sep = "\n" .. string_rep("  ", ident+1)
      end
      str = "{"..sep..table_concat(str, ","..sep).." "..sep:sub(1,-3).."}" 
      if meta then
        sep = sep:sub(1,-3)
        return "setmetatable("..sep..str..","..sep..metastr..sep:sub(1,-3)..")"
      end
      return str
    end
    fcts['function'] = function (value, ident, path)
      if test_defined(value, path) then return "nil" end
      if c_functions[value] then
        return c_functions[value]
      elseif debug == nil or debug.getupvalue(value, 1) == nil then
        return string_format("loadstring(%q)", string_dump(value))
      end
      closure_cnt = closure_cnt + 1
      local res = {string.dump(value)}
      for i = 1,math.huge do
        local name, v = debug.getupvalue(value,i)
        if name == nil then break end
        res[i+1] = v
      end
      return "closure " .. dumplua(res, ident, "closures["..closure_cnt.."]")
    end
  end
  function dumplua(value, ident, path)
    return fcts[type(value)](value, ident, path)
  end
  if varname == nil then
    varname = "return "
  elseif varname:match("^[%a_][%w_]*$") then
    varname = varname .. " = "
  end
  if fastmode then
    setmetatable(keycache, {__index = make_key })
    out[1] = varname
    table.insert(out,dumplua(value, 0))
    return table.concat(out)
  else
    setmetatable(keycache, {__index = make_key })
    local items = {}
    for i=1,10 do items[i] = '' end
    items[3] = dumplua(value, ident or 0, "t")
    if closure_cnt > 0 then
      items[1], items[6] = dumplua_closure:match("(.*\n)\n(.*)")
      out[#out+1] = ""
    end
    if #out > 0 then
      items[2], items[4] = "local t = ", "\n"
      items[5] = table.concat(out)
      items[7] = varname .. "t"
    else
      items[2] = varname
    end
    return table.concat(items)
  end
end

-- Define a shortcut function for testing
function dump(...)
  print(DataDumper(...), "\n---")
end
