require "util"

-- Create this package and set up environment

if not spec then spec = {} end
setmetatable(spec, {__index = _ENV})
_ENV = spec

-- Linkpoint

Linkpoint = class
{
	Types = Set.make {"broadcast", "unicast"},
	
	name = nil,
	type = nil,
	encoding = nil,
	parent = nil
}

-- Signal

Signal = class
{
	Types = Set.make {"CLOCK", "RESET", "DATA", "LP_ID", "LINK_ID", "VALID", 
		"READY", "SOP", "EOP", "IN", "OUT"},
	
	type = nil,
	binding = nil,
	usertype = nil,
	width = nil
}

-- Interface

Interface = class
{
	Types = Set.make {"CLOCK", "RESET", "DATA", "CONDUIT"},
	Dirs = Set.make {"IN", "OUT", "CONDUIT"},
	
	name = nil,
	type = nil,
	dir = nil,
	signals = {},
	linkpoints = {},
	clock = nil,
	parent = nil,	-- set automatically
}

function Interface:new(o)
	o = Interface:_init_inst(o)
	o.signals = {}
	o.linkpoints = {}
	return o
end

function Interface:add_signal(sig)
	Set.add(self.signals, sig)
end

function Interface:add_linkpoint(lp)
	lp.parent = self
	util.insert_unique(lp, self.linkpoints)
end

-- Parameter

Parameter = class
{
	name = nil,
	parent = nil,
	width = nil,
	depth = 1
}

-- Component

Component = class
{
	name = nil,
	module = nil,
	interfaces = {},
	parameters = {}
}

function Component:new(o)
	o = Component:_init_inst(o)
	o.interfaces = {}
	o.parameters = {}
	return o
end

function Component:add_interface(iface)
	iface.parent = self
	util.insert_unique(iface, self.interfaces)
end

function Component:add_parameter(param)
	param.parent = self
	util.insert_unique(param, self.parameters)
end

function Component:create_default_linkpoints()
	for _,iface in pairs(self.interfaces) do
		local lps = iface.linkpoints
		
		if util.empty(lps) then
			local lp = Linkpoint:new
			{
				name = "lp", 
				type="BROADCAST",
				encoding=""
			}
			
			util.insert_unique(lp, lps)
		end
	end
end
		
