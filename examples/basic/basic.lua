require 'spec'
require 'topo_xbar'

-- Create the Spec object that will be submitted to GENIE
local s = spec.Spec:new()

-- The builder is a convenient/simplified interface to the spec
local b = s:builder()

-- Component definition. Component's name is 'mult' and it's bound to a Verilog module also called 'mult'
b:component('mult', 'mult')
    -- Define the existence of a Verilog parameter called WIDTH
    b:parameter('WIDTH')
    -- Define a clock sink, called 'clk', which is bound to a Verilog input port also called 'clk'
	b:clock_sink('clk', 'clk')
    -- Define a Routed Streaming sink called 'input_a', synchronous to clock sink 'clk'
	b:interface('input_a', 'data', 'in', 'clk')
        -- Bind data/valid/ready signal roles to Verilog input/output ports
		b:signal('data', 'i_a', 'WIDTH')
		b:signal('valid', 'i_a_valid')
		b:signal('ready', 'o_a_ready')
	b:interface('input_b', 'data', 'in', 'clk')
		b:signal('data', 'i_b', 'WIDTH')
		b:signal('valid', 'i_b_valid')
		b:signal('ready', 'o_b_ready')
	b:interface('output', 'data', 'out', 'clk')
		b:signal('data', 'o_result', 'WIDTH')
		b:signal('valid', 'o_result_valid')
		b:signal('ready', 'i_result_ready')

b:component('add', 'add')
    b:parameter('WIDTH')
	b:clock_sink('clk', 'clk')
	b:interface('input_a', 'data', 'in', 'clk')
		b:signal('data', 'i_a', 'WIDTH')
		b:signal('valid', 'i_a_valid')
		b:signal('ready', 'o_a_ready')
	b:interface('input_b', 'data', 'in', 'clk')
		b:signal('data', 'i_b', 'WIDTH')
		b:signal('valid', 'i_b_valid')
		b:signal('ready', 'o_b_ready')
	b:interface('output', 'data', 'out', 'clk')
		b:signal('data', 'o_result', 'WIDTH')
		b:signal('valid', 'o_result_valid')
		b:signal('ready', 'i_result_ready')

-- Define a system called 'the_sys' that uses a crossbar topology for its network (although this system is too simple for split/merge nodes to be needed anyway)
b:system('the_sys', topo_xbar)
    -- Create an instance of the 'mult' component called 'the_mult'
	b:instance('the_mult', 'mult')
        -- Set the value of a parameter
		b:defparam('WIDTH', 16)
	b:instance('the_add', 'add')
		b:defparam('WIDTH', 16)
        
    -- Manually create a top-level clock sink called 'clk'
    -- Connect the clock sinks of the mult/add instances to this export
	b:export('clk', 'clock', 'in')
	b:link('clk', 'the_mult.clk')
	b:link('clk', 'the_add.clk')
    
    -- Export existing interfaces of instantiated components. This also creates the links to them.
    b:export_if('a', 'the_mult.input_a')
    b:export_if('b', 'the_mult.input_b')
    b:export_if('c', 'the_add.input_b')
    b:export_if('out', 'the_add.output')
    
    -- Connect the mult's output to the add's input
	b:link('the_mult.output', 'the_add.input_a')

-- Submit the spec to GENIE
s:submit()
