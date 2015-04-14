print("The kernel is being initialized ($$$)")
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

-- kernel.wml_menu_items is a TABLE indexed by id

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

Events = {}
Labels = {}
Map = {}
Pathing = {}
Sides = {}
TimeOfDay = {}
TimeOfDay.Schedule = {}
TimeOfDay.TimeAreas = {}
Units = {}
Variables = {}
Villages = {}

-- Normally static
--MoveTypes = {}
--TerrainTypes = {}
--UnitTypes = {}

kernel = {}
kernel.raised = {}
kernel.command_wml = {}
kernel.action_wml = {}
kernel.conditional_wml = {}
kernel.wml_menu_items = {}
kernel.map_lock = false
kernel.state_lock = false

function kernel.scoped_variables(t)
	local backup = {}
	for key,value in pairs(t)
		backup.key = Variables.key
		Variables.key = value
	end
	return backup
end

function kernel.restore_variables(t)
	for key,value in pairs(t)
		backup.key = Variables.key
		Variables.key = value
	end
end

function kernel.fire_handler(handler, context)
	local backup_vars = kernel.scoped_variables(fcontext)
	if not ev.conditional or (type(ev.conditional) == "function") and ev.conditional()) or (type(ev.conditional) == "string") and loadstring(ev.conditional)()) then
		if type(ev.action) == "function" then
			ev.action()
		elseif type(ev.action) == "string" then
			loadstring(ev.action)()
		else
			error("Fire: fired an event whose action was type " .. type(ev.action))
		end
	end
	kernel.restore_variables(backup_vars)
end

function Event(input) 
	if not input.name then error("Event: requires a name") end
	if type(input.name) ~= "string" then error("Event: name must be a string, not a " .. type(input.name)) end

	if not Events.(input.name) then 
		Events.(input.name) = {}
	end

	table.insert(Events.(input.name), input)
end

function Fire(name, context)
	if type(name) ~= "string" then error("Fire: event name must be a string, not a " .. type(name)) end
	if type(context) ~= "table" and type(context) ~= "nil" then error("Fire: event context must be a table, not a " .. type(name)) end
	if type(context) == "nil" then context = {} end

	table.insert(kernel.raised, {name = name, context = context})

	if #kernel.raised > 1 then return end -- Someone is already firing events, we don't need to start.

	while #kernel.raised > 0 do

		local fname = kernel.raised[1].name or error("Fire: raised something with null name")
		local fcontext = kernel.raised[1].context

		table.remove(kernel.raised, 1)

		local events_to_fire = Events.(fname)
		Events.(fname) = {}

		for _, ev in ipairs(events_to_fire) do
			kernel.fire_handler(ev, fcontext)
		end
	end
end
