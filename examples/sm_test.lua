require 'spec'

local s = spec.Spec:new()
local b = s:builder()

b:component('dispatch', 'dispatch')
	b:parameter('WIDTH')
	b:clock_sink('clk', 'clk')
	b:reset_sink('reset', 'reset')
	b:interface('out', 'data', 'out', 'clk')
		b:signal('valid', 'o_valid')
		b:signal('ready', 'i_ready')
		b:signal('data', 'o_data', 'WIDTH')
		b:signal('lp_id', 'o_lp', '1')
		b:linkpoint('unit_a', "1'b0", 'broadcast')
		b:linkpoint('unit_b', "1'b1", 'broadcast')

b:component('inverter', 'inverter')
	b:parameter('WIDTH')
	b:clock_sink('clk', 'clk')
	b:reset_sink('reset', 'reset')
	b:interface('in', 'data', 'in', 'clk')
		b:signal('valid', 'i_valid')
		b:signal('ready', 'o_ready')
		b:signal('data', 'i_data', 'WIDTH')
	b:interface('out', 'data', 'out', 'clk')
		b:signal('valid', 'o_valid')
		b:signal('ready', 'i_ready')
		b:signal('data', 'o_data', 'WIDTH')
	
b:component('xorer', 'xorer')
	b:parameter('WIDTH')
	b:clock_sink('clk', 'clk')
	b:reset_sink('reset', 'reset')
	b:interface('in', 'data', 'in', 'clk')
		b:signal('valid', 'i_valid')
		b:signal('ready', 'o_ready')
		b:signal('data', 'i_data', 'WIDTH')
		b:signal('lp_id', 'i_lp', '4')
		b:linkpoint('in0', "4'b1001", 'broadcast')
		b:linkpoint('in1', "4'b0110", 'broadcast')
	b:interface('out', 'data', 'out', 'clk')
		b:signal('valid', 'o_valid')
		b:signal('ready', 'i_ready')
		b:signal('data', 'o_data', 'WIDTH')

b:component('reverser', 'reverser')
	b:parameter('WIDTH')
	b:clock_sink('clk', 'clk')
	b:reset_sink('reset', 'reset')
	b:interface('in', 'data', 'in', 'clk')
		b:signal('valid', 'i_valid')
		b:signal('ready', 'o_ready')
		b:signal('data', 'i_data', 'WIDTH')
	b:interface('out', 'data', 'out', 'clk')
		b:signal('valid', 'o_valid')
		b:signal('ready', 'i_ready')
		b:signal('data', 'o_data', 'WIDTH')

b:system('sm_test')
	b:instance('the_dispatch', 'dispatch')
		b:defparam('WIDTH', '16')
	b:instance('the_inverter', 'inverter')
		b:defparam('WIDTH', '16')
	b:instance('the_reverser', 'reverser')
		b:defparam('WIDTH', '16')
	b:instance('xorro', 'xorer')
		b:defparam('WIDTH', '16')
	b:export('clk', 'clock', 'in')
	b:export('reset', 'reset', 'in')
	for dest in Set.mkvalues{'the_dispatch', 'the_inverter', 'the_reverser', 'xorro'} do
		b:link('clk', dest .. '.' .. 'clk')
		b:link('reset', dest .. '.' .. 'reset')
	end
	b:link('the_dispatch.out.unit_a', 'the_inverter.in')
	b:link('the_dispatch.out.unit_b', 'the_reverser.in')
	b:link('the_inverter.out', 'xorro.in.in0')
	b:link('the_reverser.out', 'xorro.in.in1')

s:submit()

