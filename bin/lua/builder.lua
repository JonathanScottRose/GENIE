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

function Builder:__ctor()
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

--- Creates a link between two clock interfaces.
-- @function clock_link
-- @tparam string|Port src clock port object or hierarhical path relative to system
-- @tparam string|Port sink clock port object or hierarhical path relative to system
-- @treturn Link

--- Creates a link between two reset interfaces.
-- @function reset_link
-- @tparam string|Port src clock port object or hierarhical path relative to system
-- @tparam string|Port sink clock port object or hierarhical path relative to system
-- @treturn Link

--- Creates a link between two conduit interfaces.
-- @function conduit_link
-- @tparam string|Port src clock port object or hierarhical path relative to system
-- @tparam string|Port sink clock port object or hierarhical path relative to system
-- @treturn Link

--- Creates a logical link between two RS interfaces.
-- Use this to define logical end-to-end transmissions between Component instances.
-- @function rs_link
-- @tparam string|Port src clock port object or hierarhical path relative to system
-- @tparam string|Port sink clock port object or hierarhical path relative to system
-- @tparam[opt=nil] unsigned src_addr source address binding, optional
-- @tparam[opt=nil] unsigned sink_addr sink address binding, optional
-- @treturn LinkRS

--- Creates a topological link between two RS interfaces.
-- Use this to manually create physical links between RS Interfaces and Split or Merge nodes.
-- @function topo_link
-- @tparam string|HierObject src object reference, or string path to, an RS interface, Split node, or Merge node
-- @tparam string|HierObject sink 
-- @treturn LinkTopo

for _,ptype in ipairs({'clock', 'reset', 'conduit', 'rs', 'topo'}) do
	Builder[ptype..'_link'] = function(self, src, sink, ...)
		if not self.cur_sys then error("Unexpected 'link'") end
		local fname = 'create_'..ptype..'_link'
		return self.cur_sys[fname](self.cur_sys, src, sink, ...)
	end
end

--- Creates an internal link between two RS ports within the same component.
-- Defines a fixed-latency path with the given latency, travelling from `sink` to `src`.
-- @tparam string|Port sink hierarchical path, or object reference, to sink RS Interface
-- @tparam string|Port src hierarchical path, or object reference, to source RS Interface
-- @tparam[opt] number latency in cycles
function Builder:internal_link(sink, src, latency)
	if not self.cur_node then error("Unexpected internal_link, no current component/system") end
	self.cur_node:create_internal_link(sink, src, latency)
end

--- Manually create a split node within the current system.
-- @tparam[opt] string name optional explicit name for split node
-- @treturn Node
function Builder:split(name)
	if not self.cur_sys then error("Unexpected 'link'") end
	local result =  self.cur_sys:create_split(name)
	self.cur_node = result
	self.cur_param_tgt = result
	return result
end

--- Manually create a merge node within the current system.
-- @tparam[opt] string name optional explicit name for merge node
-- @treturn Node
function Builder:merge(name)
	if not self.cur_sys then error("Unexpected 'link'") end
	local result = self.cur_sys:create_merge(name)
	self.cur_node = result
	self.cur_param_tgt = result
	return result
end

--- Defines an integer parameter.
-- Applies to the most recently defined Component, System, or Instance.
-- The expression must evaluate to an integer and can reference other parameters within
-- the same scope, or that of the parent System.
-- 
-- @tparam string name name
-- @tparam string val value
function Builder:int_param(name, val)
    if not self.cur_param_tgt then error("no current component or system") end
    self.cur_param_tgt:set_int_param(name, val)
end

--- Defines a string parameter.
-- Applies to the most recently defined Component, System, or Instance.
-- 
-- @tparam string name name
-- @tparam string val value
function Builder:str_param(name, val)
    if not self.cur_param_tgt then error("no current component or system") end
    self.cur_param_tgt:set_str_param(name, val)
end

--- Defines a literal parameter.
-- Applies to the most recently defined Component, System, or Instance.
-- The given expression is passed, verbatim, to the Verilog instantiation of the module.
-- 
-- @tparam string name name
-- @tparam string val value
function Builder:lit_param(name, val)
    if not self.cur_param_tgt then error("no current component or system") end
    self.cur_param_tgt:set_lit_param(name, val)
end

--- Defines a system parameter.
-- Applies to the most recently defined System.
-- Creates a parameter without a value, that is to be set by an insantiator of the
-- system, outside the scope of GENIE.
-- 
-- @tparam string name name
function Builder:sys_param(name)
    if not self.cur_sys then error("no current system") end
    self.cur_sys:create_sys_param(name)
end

--- Registers an HDL signal with an Interface.
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
    self.cur_port:add_signal(...)
end

