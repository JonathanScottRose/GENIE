util = {}

-- language support

function class(members)
	local result = members or {}
	result.__index = result
	
	function result:_init_inst(o)
		o = o or {}
		setmetatable(o, self)
		return o
	end
	
	function result:new(o)
		return self:_init_inst(o)
	end
	
	function result:subclass(o)
		local c = self:_init_inst(o)
		c.__index = c
		return c
	end
	
	return result
end

function enum(names)
	local result = {}
	for num,name in ipairs(names) do
		result[name] = num
		result[num] = name
	end
	return result
end

function keys(tab)
	return 
		function (t, i)
			local k,v = next(t, i)
			return k
		end,
		tab,
		nil
end

-- types

Set = 
{
	add = function(s, o)
		s[o] = true
	end,
	
	del = function(s, o)
		s[o] = nil
	end,
	
	make = function(list)
		local result = {}
		for k,v in pairs(list) do
			result[v] = true
		end
		return result
	end,
	
	values = function(s)
		return keys(s)
	end,
	
	mkvalues = function(list)
		return Set.values(Set.make(list))
	end
}


-- utility functions

function util.error(e, level)
	print(debug.traceback())
	error(e, level)
end

function util.assert(cond)
	if not cond then
		print(debug.traceback())
		assert(false)
	end
end

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
			k = k + 1
		else
			break
		end
	end
	return k
end
			
		