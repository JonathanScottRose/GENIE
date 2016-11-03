require 'builder'

b = genie.Builder.new()

-- make a constant for the data size
DWIDTH = 10

-- create two basic components
b:component('compA', 'comp_a_ver')	
		b:clock_sink('clkIface', 'i_clk')
		b:rs_src('sender', 'clkIface')
			b:signal('data', 'o_data', DWIDTH)
			b:signal('valid', 'o_vld')
			b:signal('lpid', 'o_dest', 1)
				b:linkpoint('lpA', "1'b0")
				b:linkpoint('lpB', "1'b1")
			
b:component('compB', 'comp_b_ver')	
	b:clock_sink('clkIface', 'i_clk')
	b:rs_sink('recvr', 'clkIface')
		b:signal('data', 'i_data', DWIDTH)
		b:signal('valid', 'i_vld')
		
b:system('TheSys')
	b:clock_sink('TopLevelClk', 'i_clk200mhz')
	
	-- instantiate our two components once each
	b:instance('instA', 'compA')
	b:instance('instB1', 'compB')
	b:instance('instB2', 'compB')
	
	-- connect clocks using a Lua for loop and string concatenation
	for _,dest in ipairs{'instA', 'instB1', 'instB2'} do
		b:link('TopLevelClk', dest .. '.clkIface')
	end
	
	-- define data links
	b:link('instA.sender.lpA', 'instB1.recvr')
	b:link('instA.sender.lpB', 'instB2.recvr')