--- Registers an HDL signal with an Interface.
-- An advanced version of add_signal to more finely control
-- the size of the HDL port and the size of the binding to the HDL port.
-- Expects two structures: a PortSpec and a BindSpec.
-- The PortSpec structure shall be a table with the following fields:
--   name: the name of the HDL port
--   width: an expression specifying the width in bits of the HDL port
--   depth: an expression specifying the second size dimension of the HDL port
-- Both width and depth are expressions that must evaluate to an integer and may contain
-- references to @{Node} parameters. If either field is absent, it is defaulted to '1'.
-- The BindSpec structure specifies the range of the HDL Port to bind to for this role.
-- It shall be a table with the following fields:
--   lo_slice: the lower bound of the second dimension of the HDL port to bind to, default 0
--   slices: the number of second-dimension indicies to bind over, default 1
--   lo_bit: the least-significant bit to bind to, must be 0 if slices is > 1, default 0
--   width: the number of bits, starting from lo_bit, to bind, must be the entire port width
--          if slices > 1
-- @function add_signal_ex
-- @tparam string role port type specific role
-- @tparam string tag unique user-defined tag needed for some roles
-- @tparam table port_spec a PortSpec structure, as defined above
-- @tparam table bind_spec a BindSpec structure, as defined above
function Builder:signal_ex(...)
	if not self.cur_port then error("no current port") end
	self.cur_port:add_signal_ex(...)
end

--- Marks a set of RS Links as temporally exclusive.
-- This is a guarantee by the designer that none of the RS Links in this set
-- will ever be used simultaneously.
-- @tparam array(array(LinkRS) s an array of RS Links (@{genie.LinkRS})
function Builder:make_exclusive(s)
    if not self.cur_sys then error("no current system") end

    -- ignore nil arg
    if not s then return end;
    
    -- s is now for sure a set of link objects
    self.cur_sys:make_exclusive(s)    
end

--- Sets the combinational logic depth of an RS Port.
--
-- This tells GENIE that there are `depth` levels of logic (LUTs) between
-- the port itself and the interior of the Component.
-- @tparam integer depth logic depth, default 0
function Builder:logic_depth(depth)
	if not self.cur_port then error("no current port") end
	self.cur_port:set_logic_depth(depth)
end

--- Sets the maximum logic depth for the System
--
-- This overrides the global setting in the GENIE options.
-- @tparam integer depth
function Builder:max_logic_depth(depth)
	if not self.sys then error("no current system") end
	self.cur_sys:set_max_logic_depth(depth)
end

--- Sets default packet size of an RS source/sink.
-- Sets the default packet size of connected RS links, in clock cycles. If unspecified, the default is 1.
-- @tparam int size transmission size in clock cycles
function Builder:packet_size(size)
	if not self.cur_port then error("no current port") end
	self.cur_port:set_default_packet_size(size)
end

--- Sets default importance of transmissions starting at an RS source port.
-- Sets the default importance of connected RS links from 0-1. If unspecified, the default is 1.
-- @tparam int imp importance from 0-1
function Builder:importance(importance)
	if not self.cur_port then error("no current port") end
	self.cur_port:set_default_importance(importance)
end

--- Creates a synchronization constraint.
-- Constrains the final latency, in cycles, of one or more Chains. A Chain is a sequence of one or more RS Links, 
-- starting at an ultimate RS Source and ending at an ultimate RS Sink. Intermediate Components that serve as a 
-- terminus for one Link and the source of the next Link in the Chain must have an internal Link defined between
-- the sink and source Interfaces to make the Chain contiguous.
--
-- The constraint takes on the form: `p1 [+/-p2, +/-p3, ...] OP rhs` where rhs is an integer, OP is a comparison operator
-- (<, <=, =, >= >), and `pn` are paths, represented as arrays of RS Links. Terms p2 and up are optional.
-- @tparam array(RSLink) p1 first chain term
-- @tparam[opt] string pn_sign sign of successive chain term, either `+` or `-`
-- @tparam[opt] array(RSLink) pn successive chain term
-- @tparam string op comparison operator: `<`, `<=`, `=`, `>=`, `>`
-- @tparam int rhs comparison constant
function Builder:sync_constraint(...)
	if not self.cur_sys then error("no current system") end
	self.cur_sys:create_sync_constraint(...)
end


--- Creates a latency query.
--
-- Once the system is generated, this will measure the end-to-end latency in clock cycles
-- through the generated interconnect and any intervening user modules, and store the result
-- in the named HDL parameter, attached to the system. This can be passed down into any
-- intantiated components within the system.
-- The query is performed on chains consisting of one or more logical RS Links. If the specified
-- chain contains more than one link, the intervening components must have internal links defined
-- between the sink of the preceding link and the source of the next link.
-- @tparam array_or_set(RSLink) chain an array or set of RS Links that form the chain to measure
-- @tparam string parm_name name of system-level HDL parameter to store the result into
function Builder:latency_query(...)
    if not self.cur_sys then error("no current system") end
    self.cur_sys:create_latency_query(...)
end






