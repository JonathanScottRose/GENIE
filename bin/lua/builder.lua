--- Convenience class used to enable easy creation of systems.
-- This wraps the more detailed and complex @{genie} API.
-- @usage 
-- require 'builder'
-- local g = genie.Builder.new()
-- @classmod Builder

require "util"

genie.Builder = class()
local g = genie;
local Builder = genie.Builder

-- Private functions for looking up links by their labels
local function name_to_link(self, sys, name)
    local t = self.name2link[sys]
    local link = t[name]
    if not link then
        error("no link found with name " .. name .. " in system " .. sys, 2)
    end
    return link
end
    
local function names_to_links(self, sys, names)
    local result = {}
    for name in values(names) do
        local link = name_to_link(self, sys, name)
        table.insert(result, link)
    end
    return result
end

function Builder:__ctor()
    -- Holds string->Link mappings for each System
    self.name2link = {}
end       

--- Defines a new Component.
-- @tparam string name Component name
-- @tparam[opt] string modl name of Verilog module, defaults to `name`
-- @treturn genie.Node raw representation for advanced use
function Builder:component(name, modl)
	modl = modl or name
    self.cur_node = g.create_module(name, modl)
    self.cur_param_tgt = self.cur_node
    return self.cur_node
end

local function create_clockreset_port(self, name, type, dir, sig)
	sig = sig or name
	if not self.cur_node then error("Need a Module/System to create an Interface in", 2) end
	local fname = util.con_cat('create', type, 'port')
	self.cur_port = self.cur_node[fname](self.cur_node, name, dir, sig)
	return self.cur_port
end

--- Define a Clock Source Interface.
-- Also creates the clock signal binding within the Interface.
-- Calls @{Builder:interface} with type='`clock`' and dir='`out`' followed
-- by a call to @{Builder:signal}.
-- @function clock_src
-- @tparam string name name of Interface
-- @tparam[opt] string binding name of Verilog clock signal in module, defaults to `name`
-- @treturn genie.Port

--- Define a Clock Sink Interface.
-- Also creates the clock signal binding within the Interface.
-- Calls @{Builder:interface} with type='`clock`' and dir='`in`' followed
-- by a call to @{Builder:signal}.
-- @function clock_sink
-- @tparam string name name of Interface
-- @tparam[opt] string binding name of Verilog clock signal in module, defaults to `name`
-- @treturn genie.Port

--- Define a Reset Source Interface.
-- Also creates the reset signal binding within the Interface.
-- Calls @{Builder:interface} with type='`reset`' and dir='`out`' followed
-- by a call to @{Builder:signal}.
-- @function reset_src
-- @tparam string name name of Interface
-- @tparam[opt] string binding name of Verilog reset signal in module, defaults to `name`
-- @treturn genie.Port

--- Define a Reset Sink Interface.
-- Also creates the reset signal binding within the Interface.
-- Calls @{Builder:interface} with type='`reset`' and dir='`in`' followed
-- by a call to @{Builder:signal}.
-- @function reset_sink
-- @tparam string name name of Interface
-- @tparam[opt] string binding name of Verilog reset signal in module, defaults to `name`
-- @treturn genie.Port

--- Define a Conduit Source Interface.
-- Must connect to a Conduit Sink, but the direction of individual contained signals can still vary.
-- Calls @{Builder:interface} with type='`conduit`' and dir='`out`'.
-- @function conduit_src
-- @tparam string name name of Interface
-- @treturn genie.Port

--- Define a Conduit Sink Interface.
-- Must connect to a Conduit Source, but the direction of individual contained signals can still vary.
-- Calls @{Builder:interface} with type='`conduit`' and dir='`in`'.
-- @function conduit_sink
-- @tparam string name name of Interface
-- @treturn genie.Port

