require 'builder'
require 'topo_xbar'

b = genie.Builder.new()

-- begin definition of component compA which wraps Verilog module comp_a_ver
b:component('compA', 'comp_a_ver')	
	-- create a clock sink interface, containing Verilog input port i_clk
	b:clock_sink('clkIface', 'i_clk')
	-- create a Routed Streaming source interface, synchronous to clock interface clkIface
	b:rs_src('sender', 'clkIface')
		-- this Interface contains a 10 bit wide data signal and a valid signal for handshaking
		b:signal('data', 'o_data', 10)
		b:signal('valid', 'o_vld')

-- create a second similar component that has a Sink interface
b:component('compB', 'comp_b_ver')	
	b:clock_sink('clkIface', 'i_clk')
	b:rs_sink('recvr', 'clkIface')
		b:signal('data', 'i_data', 10)
		b:signal('valid', 'i_vld')
		
-- Define a top-level system called TheSys.
-- This system uses the built-in Sparse Crossbar topology function topo_xbar,
-- even though we expect a single point-to-point link.
b:system('TheSys', topo_xbar)
	-- Create a top-level clock sink interface for the system and
	-- decide what the name of the associated Verilog signal will be 
	-- in the GENIE-generated Verilog module that will be output.
	b:clock_sink('TopLevelClk', 'i_clk200mhz')
	
	-- instantiate our two components once each
	b:instance('instA', 'compA')
	b:instance('instB', 'compB')
	
	-- connect clocks
	b:link('TopLevelClk', 'instA.clkIface')
	b:link('TopLevelClk', 'instB.clkIface')
	
	-- define the point-to-point data link
	b:link('instA.sender', 'instB.recvr')