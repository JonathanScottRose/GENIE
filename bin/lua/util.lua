--- Useful language constructs and helper functions.
-- Contains an OO class system implementation, enums, various iterators for
-- range-based for loops, Sets, and other tidbits. 
-- @module util

util = {}

--
-- LANGUAGE/OO SUPPORT
--

--- OO Support.
-- A simple class system. See @{Builder} for an example of its use.
-- @section oo

--- Defines a new base class.
-- The returned table, representing the class, will have an empty constructor, which is a method
-- named `__ctor`. It can be overridden with your own constructor. To actually instantiate the class,
-- one must call the `.new` static method.
--@usage
--foo = class()
--function foo:__ctor(x,y,z) ... end
--function foo:mymethod() ... end
--foo_inst = foo.new(xparam,yparam,zparam)
--foo_inst:mymethod()
-- @treturn table
function class()
	local result = {}
    setmetatable(result, result)
	
    function result.new(...)
        local inst = {}
        inst.__index = result
        setmetatable(inst, inst)
        inst:__ctor(...)
        return inst
    end
    
    function result:__ctor(...)
    end
	
	return result
end

--- Defines a subclass of an existing (sub)class.
-- @tparam table base base class
-- @treturn table
--@usage
--bar = subclass(foo)
--bar:mymethod()
function subclass(base)
    local result = class()
    result.__index = base
    return result
end

--- Enumerations.
-- @section enums

--- Creates an enumeration from an array of strings.
-- This is a bidirectional mapping between the strings and their
-- associated unique integers. The strings are pre-processed and made lowercase.
-- @tparam array names an array of unique strings
-- @treturn table a bidirectional mapping of strings to integers
function enum(names)
	local result = {}
	for num,name in ipairs(names) do
        local lname = string.lower(name)
		result[lname] = num
		result[num] = lname
	end
	return result
end

--
-- ITERATION
--

--- Iterators.
-- Used for range-based `for` loops.
--@usage
--t = { ['a']=1, [3.1415]='hello', ['beef']='jerky' }
--for v in values(t) do
--  ...
--end
-- @section iteration

--- Generic table iterator.
-- Used by specialized iterators.
-- @tparam table t
-- @tparam[opt] function(table,a,b) order returns true if `a` should precede `b`
-- @tparam[opt] func extr(k,v) extractor function
-- @return a for-loop iterator that iterates over `t` using an optional ordering, returning a `key,val` pair on each
-- iteration, or returning `extr(key,val)` instead if `extr` was specified
function iter(t, order, extr)
	local keys = {}
	for k in pairs(t) do
		table.insert(keys, k)
	end
	
	if order then
		table.sort(keys, function(a,b) return order(t, a, b) end)
	end
	
	local i = 0
	return function()
		i = i + 1
		if keys[i] ~= nil then
			k,v = keys[i], t[keys[i]]
			if extr then return extr(k,v)
			else return k,v
			end
		end
	end
end

-- extract key
local function extr_k(k,v)
	return k
end
	
-- extract value
local function extr_v(k,v)
	return v
end

-- redirects sorting function to work on values rather than keys
local function sort_v(func)
	return function(t,a,b)
		if func then return func(t[a], t[b])
		else return t[a] < t[b]
		end
	end
end

local function sort_k(func)
	return function(t,a,b)
		if func then return func(a,b)
		else return a < b
		end
	end
end

--- Iterate over table keys, unsorted.
-- @tparam table t
-- @return iterator
function keys(t)
	return iter(t, nil, extr_k)
end

--- Iterate over keys, sorted by keys (default ascending order, can be overridden)
-- @tparam table t
-- @tparam[opt] function(a,b) order returns true if `a` should precede `b`
-- @return iterator
function skeysk(t, order)
	return iter(t, sort_k(order), extr_k)
end

--- Iterate over keys, sorted by values (default ascending order, can be overridden)
-- @tparam table t
-- @tparam[opt] function(a,b) order returns true if `a` should precede `b`
-- @return iterator
function skeysv(t, order)
	return iter(t, sort_v(order), extr_k)
end

--- Iterate over table values, unsorted.
-- @tparam table t
-- @return iterator
function values(t)
	return iter(t, nil, extr_v)
end

--- Iterate over values, sorted by keys (default ascending order, can be overridden)
-- @tparam table t
-- @tparam[opt] function(a,b) order returns true if `a` should precede `b`
-- @return iterator
function svaluesk(t, order)
	return iter(t, sort_k(order), extr_v)
end

--- Iterate over values, sorted by values (default ascending order, can be overridden)
-- @tparam table t
-- @tparam[opt] function(a,b) order returns true if `a` should precede `b`
-- @return iterator
function svaluesv(t, order)
	return iter(t, sort_v(order), extr_v)
end

--- Iterate over key/value pairs, sorted by keys (default ascending order, can be overridden)
-- @tparam table t
-- @tparam[opt] function(a,b) order returns true if `a` should precede `b`
-- @return iterator
function spairsk(t, order)
	return iter(t, sort_k(order))
end

--- Iterate over key/value pairs, sorted by values (default ascending order, can be overridden)
-- @tparam table t
-- @tparam[opt] function(a,b) order returns true if `a` should precede `b`
-- @return iterator
function spairsv(t, order)
	return iter(t, sort_v(order))
end

--
-- SETS
--

