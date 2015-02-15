require "util"

-- Create this package and set up environment

if not spec then spec = {} end
setmetatable(spec, {__index = _ENV})
_ENV = spec

-- Linkpoint

Linkpoint = class
{
	Types = enum {"broadcast"},
	
	name = nil,
	type = nil,
	encoding = nil,
	parent = nil
}

-- Signal

Signal = class
{
	Types = enum {"CLOCK", "RESET", "DATA", "LP_ID", "VALID", 
		"READY", "SOP", "EOP", "IN", "OUT"},
	
	type = nil,
	binding = nil,
	usertype = nil,
	width = nil
}

function Signal:validate()
    if not self.Types[string.lower(self.type)] then
        util.error('invalid signal type: ' .. self.type)
    end    
end

-- Interface

Interface = class
{
	Types = enum {"CLOCK", "RESET", "DATA", "CONDUIT"},
	Dirs = enum {"IN", "OUT"},
	
	name = nil,
	type = nil,
	dir = nil,
	signals = {},
	linkpoints = {},
	clock = nil,
	parent = nil,	-- set automatically
}

function Interface:validate()   
    if not self.Types[string.lower(self.type)] then
        util.error('invalid interface type: ' .. self.type)
    end
    
    if not self.Dirs[string.lower(self.dir)] then
        util.error('invalid interface direction: ' .. self.dir)
    end
end

function Interface:new(o)
	o = Interface:_init_inst(o)
	o.signals = {}
	o.linkpoints = {}
    
    -- create default linkpoint. gets deleted if real linkpoints are defined
    o:add_linkpoint(Linkpoint:new
    {
        name = 'lp',
        type = 'BROADCAST',
        encoding = ''
    })
    
	return o
end

function Interface:add_signal(sig)
    sig:validate()

	table.insert(self.signals, sig)
	return sig
end

function Interface:add_linkpoint(lp)
    -- delete default linkpoint
    self.linkpoints['lp'] = nil

	lp.parent = self
	util.insert_unique(lp, self.linkpoints)
	return lp
end

function Interface:rev_dir(d)
    if d == 'OUT' then return 'IN' 
    elseif d == 'IN' then return 'OUT'
    else util.error("Unknown direction: " .. d)
    end
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
    iface:validate()
	util.insert_unique(iface, self.interfaces)
	return iface
end

function Component:add_parameter(param)
	param.parent = self
	util.insert_unique(param, self.parameters)
end
		
