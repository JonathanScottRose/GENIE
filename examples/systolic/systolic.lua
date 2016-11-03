require 'builder'

b = genie.Builder.new()

b:component('compA', 'comp_a_ver')	
	b:clock_sink('clkIface', 'i_clk')
	b:rs_src('sender', 'clkIface')
		b:signal('data', 'o_data', 10)
		b:signal('valid', 'o_vld')

b:component('compB', 'comp_b_ver')	
	b:clock_sink('clkIface', 'i_clk')
	b:rs_sink('recvr', 'clkIface')
		b:signal('data', 'i_data', 10)
		b:signal('valid', 'i_vld')

local N_RECV = 5
local send
local recv = {}		
local snd_to_recv = {}
		
b:system('TheSys')
	b:clock_sink('TopLevelClk', 'i_clk200mhz')
	
	-- one sender, N receivers
	send = b:instance('send', 'compA')
	b:link('TopLevelClk', 'send.clkIface')
	for i=0,N_RECV-1 do
		recv[i] = b:instance('recv'..i, 'compB')
		b:link('TopLevelClk', recv[i]:get_port('clkIface'))
		
		local lnk = b:link('send.sender', recv[i]:get_port('recvr'))
		snd_to_recv[i] = lnk
	end
	
	-- set up latency constraints	
	for i=0,N_RECV-1 do
		b:latency_constraint({snd_to_recv[i]}, '=', i+2)
	end
		
				
	