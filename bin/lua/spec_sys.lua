require "util"
require "spec_comp"

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
    if not self.parent.parent.components[self.component].parameters[param] then 
        util.error("unknown parameter "..param)
    end
    
	self.param_bindings[param] = value
end

function Instance:validate()
    if not self.parent.parent.components[self.component] then
        util.error(self.name .. ' instantiates unknown component ' .. self.component)
    end
end

-- Export

Export = SysObject:subclass
{
	type = 'EXPORT',
	interface = nil
}

function Export:new(o)
    o = self:_init_inst(o)
    
    -- make sure the export is already named, so we can name the interface
    if not o.name then util.error("must provide name for export") end
    
    -- create internal interface, or co-opt the passed-in one
    o.interface = o.interface or {}
    o.interface.name = o.name
    o.interface = Interface:new(o.interface)
        
    return o
end

function Export:validate()
    self.interface:validate()
end

-- LinkTarget

LinkTarget = class
{
}

function LinkTarget:new(o)
    o = self:_init_inst(o)
    
    o.iface_isdefault = o.iface == nil
    o.lp_isdefault = o.lp == nil
    
    o.iface = o.iface or 'iface'
    o.lp = o.lp or 'lp'
    
    return o
end

function LinkTarget:parse(str)
	m = {}
	for val in string.gmatch(str, "[^.]+") do
		table.insert(m, val)
	end
	
    self.obj = m[1] or self.obj
    
    if m[2] then
        self.iface = m[2]
        self.iface_isdefault = false
    end
	
    if m[3] then
        self.lp = m[3]
        self.lp_isdefault = false
    end
end

function LinkTarget:create_from_parse(str)
    local result = LinkTarget:new()
    result:parse(str)
    return result
end

function LinkTarget:str()
	return self.obj .. "." .. self.iface .. "." .. self.lp
end

function LinkTarget:str_pretty()
    util.print(self)
    local result = self.obj
    if not self.iface_isdefault then
        result = result .. '.' .. self.iface
        if not self.lp_isdefault then
            result = result .. '.' .. self.lp
        end
    end
    return result
end

function LinkTarget:phys()
	return self.obj .. "." .. self.iface
end
	

-- Link

Link = class
{
	src = nil,
	dest = nil,
    label = nil
}

function Link:new(o)
	o = Link:_init_inst(o)
	o.src = LinkTarget:new()
	o.dest = LinkTarget:new()
	return o
end

-- System

System = class
{
	name = nil,
	parent = nil,
	links = {},
	objects = {},
    parameters = {},
	topo_func = nil,
    excl_groups = {},
    latency_queries = {}
}

function System:new(o)
	o = System:_init_inst(o)
	o.links = {}
	o.objects = {}
    o.parameters = {}
    o.excl_groups = {}
    o.latency_queries = {}
	return o
end

function System:add_link(link)
    for targ in values{link.src, link.dest} do
        local obj = self.objects[targ.obj]
        local good = obj ~= nil
        
        if good and obj.type == 'INSTANCE' then
            local comp = self.parent.components[obj.component]
            local iface = comp.interfaces[targ.iface]
            good = iface ~= nil
            good = good and iface.linkpoints[targ.lp] ~= nil
        end
        
        if not good then
            util.error('invalid link src or dest: ' .. targ:str_pretty())
        end
    end
	table.insert(self.links, link)
end

function System:add_object(obj)
	obj.parent = self
    obj:validate()
	util.insert_unique(obj, self.objects)
    return obj
end

function System:add_parameter(param)
	param.parent = self
	util.insert_unique(param, self.parameters)
end

function System:add_exclusion_group(grp)
    table.insert(self.excl_groups, grp)
end

function System:add_latency_query(label, param)
    table.insert(self.latency_queries,
        {
            label = label,
            param = param
        }
    )
end

function System:export_iface(exname, target)
    -- extract target's object name and interface name
    local targ_lt = LinkTarget:create_from_parse(target)
    local instname = targ_lt.obj
    local ifname = targ_lt.iface
    
    -- get target object and interface
    local inst = self.objects[instname]
    local iface = self.parent.components[inst.component].interfaces[ifname]
    
    -- create the export
    -- defer copying of clock and signals until system post-processing time
    local ex = Export:new
    {
        name = exname,
        interface =
        {
            type = iface.type, 
            dir = Interface:rev_dir(iface.dir)
        }
    }
    
    -- register export in system
    self:add_object(ex)
    
    -- create linktarget that points to the export
    local ex_lt = LinkTarget:new
    {
        obj = exname
    }
    
    -- copy linkpoints and create links from exported interface to export
    for old_lp in values(iface.linkpoints) do
        local new_lp = Linkpoint:new
        {
            name = old_lp.name,
            type = old_lp.type,
            encoding = old_lp.encoding
        }
        ex.interface:add_linkpoint(new_lp)
        
        -- set up linktarget that points to exported interface
        targ_lt.lp = old_lp.name
        
        -- set up linktarget that points to the export
        ex_lt.lp = new_lp.name
        
        -- create link and determine correct polarity
        local link = Link:new()
        if iface.dir == 'OUT' then
            link.src = targ_lt
            link.dest = ex_lt
        else
            link.src = ex_lt
            link.dest = targ_lt
        end
        
        -- register link in system
        self:add_link(link)
    end
