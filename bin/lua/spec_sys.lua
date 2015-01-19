require "util"

-- Create this package and set up environment

if not spec then spec = {} end
setmetatable(spec, {__index = _ENV})
_ENV = spec


-- SysObject

SysObject = class
{
	Types = Set.make {'INSTANCE', 'EXPORT'},
	
	name = nil,
	parent = nil,
	type = nil
}

-- Instance

Instance = SysObject:subclass
{
	type = 'INSTANCE',
	component = nil,
	param_bindings = {}
}

function Instance:new(o)
	o = Instance:_init_inst(o)
	o.param_bindings = {}
	return o
end

function Instance:bind_param(param, value)
	self.param_bindings[param] = value
end

-- Export

Export = SysObject:subclass
{
	type = 'EXPORT',
	iface_type = nil,
	iface_dir = nil
}

-- LinkTarget

LinkTarget = class
{
	obj = nil,
	iface = "iface",
	lp = "lp"
}

function LinkTarget:parse(str)
	m = {}
	for val in string.gmatch(str, "[^.]+") do
		table.insert(m, val)
	end
	self.obj = m[1] or self.obj
	self.iface = m[2] or self.iface
	self.lp = m[3] or self.lp
end

function LinkTarget:str()
	return self.obj .. "." .. self.iface .. "." .. self.lp
end

function LinkTarget:phys()
	return self.obj .. "." .. self.iface
end
	

-- Link

Link = class
{
	src = nil,
	dest = nil
}

function Link:new(o)
	o = Link:_init_inst(o)
	self.src = LinkTarget:new()
	self.dest = LinkTarget:new()
	return o
end

-- System

System = class
{
	name = nil,
	parent = nil,
	links = {},
	objects = {},
	topo_func = nil
}

function System:new(o)
	o = System:_init_inst(o)
	o.links = {}
	o.objects = {}
	return o
end

function System:add_link(link)
	table.insert(self.links, link)
end

function System:add_object(obj)
	obj.parent = self
	util.insert_unique(obj, self.objects)
end
	
function System:create_auto_exports()
	local is_connected = {}
	
	for link in values(self.links) do
		is_connected[link.src:str()] = true
		is_connected[link.dest:str()] = true
	end
	
	-- to insert
	local new_objs = {}
	
	for obj in values(self.objects) do
		if obj.type ~= 'INSTANCE' then goto nextobj end
	
		local src_lt = LinkTarget:new()
		src_lt.obj = obj.name
		
		local comp = self.parent.components[obj.component]
		for iface in values(comp.interfaces) do
			src_lt.iface = iface.name
			
			local is_out = iface.dir == 'OUT'
			
			for lp in values(iface.linkpoints) do
				src_lt.lp = lp.name
				
				if not is_connected[src_lt:str()] and util.count(iface.linkpoints) < 2 then
					local exname = obj.name .. "_" .. iface.name
					local src_lt = LinkTarget:new
					{
						obj = obj.name,
						iface = iface.name,
						lp = lp.name
					}
					
					local exp_lt = LinkTarget:new { obj = exname }
					local new_export = Export:new
					{
						name = exname,
						iface_type = iface.type,
						iface_dir = iface.dir
					}
					
					local new_link = Link:new()
					if (is_out) then
						new_link.src = src_lt
						new_link.dest = exp_lt
					else
						new_link.src = exp_lt
						new_link.dest = src_lt
					end

					self:add_link(new_link)
					Set.add(new_objs, new_export) -- add later
				end
			end
		end
		
		::nextobj::
	end
	
	-- add here, after iteration!
	for obj in Set.values(new_objs) do
		self:add_object(obj)
	end
end

function System:create_default_reset()
	-- check if system has a reset already
	for obj in values(self.objects) do
		if obj.iface_type == "RESET" then return end
	end
	
	-- create a reset export, connected to nothing
	self:add_object(Export:new
	{
		name = "reset",
		iface_type = "RESET",
		iface_dir = "IN"
	})
	
	-- for the component representing the system, create a reset sink
	-- interface with one reset signal
	local comp = self.parent.components[self.name]
	local iface = comp:add_interface(Interface:new
	{
		name = "reset",
		type = "RESET",
		dir = "IN"
	})
	
	iface:add_signal(Signal:new
	{
		type = "RESET",
		binding = "reset",
		width = 1
	})
end

function System:componentize()
	-- check for existing component with same name
	local new_comp = self.parent.components[self.name]
	if new_comp then
		util.error("Can't componentize system " .. self.name .. " because a component with the same \
			name already exists")
	end
	
	-- create the new component for this system and register it with the parent Spec
	new_comp = self.parent:add_component(Component:new
	{
		name = self.name,
		module = self.name
	})
	
	-- to ensure we only visit exports once (for clocks, resets)
	local visited = {}
	
	-- find feeders of all destinations: used to find clock sources
	local feeder = {}
	for link in values(self.links) do
		feeder[link.dest:phys()] = link.src.obj
	end
	
	local function find_clk(obj,iface)
		if not iface then return nil end
		return feeder[obj .. "." .. iface]
	end
	
	-- find all exports in this system and the things they are exporting
	for link in values(self.links) do
		local ex = link.src
		local ex_o = self.objects[link.src.obj]
		local other = link.dest
		local other_o = self.objects[link.dest.obj]
		
		-- swap so that "other" is the exported thing
		if other_o.type == "EXPORT" then
			ex_o,other_o,ex,other = other_o,ex_o,other,ex
		end
		
		if ex_o.type == "EXPORT" and not visited[ex_o] then
			util.assert(other_o.type == "INSTANCE")
			Set.add(visited, ex_o)
			local other_if = self.parent.components[other_o.component].interfaces[other.iface]
			
			-- create an interface of the same type and direction as the export and exported interface
			-- it's named after the export, though
			local new_if = new_comp:add_interface(Interface:new
			{
				name = ex_o.name,
				type = other_if.type,
				dir = other_if.dir,
				clock = find_clk(other_o.name, other_if.clock)
			})
			
			-- create signals, whose net names are auto-generated based on exported object, exported interface
			-- name, and type/role
			for old_sig in values(other_if.signals) do
				local newsigname = ex_o.name
				if new_if.type == "CLOCK" or new_if.type == "RESET" then
					-- no additional modifications
				elseif old_sig.usertype then
					-- data signal with usertype OR a conduit
					newsigname = newsigname .. "_" .. old_sig.usertype
				else
					-- signal in a data interface with no usertype
					newsigname = newsigname .. "_" .. string.lower(old_sig.type)
				end
	
				-- resolve parameterized widths to a constant, using the exported instance's param bindings
				local width = ct.eval_expression(old_sig.width, other_o.param_bindings)
				
				new_if:add_signal
				{
					binding = newsigname,
					type = old_sig.type,
					usertype = old_sig.usertype,
					width = width
				}
			end				
		end
	end
end

function System:is_subsystem_of(other)
	for obj in values(other.objects) do
		if obj.type == "INSTANCE" and obj.component == self.name then return true end
	end
	return false
end

