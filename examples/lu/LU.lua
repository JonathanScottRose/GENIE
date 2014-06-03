require 'spec'
require 'topo_xbar'
require 'math' 


s = spec.Spec:new()
b = s:builder()
local num_cpu = 2
local cid_width = 1

dofile "cpu.lua"
--[[
b:component('mem','memnode_top_ct')
	b:clock_sink('clk_b','clk_b')
	b:reset_sink('reset','reset')
	b:interface('data_in', 'data','in','clk_b')
		b:signal('data','i_net_flit','256')
		b:signal('valid','i_net_valid')
		b:signal('ready','o_net_ready')
		b:signal('sop','i_net_sop')
		b:signal('eop','i_net_eop')
	b:interface('data_out', 'data','out','clk_b')
		b:signal('data','o_net_flit','256')
		b:signal('valid','o_net_valid')
		b:signal('ready','i_net_ready')
		b:signal('sop','o_net_sop')
		b:signal('eop','o_net_eop')
		b:signal('lp_id','o_net_lp_id', cid_width)
		b:linkpoint('bcast', cid_width .. "'d0",'broadcast')
		for i = 0, num_cpu-1 do
			b:linkpoint('cpu'..i, cid_width .. "'d" .. i+1 , 'broadcast')
		end
	
	b:interface('cmd_in', 'data','in','clk_b')
		b:signal('data','i_cmd_flit','42')
		b:signal('valid','i_cmd_valid')
		b:signal('ready','o_cmd_ready')
		b:signal('lp_id','i_cmd_lp_id', cid_width)
		b:linkpoint('ctrl',cid_width .. "'d0",'broadcast')
		for i = 0, num_cpu-1 do
			b:linkpoint('cpu'..i, cid_width .. "'d" .. i+1 , 'broadcast')
		end
	b:interface('ddr','conduit','out')
		b:signal('out','addr','30','addr')
		b:signal('out','wdata','256','wdata')
		b:signal('out','be','32','be')
		b:signal('out','read_req','1','read_req')
		b:signal('out','write_req','1','write_req')
		b:signal('out','burstcount','6','ddr_size')
		b:signal('in','waitrequest','1','waitrequest')
		b:signal('in','rdata','256','rdata')
		b:signal('in','rdata_valid','1','rdata_valid')

b:component('ctrl','ctrl_top_ct')
	b:clock_sink('clk_b','clk_b')
	b:reset_sink('reset','reset')
	b:interface('cmd_in', 'data','in','clk_b')
		b:signal('data','i_cmd_flit','42')
		b:signal('valid','i_cmd_valid')
		b:signal('ready','o_cmd_ready')
	b:interface('cmd_out', 'data','out','clk_b')
		b:signal('data','o_cmd_flit','42')
		b:signal('valid','o_cmd_valid')
		b:signal('ready','i_cmd_ready')
		b:signal('sop','o_cmd_sop')
		b:signal('eop','o_cmd_eop')
		b:signal('lp_id','o_cmd_lp_id',cid_width)
		for i = 0, num_cpu-1 do
			b:linkpoint('cpu'..i, cid_width .. "'d" .. i , 'broadcast')
		end
		b:linkpoint('mem', cid_width .. "'d" .. num_cpu,'broadcast')

	b:interface('ctrl_top','conduit', 'out' )
		b:signal('in','i_nblocks','8','nblocks')
		b:signal('in','i_go','1','go')
		b:signal('out','o_done','1','done')


b:system('lu_top', topo_xbar)

	b:instance('ctrl_node','ctrl')
		b:defparam('LP_ID_WIDTH', cid_width)
		b:defparam('MEM_LOCAL_ID', num_cpu)
	b:instance('mem_node','mem')
		b:defparam('LP_ID_WIDTH', cid_width)
		b:defparam('CTRL_LOCAL_ID', 0)
		b:defparam('BURSTLEN','6')
		b:defparam('ADDRESSWIDTH','30')
	b:export('clk_a','clock','in')
	b:export('clk_b','clock','in')
	b:export('reset','reset','in')
		
	for i = 0, num_cpu-1 do 
		local cur_inst = 'cpu' .. i 
		b:instance(cur_inst, 'cpu')
		if ( i == 0 ) then
			b:defparam('IS_BPU',1)
		else
			b:defparam('IS_BPU',0)
		end

		b:link('clk_b', cur_inst .. '.clk_b') 
		b:link('clk_a', cur_inst .. '.clk_a') 
		b:link('reset', cur_inst .. '.reset')
		b:link('mem_node.data_out.' .. cur_inst, cur_inst .. '.data_in')
		b:link('mem_node.data_out.bcast', cur_inst .. '.data_in')
		b:link(cur_inst .. '.data_out','mem_node.data_in')

		b:link(cur_inst .. '.cmd_out_mem','mem_node.cmd_in.' .. cur_inst)
		b:link(cur_inst .. '.cmd_out_ctrl','ctrl_node.cmd_in')
		b:link('ctrl_node.cmd_out.' .. cur_inst, cur_inst .. '.cmd_in')
	end
	
	b:link('clk_b','mem_node.clk_b')
	b:link('clk_b','ctrl_node.clk_b')
	b:link('reset','mem_node.reset')
	b:link('reset','ctrl_node.reset')
	b:link('ctrl_node.cmd_out.mem','mem_node.cmd_in.ctrl')
    ]]--

s:submit()