end
	
function System:create_auto_exports()
    -- iterate over all links and mark each link's PHYSICAL endpoint as being connected.
    -- we use this to find out which things are UNconnected later
    local is_connected = {}
	
	for link in values(self.links) do
		is_connected[link.src:phys()] = true
		is_connected[link.dest:phys()] = true
	end
	
    -- gather instances from system
    local instances = {}
    for obj in values(self.objects) do
        if obj.type == 'INSTANCE' then Set.add(instances, obj) end
    end
	
    -- iterate over all unconnected instance.interface and export them
	for inst in Set.values(instances) do
		local comp = self.parent.components[inst.component]
		for iface in values(comp.interfaces) do		
            local test_lt = LinkTarget:new
            {
                obj = inst.name,
                iface = iface.name
            }
            
            if not is_connected[test_lt:phys()] then
                -- automatically create a name for the export
                local exname = util.con_cat(inst.name, iface.name)
                self:export_iface(exname, test_lt:phys())
            end
		end
	end
end

function System:finalize_exports()
    -- for every clock sink, find the source export
    local clk_feeder = {}
    for link in values(self.links) do
        local ex = self.objects[link.src.obj]
        if ex.type == 'EXPORT' and ex.interface.type == 'CLOCK' and ex.interface.dir == 'OUT' then
            clk_feeder[link.dest:phys()] = ex
        end
    end
    
    for link in values(self.links) do
        local inst = self.objects[link.src.obj]
        local ex = self.objects[link.dest.obj]
        local ifname = link.src.iface
        
        if inst.type == 'EXPORT' then
            inst,ex = ex,inst
            ifname = link.dest.iface
        end
        
        if inst.type == 'INSTANCE' and ex.type == 'EXPORT' then
            local iface = self.parent.components[inst.component].interfaces[ifname]
            
            -- copy clock domain of exported thing
            if iface.clock and not ex.interface.clock then
                -- create a path to the clock sink interface
                local clk_sink_targ = LinkTarget:new
                {
                    obj = inst.name,
                    iface = iface.clock
                }
                
                -- look up the export feeding this interface
                local clk_src_export = clk_feeder[clk_sink_targ:phys()]
                
                -- make the name of that export the clock feeder of the data export
                ex.interface.clock = clk_src_export.name
            end
            
            -- copy signals of exported thing
            for old_sig in values(iface.signals) do
                -- nasty. find if this type of signal already has a definition in the export
                local found = false
                for ex_sig in values(ex.interface.signals) do
                    if ex_sig.type == old_sig.type and ex_sig.usertype == old_sig.usertype then
                        found = true
                        break
                    end
                end
                
                -- signal in interface but not in export: copy it
                if not found then
                    -- auto-generate a verilog signal name
                    local binding = util.con_cat(ex.name, string.lower(old_sig.usertype or old_sig.type))
                    if (iface.type == 'CLOCK' or iface.type == 'RESET') then
                        binding = ex.name
                    end
                    
                    -- if the width of the exported thing was parameterized, resolve
                    -- the parameter expression to a concrete value using the instance's parameter
                    -- bindings.
                    local width = genie.eval_expression(old_sig.width, inst.param_bindings)
                    
                    ex.interface:add_signal(Signal:new
                    {
                        type = old_sig.type,
                        usertype = old_sig.usertype,
                        width = width,
                        binding = binding
                    })
                end  
            end
        end
    end
end

function System:create_default_reset()
	-- check if system has a reset export already
	for obj in values(self.objects) do
		if obj.type == 'EXPORT' and obj.interface.type == "RESET" then return end
	end
	
	-- create a reset export, connected to nothing
    -- note the direction is 'OUT' since, from the viewpoint of inside the system,
    
	local ex = self:add_object(Export:new
	{
		name = "reset",
        interface = {type = 'RESET', dir = 'OUT'}
	})
    
    -- must manually create reset signal, since finalize_exports will not process this export due to
    -- it not being connected to anything.
    ex.interface:add_signal(Signal:new
    {
        type = 'RESET',
        binding = 'reset',
        width = '1'
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
    
    -- copy parameter definitions from System to Component
    for k,v in pairs(self.parameters) do
        new_comp.parameters[k] = v
    end
	
    -- gather all exports of the system and add them as interfaces of the new component
    for ex in values(self.objects) do
        if ex.type == 'EXPORT' then
            -- clone the interface attached to the export, except for a change in direction
            local iface = Interface:new
            {
                name = ex.interface.name,
                type = ex.interface.type,
                dir = Interface:rev_dir(ex.interface.dir),
                clock = ex.interface.clock
            }
            
            -- do an alias of the linkpoints and signals tables since they require no modification and
            -- won't be modified later in the flow
            iface.linkpoints = ex.interface.linkpoints
            iface.signals = ex.interface.signals
            
            new_comp:add_interface(iface)
        end
    end
end

function System:is_subsystem_of(other)
	for obj in values(other.objects) do
		if obj.type == "INSTANCE" and obj.component == self.name then return true end
	end
	return false
end

