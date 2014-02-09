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
	objects = {}
}

function System:new(o)
	o = System:_init_inst(o)
	o.links = {}
	o.objects = {}
	return o
end

function System:add_link(link)
	Set.add(self.links, link)
end

function System:add_object(obj)
	obj.parent = self
	util.insert_unique(obj, self.objects)
end
	
function System:create_auto_exports()
	local is_connected = {}
	
	for link in keys(self.links) do
		is_connected[link.src:str()] = true
		is_connected[link.dest:str()] = true
	end
	
	for _,obj in pairs(self.objects) do
		if obj.type ~= 'INSTANCE' then goto nextobj end
		
		local src_lt = LinkTarget:new()
		src_lt.obj = obj.name
		
		local comp = self.parent.components[obj.component]
		for _,iface in pairs(comp.interfaces) do
			src_lt.iface = iface.name
			
			local is_out = iface.dir == 'OUT'
			
			for _,lp in pairs(iface.linkpoints) do
				src_lt.lp = lp.name
				
				if not is_connected[src_lt:str()] then
					local exname = obj.name .. "_" .. iface.name
					if util.count(iface.linkpoints) > 1 then
						exname = exname .. "_" .. lp.name
					end
					
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
					self:add_object(new_export)
				end
			end
		end
		
		::nextobj::
	end
end

		
