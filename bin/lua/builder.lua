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
        error("no link found with name " .. name .. " in system " .. sys)
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
-- @tparam string modl name of Verilog module
-- @treturn genie.Node raw representation for advanced use
function Builder:component(name, modl)
    self.cur_node = g.Node.new(name, modl)
    self.cur_param_tgt = self.cur_node
    return self.cur_node
end

--- Defines a new Interface.
-- Applies to the current Component or System, which is defined by the most recent call to
-- @{Builder:component} or @{Builder:system}.
-- @tparam string name name of Interface
-- @tparam string type one of `clock`, `reset`, `conduit`, or `rs`
-- @tparam string dir one of `in` or `out`
-- @treturn genie.Port raw representation for advanced use
function Builder:interface(name, type, dir)
	if not self.cur_node then error("Unexpected 'interface'") end
    self.cur_port = self.cur_node:add_port(name, type, dir)
    return self.cur_port
end


--- Define a Clock Source Interface.
-- Also creates the clock signal binding within the Interface.
-- Calls @{Builder:interface} with type='`clock`' and dir='`out`' followed
-- by a call to @{Builder:signal}.
-- @function clock_src
-- @tparam string name name of Interface
-- @tparam string binding name of Verilog clock signal in module
-- @treturn genie.Port

--- Define a Clock Sink Interface.
-- Also creates the clock signal binding within the Interface.
-- Calls @{Builder:interface} with type='`clock`' and dir='`in`' followed
-- by a call to @{Builder:signal}.
-- @function clock_sink
-- @tparam string name name of Interface
-- @tparam string binding name of Verilog clock signal in module
-- @treturn genie.Port

--- Define a Reset Source Interface.
-- Also creates the reset signal binding within the Interface.
-- Calls @{Builder:interface} with type='`reset`' and dir='`out`' followed
-- by a call to @{Builder:signal}.
-- @function reset_src
-- @tparam string name name of Interface
-- @tparam string binding name of Verilog reset signal in module
-- @treturn genie.Port

--- Define a Reset Sink Interface.
-- Also creates the reset signal binding within the Interface.
-- Calls @{Builder:interface} with type='`reset`' and dir='`in`' followed
-- by a call to @{Builder:signal}.
-- @function reset_sink
-- @tparam string name name of Interface
-- @tparam string binding name of Verilog reset signal in module
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
        local result = self:interface(name, 'clock', ifdir)
        self:signal('clock', binding, 1)
        return result
    end
    
    Builder['reset_'..suffix] = function(self, name, binding)
        local result = self:interface(name, 'reset', ifdir)
        self:signal('reset', binding, 1)
        return result
    end
    
    Builder['conduit_'..suffix] = function(self, name)
        return self:interface(name, 'conduit', ifdir)
    end
    
    Builder['rs_'..suffix] = function(self, name, clockif)
        local result = self:interface(name, 'rs', ifdir)
        result:set_clock_port_name(clockif)
        return result
    end
	
	Builder['msg_'..suffix] = function(self, name, vname, clockif)
		local result = self['rs_'..suffix](self, name, clockif)
		self:signal('valid', vname)
		return result
	end
end

--- Creates a Linkpoint.
-- Applies to the current Interface, defined by the most recent call to @{Builder:interface} or its 
-- specialized convenience functions.
-- @tparam string name name of Linkpoint
-- @tparam string lpid Verilog encoding of Linkpoint ID, eg `2'b00`
-- @treturn genie.RSLinkpoint raw representation for advanced use
function Builder:linkpoint(name, lpid)
	if not self.cur_port then error("Unexpected 'linkpoint'") end
    if not self.cur_port.add_linkpoint then error("Interface does not support linkpoints") end
    
    return self.cur_port:add_linkpoint(name, lpid, 'broadcast')
end

--- Creates a System.
-- Can provide a GENIE-supplied topology function, or a custom one.
-- @tparam string name name of System
-- @tparam function topofunc topology function
-- @treturn genie.Node raw representation for advanced use
function Builder:system(name, topofunc)
	self.cur_sys = g.System.new(name, topofunc)
    self.cur_node = self.cur_sys
	self.cur_param_tgt = self.cur_sys
    
    -- initialize name2link table for this system
    self.name2link[self.cur_sys] = {}
	return self.cur_sys
end

--- Instantiates a Component.
-- Instantiates within the current System, created by the most recent call to @{Builder:system}.
-- @tparam string name name of instance
-- @tparam string comp name of previously-defined Component
-- @treturn genie.Node raw representation for advanced use
function Builder:instance(name, comp)
	if not self.cur_sys then error("Unexpected 'instance'") end
    self.cur_param_tgt = self.cur_sys:add_node(name, comp)
    return self.cur_param_tgt
end

--- Exports an Interface of a Component instance.
-- Within the current System, this creates a new top-level Interface with the same
-- type and complementary direction, and automatically creates a Link between them.
-- @tparam string|Port iface hierarchical path, or reference to, Interface to export
-- @tparam string name name of new exported Interface that will be generated
-- @treturn genie.Port raw representation for advanced use
function Builder:export(iface, name)
	if not self.cur_sys then error("Unexpected 'export'") end
    self.cur_port = self.cur_sys:make_export(iface, name)
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
function Builder:parameter(name, val)
    if not self.cur_param_tgt then error("no current component or system") end
    self.cur_param_tgt:def_param(name, val)
end

--- Registers a Verilog signal with an Interface.
-- Applies to the Interface created by the most recent call to @{Builder:interface} or its specialized
-- convenience functions. The `role` and `vname` arguments are mandatory. Some types of signals also require
-- a tag. If width is not specified, it defaults to 1.
--
-- It is also possible to refer to _part_ of a Verilog signal. This is done by supplying a three-element array 
-- for the width argument, in the form `{totalwidth, width, lsb}`. `totalwidth` is the size of the entire
-- Verilog signal. Bits `[lsb+width-1 : lsb]` will be selected.
-- @tparam string role Signal role. Allowable values depend on the type of Interface.
-- @tparam[opt] string tag Used-defined tag to differentiate similar signal roles.
-- @tparam string vname Verilog name of signal in module to bind to.
-- @tparam[opt=1] string|number|array width width of signal, can be an expression, or a partial binding as described above
function Builder:signal(role, tag, vname, width)
    if not self.cur_port then error("no current port") end
    
    local newargs
    
    if not vname then newargs = {role, tag, 1}
    elseif not width then newargs = {role, tag, vname}
    else newargs = {role, tag, vname, width}
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
-- The constraint takes on the form: `lb <= p1 [+/-p2, +/-p3, ...] <= ub` where `lb` and `ub` are the upper/lower 
-- latency bounds and are integers, and `pn` are paths, represented as arrays of RS Links. Terms p2 and up are optional.
-- @tparam array(RSLink) p1 first path term
-- @tparam[opt] string pn_sign sign of successive path term, either `+` or `-`
-- @tparam[opt] array(RSLink) pn successive path term
-- @tparam int lb lower bound
-- @tparam int ub upper bound
function Builder:latency_constraint(...)
	if not self.cur_sys then error("no current system") end
	self.cur_sys:create_latency_constraint(...)
end

