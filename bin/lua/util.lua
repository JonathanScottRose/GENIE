util = {}

--
-- LANGUAGE/OO SUPPORT
--

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

function subclass(base)
    local result = class()
    result.__index = base
    return result
end

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

-- generic iterator: table, order(key,val), extractor to process each iterated key,val
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

-- over keys, unsorted
function keys(t)
	return iter(t, nil, extr_k)
end

-- over keys, sorted by order(keys)
function skeysk(t, order)
	return iter(t, sort_k(order), extr_k)
end

-- over keys, sorted by order(values)
function skeysv(t, order)
	return iter(t, sort_v(order), extr_k)
end

-- over values, unsorted
function values(t)
	return iter(t, nil, extr_v)
end

-- over values, sorted by order(keys)
function svaluesk(t, order)
	return iter(t, sort_k(order), extr_v)
end

-- over values, sorted by order(values)
function svaluesv(t, order)
	return iter(t, sort_v(order), extr_v)
end

-- over pairs, sorted by order(keys)
function spairsk(t, order)
	return iter(t, sort_k(order))
end

-- over pairs, sorted by order(values)
function spairsv(t, order)
	return iter(t, sort_v(order))
end

--
-- TYPES
--

Set = 
{
	add = function(s, o)        
		s[o] = true
	end,
	
	del = function(s, o)
		s[o] = nil
	end,
    
    clear = function(s)
        for k in pairs (s) do
            s[k] = nil
        end
    end,
	
	make = function(list)
		local result = {}
		for k,v in pairs(list) do
			result[v] = true
		end
		return result
	end,
	
	values = function(s)
        -- important! sort the keys to ensure deterministic iteration order
		return skeysk(s)
	end,
	
	mkvalues = function(list)
		return Set.values(Set.make(list))
	end
}


--
-- FUNCTIONS
--

function util.is_member(x, y)
	for _,v in ipairs(y) do
		if v == x then return true end
	end
	return false
end

function util.copy_fields(src, dest)
	for k,v in pairs(src) do
		dest[k] = v
	end
end

function util.insert_unique(v, tab)
	util.assert(v.name)
	if tab[v.name] then
		util.error(v.name .. " already exists")
	end
	tab[v.name] = v
end

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

function util.empty(t)
	return next(t) == nil
end

function util.count(t)
	local result = 0
	for _,i in pairs(t) do
		result = result + 1
	end
	return result
end

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

function util.con_cat(...)
    local result = ''
    local args = {...}
    for i=1,#args do
        result = result .. args[i]
        if i ~= #args then result = result .. '_' end
    end
    return result
end
			
		