--- Define a Routed Streaming Source Interface.
-- Also associates a Clock Interface with it.
-- Calls @{Builder:interface} with type='`rs`' and dir='`out`', followed by a call to
-- @{Builder:assoc_clk} to associate a clock interface.
-- @function rs_src
-- @tparam string name name of Interface
-- @tparam string clockif name of associated Clock Interface
-- @treturn genie.Port

--- Define a Routed Streaming Sink Interface.
-- Also associates a Clock Interface with it.
-- Calls @{Builder:interface} with type='`rs`' and dir='`in`', followed by a call to
-- @{Builder:assoc_clk} to associate a clock interface.
-- @function rs_sink
-- @tparam string name name of Interface
-- @tparam string clockif name of associated Clock Interface
-- @treturn genie.Port

--- Define a Message Source
-- This is a special case of a Routed Streaming source interface that has only
-- a '`valid`' signal.
-- @function msg_src
-- @tparam string name name of Interface
-- @tparam string vname Verilog signal name
-- @tparam string clockif name of associated Clock Interface
-- @treturn genie.Port

--- Define a Message Sink
-- This is a special case of a Routed Streaming sink interface that has only
-- a '`valid`' signal.
-- @function msg_sink
-- @tparam string name name of Interface
-- @tparam string vname Verilog signal name
-- @tparam string clockif name of associated Clock Interface
-- @treturn genie.Port

-- programmatically define several special-case versions of :interface,
-- for each network type and direction
for _,i in pairs({{'src', 'out'}, {'sink', 'in'}}) do
    local suffix = i[1]
    local ifdir = i[2]
    
    Builder['clock_'..suffix] = function(self, name, binding)
		return create_clockreset_port(self, name, 'clock', ifdir, binding)
    end
    
    Builder['reset_'..suffix] = function(self, name, binding)
		return create_clockreset_port(self, name, 'reset', ifdir, binding)
    end
    
    Builder['conduit_'..suffix] = function(self, name)
		if not self.cur_node then error("Need a Module/System to create an Interface in") end
		self.cur_port = self.cur_node:create_conduit_port(name, ifdir)
        return self.cur_port
    end
    
    Builder['rs_'..suffix] = function(self, name, clockif)
        if not self.cur_node then error("Need a Module/System to create an Interface in") end
		self.cur_port = self.cur_node:create_rs_port(name, ifdir, clockif)
		return self.cur_port
    end
	
	Builder['msg_'..suffix] = function(self, name, vname, clockif)
		local result = self['rs_'..suffix](self, name, clockif)
		result:signal('valid', vname)
		return result
	end
end

--- Creates a System.
-- @tparam string name name of System
-- @treturn genie.System raw representation for advanced use
function Builder:system(name)
	self.cur_sys = g.create_system(name)
    self.cur_node = self.cur_sys
	self.cur_param_tgt = self.cur_sys
    
    -- initialize name2link table for this system
    self.name2link[self.cur_sys] = {}
	return self.cur_sys
end

--- Instantiates a Component.
-- Instantiates within the current System, created by the most recent call to @{Builder:system}.
-- @tparam string comp name of previously-defined Component
-- @tparam string name name of instance
-- @treturn genie.Node raw representation for advanced use
function Builder:instance(comp, name)
	if not self.cur_sys then error("Unexpected 'instance'") end
    self.cur_param_tgt = self.cur_sys:create_instance(comp, name)
    return self.cur_param_tgt
end

--- Exports an Interface of a Component instance.
-- Within the current System, this creates a new top-level Interface with the same
-- type and complementary direction, and automatically creates a Link between them.
-- @tparam string|Port iface hierarchical path, or reference to, Interface to export, relative to system
-- @tparam[opt] string name name of new exported Interface that will be generated, default autogenerated
-- @treturn genie.Port raw representation for advanced use
function Builder:export(iface, name)
	if not self.cur_sys then error("Unexpected 'export'") end
    self.cur_port = self.cur_sys:export_port(iface, name)
	return self.cur_port
end

