require 'builder'

local b = genie.Builder.new()


-- Component definition. Component's name is 'mult' and it's bound to a Verilog module also called 'mult'
b:component('mult', 'mult')
    -- Define the existence of a Verilog parameter called WIDTH
    b:parameter('WIDTH')
    -- Define a clock sink, called 'clk', which is bound to a Verilog input port also called 'clk'
	b:clock_sink('clk', 'clk')
    -- Define a Routed Streaming sink called 'input_a', synchronous to clock sink 'clk'
	b:rs_sink('input_a', 'clk')
        -- Bind data/valid/ready signal roles to Verilog input/output ports
		b:signal('data', 'i_a', 'WIDTH')
		b:signal('valid', 'i_a_valid')
		b:signal('ready', 'o_a_ready')
	-- More RS sources/sinks
	b:rs_sink('input_b', 'clk')
		b:signal('data', 'i_b', 'WIDTH')
		b:signal('valid', 'i_b_valid')
		b:signal('ready', 'o_b_ready')
	b:rs_src('output', 'clk')
		b:signal('data', 'o_result', 'WIDTH')
		b:signal('valid', 'o_result_valid')
		b:signal('ready', 'i_result_ready')

-- An adder: also has two inputs and one output
b:component('add', 'add')
    b:parameter('WIDTH')
	b:clock_sink('clk', 'clk')
	b:rs_sink('input_a', 'clk')
		b:signal('data', 'i_a', 'WIDTH')
		b:signal('valid', 'i_a_valid')
		b:signal('ready', 'o_a_ready')
	b:rs_sink('input_b', 'clk')
		b:signal('data', 'i_b', 'WIDTH')
		b:signal('valid', 'i_b_valid')
		b:signal('ready', 'o_b_ready')
	b:rs_src('output', 'clk')
		b:signal('data', 'o_result', 'WIDTH')
		b:signal('valid', 'o_result_valid')
		b:signal('ready', 'i_result_ready')

-- Define a system called 'the_sys' that uses a crossbar topology for its network 
-- (although this system is too simple for split/merge nodes to be needed anyway)
b:system('the_sys')
    -- Create an instance of the 'mult' component called 'the_mult'
	b:instance('the_mult', 'mult')
        -- Set the value of a parameter
		b:parameter('WIDTH', 16)
	b:instance('the_add', 'add')
		b:parameter('WIDTH', 16)
        
    -- Manually create a top-level clock sink called 'clk' with Verilog
	-- port name also being 'clk'
    -- Connect the clock sinks of the mult/add instances to this export
	b:clock_sink('clk', 'clk')
	b:link('clk', 'the_mult.clk')
	b:link('clk', 'the_add.clk')
    
    -- Export existing interfaces of instantiated components. This also creates the links to them.
    b:export('the_mult.input_a', 'a')
    b:export('the_mult.input_b', 'b')
    b:export('the_add.input_b', 'c')
    b:export('the_add.output', 'out')
    
    -- Connect the mult's output to the add's input
	b:link('the_mult.output', 'the_add.input_a')