--- Sets.
-- Set-like tables.
-- Stored elements comprise the keys, and the values are just dummy `true`s.
-- Faciliates easy addition, removal, membership testing.
-- Functions are stored in a table called Set. This is not a class.
-- @section sets

Set = 
{
	--- Add an element to set.
	-- @function Set.add
	-- @tparam table s set
	-- @param o object
	add = function(s, o)        
		s[o] = true
	end,
	
	--- Remove an element from set.
	-- @function Set.del
	-- @tparam table s set
	-- @param o object
	del = function(s, o)
		s[o] = nil
	end,
    
	--- Empties the set.
	-- @function Set.clear
	-- @tparam table s set
    clear = function(s)
        for k in pairs (s) do
            s[k] = nil
        end
    end,
	
	--- Make a set from an array.
	-- Calls @{Set.add} on each element.
	-- @function Set.make
	-- @tparam array list
	-- @return set
	make = function(list)
		local result = {}
		for k,v in pairs(list) do
			result[v] = true
		end
		return result
	end,
	
	--- Iterates over values of the set.
	-- @function Set.values
	-- @tparam table s set
	-- @return iterator
	values = function(s)
        -- important! sort the keys to ensure deterministic iteration order
		return skeysk(s)
	end,
	
	--- Iterates over the given items.
	-- Probably equivalent to calling @{values} on an array directly.
	-- @function Set.mkvalues
	-- @tparam array list
	-- @return iterator
	mkvalues = function(list)
		return Set.values(Set.make(list))
	end
}


--
-- FUNCTIONS
--

--- Functions.
-- The rest.
-- @section functions

--- Tests membership in array.
-- @tparam array x an array (NOT a set)
-- @param y value (not key!) to search for
-- @treturn bool
function util.is_member(x, y)
	for _,v in ipairs(y) do
		if v == x then return true end
	end
	return false
end

--- Copies contents of one table into another.
-- @tparam table src source table
-- @tparam table dest dest table, need not be empty
function util.copy_fields(src, dest)
	for k,v in pairs(src) do
		dest[k] = v
	end
end

--- Recursively print table contents.
-- It's smart! Avoids following circular references. 
-- Also prints non-table objects, but nothing special is done for them.
-- @tparam table t
-- @tparam[opt=1000] number depth maximum traversal depth
-- @tparam[optchain=""] string indent indentation header

function util.print(t, depth, indent, visited)
	if not depth then depth = 1000 elseif depth == 0 then return end
	indent = indent or ""
	visited = visited or {}
	
	if (type(t) == "table") then
		if visited[t] then return end
		Set.add(visited, t)
		
		if util.empty(t) then
			print (indent .. "empty table")
		else
			for k,v in pairs(t) do
				print (indent .. tostring(k), v)
				if type(k) == "table" and not util.empty(k) then
					util.print(k, depth-1, indent .. "  ", visited)
				end
				if type(v) == "table" and not util.empty(v) then
					util.print(v, depth-1, indent .. "  ", visited)
				end
			end
		end
	else
		if not t then t = 'nil' end
		print (indent .. t)
	end
end

--- Returns if table is empty.
-- @tparam table t
-- @treturn boolean
function util.empty(t)
	return next(t) == nil
end

--- Returns size of table.
-- Table doesn't have to be an array.
-- @tparam table t
-- @treturn number
function util.count(t)
	local result = 0
	for _,i in pairs(t) do
		result = result + 1
	end
	return result
end

--- Appends the values of one table to another.
-- The values of tsrc get inserted with table.insert into tdest.
-- @tparam table tdest
-- @tparam table tsrc
-- @treturn table tdest, after concatenation
function util.append_vals(tdest, tsrc)
	for v in values(tsrc) do
		table.insert(tdest, v)
	end
end

--- Generates a unique table key from a prefix.
-- If `base` is "foo", then this will return the first "fooN"
-- that does not yet exist as a key in the table, where N is an integer.
-- @tparam table t
-- @tparam string base prefix
-- @treturn string
function util.unique_key(t, base)
	local n = 0
	local k = ""
	for _,v in pairs(t) do
		k = base .. tostring(n)
		if t[k] then 
			n = n + 1
		else
			break
		end
	end
	return k
end

--- Concatenate arguments with underscores.
-- @param ... a bunch of strings
-- @treturn string
function util.con_cat(...)
    local result = ''
    local args = {...}
    for i=1,#args do
        result = result .. args[i]
        if i ~= #args then result = result .. '_' end
    end
    return result
end

--- Replace all dots in a string with underscores.
--- This is useful for operating on hierarchical names.
-- @param str input string
-- @treturn string
function util.dot_to_uscore(str)
	return string.gsub(str, '%.', '_')
end

--- Creates an empty multi-dimensional table.
-- Dimensions are 0-indexed.
-- @param ... sizes of each dimension
-- @treturn table
function util.md_array(...)
	local args = {...}
	
	-- Recursively creates sub-tables
	local function init_dim(t, d)
		if d >= #args then return end
		local dim = args[d]
		
		for i=0,dim-1 do
			t[i] = {}
			init_dim(t[i], d+1)
		end
	end
	
	local result = {}
	init_dim(result, 1)
	
	return result
end

--- Returns ceiling of log base 2.
-- @param number
-- @treturn integer
function util.clog2(x)
	return math.ceil(math.log(x, 2))
end
			
		