--- Defines a Link between two Interfaces.
-- The source and sink need to be of the same network type.
-- For Routed Streaming Interfaces, the source/sink can be a Linkpoint.
-- 
-- @tparam string|Port src hierarchical path, or object reference, to source Interface/Linkpoint
-- @tparam string|Port dest hierarchical path, or object reference, to sink Interface/Linkpoint
-- @tparam[opt] string label unique label for Link
-- @treturn genie.Link raw representation for advanced use
function Builder:link(src, dest, label)
	if not self.cur_sys then error("Unexpected 'link'") end
    local link = self.cur_sys:add_link(src, dest)    
    local n2l = self.name2link[self.cur_sys]
    if label then 
        if n2l[label] then
            error('link with label ' .. label .. ' already exists ' .. 
                ' in system ' .. self.cur_sys)
        end
        n2l[label] = link 
    end
    return link
end

for _,ptype in ipairs({'clock', 'reset', 'conduit', 'rs'}) do
	Builder[ptype..'_link'] = function(self, src, sink)
		if not self.cur_sys then error("Unexpected 'link'") end
		local fname = 'create_'..ptype..'_link'
		return self.cur_sys[fname](self.cur_sys, src, sink)
	end
end


--- Defines an internal RS link between two RS Interfaces within a Component.
-- The internal link is between a sink port (that receives data from outside the Component),
-- that feeds a a source port (that sends data out of the Component)). An optional parameter
-- specifies the latency in cycles (only makes sense if sink/src are on the same clock domain).
-- @tparam string|Port sink hierarchical path, or object reference, to sink RS Interface
-- @tparam string|Port src hierarchical path, or object reference, to source RS Interface
-- @tparam[opt] number latency in cycles
function Builder:internal_link(sink, src, latency)
	if not self.cur_node then error("Unexpected internal_link, no current component/system") end
	self.cur_node:add_internal_link(sink, src, latency)
end

--- Associates a Clock Interface with the current RS Interface.
-- Applies to most recently-defined RS Interface.
-- @tparam string clkif name of Clock Interface
function Builder:assoc_clk(clkif)
    if not self.cur_port then error("no current interface") end
    self.cur_port:set_clock_port_name(clkif)
end

--- Marks a set of RS Links as temporally exclusive.
-- This is a guarantee by the designer that none of the RS Links in this set
-- will ever be used simultaneously.
-- @tparam array(string)|array(Link) s an array of RS Links (@{genie.Link}) or RS Link labels
function Builder:make_exclusive(s)
    if not self.cur_sys then error("no current system") end

    -- ignore null args
    if not s then return end;
    
    -- ensure a table, and ignore empty table
    if type(s) ~= 'table' then 
        error('expected array/set of labels or RS link objects')
    end
    
    if not next(s) then return end
    
    -- based on the first entry in the table, guess whether or not we have
    -- been given a list of actual objects, or a list of string labels
    local k0,v0 = next(s)
    if type(k0) == 'string' or type(v0) == 'string' then
         s = names_to_links(self, self.cur_sys, s)
    elseif type(k0) ~= 'userdata' and type(v0) ~= 'userdata' then
        error('expected array/set of labels or RS link objects')
    end
    
    -- s is now for sure a set of link objects
    g.make_exclusive(s)    
end

--- Creates or defines a parameter.
-- Applies to the most recently defined Component or System.
-- Parameters of Components can reference parameters of their parent System.
-- Creating a parameter without a value allows it to be set later.
-- Creating a parameter with a value on a System generates it as a Verilog `localparam`
-- and it can not be overridden later.
-- @tparam string name name
-- @tparam number|string val value or expression
function Builder:int_param(name, val)
    if not self.cur_param_tgt then error("no current component or system") end
    self.cur_param_tgt:set_int_param(name, val)
end

function Builder:str_param(name, val)
    if not self.cur_param_tgt then error("no current component or system") end
    self.cur_param_tgt:set_str_param(name, val)
