require 'builder'
require 'topo_xbar'
require 'topo_ring'

local b = genie.Builder.new()

b:component('dispatch', 'dispatch')
	b:parameter('WIDTH')
	b:clock_sink('clk', 'clk')
	b:reset_sink('reset', 'reset')
	b:rs_src('out', 'clk')
		b:signal('valid', 'o_valid')
		b:signal('ready', 'i_ready')
		b:signal('data', 'o_data', 'WIDTH')
		b:signal('lpid', 'o_lp', '1')
		b:linkpoint('unit_a', "1'b0", 'broadcast')
		b:linkpoint('unit_b', "1'b1", 'broadcast')

b:component('inverter', 'inverter')
	b:parameter('WIDTH')
	b:clock_sink('clk', 'clk')
	b:reset_sink('reset', 'reset')
	b:rs_sink('in', 'clk')
		b:signal('valid', 'i_valid')
		b:signal('ready', 'o_ready')
		b:signal('data', 'i_data', 'WIDTH')
	b:rs_src('out', 'clk')
		b:signal('valid', 'o_valid')
		b:signal('ready', 'i_ready')
		b:signal('data', 'o_data', 'WIDTH')
	
b:component('xorer', 'xorer')
	b:parameter('WIDTH')
	b:clock_sink('clk', 'clk')
	b:reset_sink('reset', 'reset')
	b:rs_sink('in', 'clk')
		b:signal('valid', 'i_valid')
		b:signal('ready', 'o_ready')
		b:signal('data', 'i_data', 'WIDTH')
		b:signal('lpid', 'i_lp', '4')
		b:linkpoint('in0', "4'b1001", 'broadcast')
		b:linkpoint('in1', "4'b0110", 'broadcast')
	b:rs_src('out', 'clk')
		b:signal('valid', 'o_valid')
		b:signal('ready', 'i_ready')
		b:signal('data', 'o_data', 'WIDTH')

b:component('reverser', 'reverser')
	b:parameter('WIDTH')
	b:clock_sink('clk', 'clk')
	b:reset_sink('reset', 'reset')
	b:rs_sink('in', 'clk')
		b:signal('valid', 'i_valid')
		b:signal('ready', 'o_ready')
		b:signal('data', 'i_data', 'WIDTH')
	b:rs_src('out', 'clk')
		b:signal('valid', 'o_valid')
		b:signal('ready', 'i_ready')
		b:signal('data', 'o_data', 'WIDTH')

b:system('sm_test', make_topo_xbar(true, true))
    b:clock_sink('clk', 'clk')
    b:reset_sink('reset', 'reset')
	b:instance('the_dispatch', 'dispatch')
		b:parameter('WIDTH', '16')
	b:instance('the_inverter', 'inverter')
		b:parameter('WIDTH', '16')
	b:instance('the_reverser', 'reverser')
		b:parameter('WIDTH', '16')
	b:instance('xorro', 'xorer')
		b:parameter('WIDTH', '16')
	for dest in Set.mkvalues{'the_dispatch', 'the_inverter', 'the_reverser', 'xorro'} do
		b:link('clk', dest .. '.' .. 'clk')
		b:link('reset', dest .. '.' .. 'reset')
	end
	b:link('the_dispatch.out.unit_a', 'the_inverter.in')
	b:link('the_dispatch.out.unit_b', 'the_reverser.in')
	b:link('the_inverter.out', 'xorro.in.in0')
	b:link('the_reverser.out', 'xorro.in.in1')