end

function Builder:lit_param(name, val)
    if not self.cur_param_tgt then error("no current component or system") end
    self.cur_param_tgt:set_lit_param(name, val)
end

--- Registers a Verilog signal with an Interface.
-- Applies to the Interface created by the most recent call to @{Builder:interface} or its specialized
-- convenience functions. The `role` and `vname` arguments are mandatory. Some types of signals also require
-- a tag. If width is not specified, it defaults to 1.
--
-- @tparam string role Signal role. Allowable values depend on the type of Interface.
-- @tparam[opt] string tag Used-defined tag to differentiate similar signal roles.
-- @tparam string vname Verilog name of signal in module to bind to.
-- @tparam[opt=1] expression width width in bits of the signal, can be an expression, mandatory only if `tag` specified
function Builder:signal(...)
    if not self.cur_port then error("no current port") end
    
	local args = table.pack(...)
    local newargs
    
	if args.n == 2 then newargs = {args[1], nil, args[2]} 
	elseif args.n == 3 then newargs = {args[1], nil, args[3], args[4]} 
	elseif args.n == 4 then newargs = args 
	else error('Expected 2, 3, or 4 parameters') 
	end
    
    self.cur_port:add_signal(table.unpack(newargs))
end

--- Creates a latency query.
-- Creates a new system-level parameter of the given name, whose value is set to the latency of the given RS link.
-- The link can be specified by string label or by a reference to a Link object.
-- @tparam string|Link link the Routed Streaming link to query, specified by string link label or directly by object
-- @tparam string param name of the system-level parameter to create and populate
function Builder:latency_query(link, param)
    if not self.cur_sys then error("no current system") end
    
    -- function overload: link may be a label, or an actual link.
    -- convert to an actual link if it's just a label
    if type(link) == 'string' then
        link = name_to_link(self, self.cur_sys, link)
    elseif type(link) ~= 'userdata' then
        error('expected a link object or a link label')
    end
    
    self.cur_sys:create_latency_query(link, param)
end

--- Creates a latency constraint.
-- Constrains the final latency, in cycles, of one or more Paths. A Path is a sequence of one or more RS Links, 
-- starting at an ultimate RS Source and ending at an ultimate RS Sink. Intermediate Components that serve as a 
-- terminus for one Link and the source of the next Link in the Path must have an internal Link defined between
-- the sink and source Interfaces to make the Path contiguous.
--
-- The constraint takes on the form: `p1 [+/-p2, +/-p3, ...] OP rhs` where rhs is an integer, OP is a comparison operator
-- (<, <=, =, >= >), and `pn` are paths, represented as arrays of RS Links. Terms p2 and up are optional.
-- @tparam array(RSLink) p1 first path term
-- @tparam[opt] string pn_sign sign of successive path term, either `+` or `-`
-- @tparam[opt] array(RSLink) pn successive path term
-- @tparam string op comparison operator
-- @tparam int rhs comparison constant
function Builder:latency_constraint(...)
	if not self.cur_sys then error("no current system") end
	self.cur_sys:create_latency_constraint(...)
end

--- Sets default packet size of an RS source/sink.
-- Sets the default packet size of connected RS links, in clock cycles. If unspecified, the default is 1.
-- @tparam int size transmission size in clock cycles
function Builder:packet_size(size)
	if not self.cur_port then error("no current port") end
	if self.cur_port:get_type() ~= 'rs' then error("need an RS port") end
	
	self.cur_port:set_pktsize(size)
end

--- Sets default importance of an RS source/sink.
-- Sets the default importance of connected RS links from 0-1. If unspecified, the default is 1.
-- @tparam int imp importance from 0-1
function Builder:importance(importance)
	if not self.cur_port then error("no current port") end
	if self.cur_port:get_type() ~= 'rs' then error("need an RS port") end
	
	self.cur_port:set_importance(importance)
end
