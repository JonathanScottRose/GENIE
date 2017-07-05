require 'topo_xbar'
require 'builder'
require 'math'
require 'util'
dofile 'cpu.lua'

--[[
function topo_lobe(sys, N_CPU, N_MEM, DISABLE_BCAST)
    local function inp_ring(things, reg_input, has_extern_in, has_extern_out)
        local last = nil
        for i = 1,#things-1 do
            local thing = things[i]
            
            -- new output added to ring, optionally buffered
            local ingress = thing
            if reg_input then
                ingress = sys:add_buffer(util.unique_key(sys:get_objects(), 'merge_reg'))
                sys:add_link(thing,ingress)
            end
            
            -- join new thing into ring
            if last then
                local mg = sys:add_merge(util.unique_key(sys:get_objects(), 'merge'))
                local rg = sys:add_buffer(mg:get_name() .. '_reg')
                sys:add_link(ingress, mg)
                sys:add_link(last, mg)
                sys:add_link(mg, rg)
                last = rg
            else
                last = ingress
            end
        end
        
        -- export from ring
        local extern_out = nil           
        if has_extern_out then
            extern_out = sys:add_split(util.unique_key(sys:get_objects(), 'ext_out'))
            sys:add_link(last, extern_out)
            last = sys:add_buffer(extern_out:get_name() .. '_reg')
            sys:add_link(extern_out, last)
        end
        
        -- import into ring
        local extern_in = nil
        if has_extern_in then
            extern_in = sys:add_merge(util.unique_key(sys:get_objects(), 'ext_in'))
            sys:add_link(last, extern_in)
            last = sys:add_buffer(extern_in:get_name() .. '_reg')
            sys:add_link(extern_in, last)
        end
        
        -- connect last thing
        local thing = things[#things]
        sys:add_link(last, thing)
        
        return extern_in,extern_out
    end
    
    local rdreq_ext_out = {}
    local rdreq_ext_in = {}
    local wrreq_ext_out = {}
    local wrreq_ext_in = {}
    	
    -- build rdreq/wrreq input rings
    for m = 0,N_MEM-1 do
        local things_rdreq = {}
        local things_wrreq = {}
        for n = m,N_CPU-1,N_MEM do
            table.insert(things_rdreq, sys:get_object('cpu'..n..'.rdreq'):get_topo_port())
            table.insert(things_wrreq, sys:get_object('cpu'..n..'.wrreq'):get_topo_port())
        end
        table.insert(things_rdreq, sys:get_object('mem'..m..'.rdreq'):get_topo_port())
        table.insert(things_wrreq, sys:get_object('mem'..m..'.wrreq'):get_topo_port())
        rdreq_ext_in[m], rdreq_ext_out[m] = inp_ring(things_rdreq, true, true, true)
        wrreq_ext_in[m], wrreq_ext_out[m] = inp_ring(things_wrreq, false, m ~= 0, m == 0)
    end
    
    -- rdreq for ctrl node
    local ctrl = sys:get_object('ctrl_node')
    local ctrl_in = ctrl:get_port('i_rdreq'):get_topo_port()
    local ctrl_out = ctrl:get_port('o_rdreq'):get_topo_port()
    if N_MEM > 1 then
        local ctrl_in_mg = sys:add_merge('ctrl_i_rdreq_mg')
        local ctrl_in_mg_rg = sys:add_buffer('ctrl_i_rdreq_mg_reg')
        local ctrl_out_sp = sys:add_split('ctrl_o_rdreq_sp')
        sys:add_link(ctrl_in_mg_rg, ctrl_in)
        sys:add_link(ctrl_in_mg, ctrl_in_mg_rg)
        sys:add_link(ctrl_out, ctrl_out_sp)
        ctrl_in = ctrl_in_mg
        ctrl_out = ctrl_out_sp
    end    
    
    -- build rdresp distribution network
    local rdresp_ext_out = {}
    local rdresp_ext_in = {}
    for m = 0,N_MEM-1 do
        local mem_out = sys:get_object('mem'..m..'.rdresp'):get_topo_port()
        local last = sys:add_buffer('mem_rdresp_reg'..m)
        sys:add_link(mem_out, last)
        if N_MEM > 1 then
            local mg = sys:add_merge('mem_rdresp_ingress'..m)
            rdresp_ext_in[m] = mg
            sys:add_link(last, mg)
            last = sys:add_buffer(mg:get_name() .. '_reg')
            sys:add_link(mg, last)
        end
        
        local sp = sys:add_split('mem_rdresp_split'..m)
        rdresp_ext_out[m] = sp
        sys:add_link(last, sp)
        
        for n = m,N_CPU-1,N_MEM do
            local ce_in = sys:get_object('cpu'..n..'.rdresp'):get_topo_port()
            local rg = sys:add_buffer('cpu'..n..'_rdresp_reg')
            sys:add_link(sp, rg)
            sys:add_link(rg, ce_in)
        end
    end
    
    -- connect rings to each other
    if DISABLE_BCAST==1 then error "I don't know how to do this" end
    for m = 0,N_MEM-1 do
        if m > 0 then
            sys:add_link(rdreq_ext_out[0], rdreq_ext_in[m])
            sys:add_link(wrreq_ext_out[0], wrreq_ext_in[m])
        end
        sys:add_link(rdreq_ext_out[m], ctrl_in)
        sys:add_link(ctrl_out, rdreq_ext_in[m])
        for m2 = 0,N_MEM-1 do
            if m ~= m2 then
                sys:add_link(rdresp_ext_out[m], rdresp_ext_in[m2])
            end
        end
    end
    
	-- connect everything else using a crossbar
	topo_xbar(sys)
end
--]]

local function is_pwr_of_2 (a) 
    local i = 0
    assert (a > 0) 
    while (a > 0) do
        i = i + (a % 2) 
        a = a >> 1
    end
    return (i == 1) 
end

-- experimental parameters; knobs
local N_CPU = tonumber(genie.argv.N_CPU) or error("Must define N_CPU")
local N_MEM = tonumber(genie.argv.N_MEM) or error("Must define N_MEM")
local DISABLE_BCAST = tonumber(genie.argv.DISABLE_BCAST) or 0
local PERF_SIM = tonumber(genie.argv.PERF_SIM)==1 or false
local TOPOLOGY = tonumber(genie.argv.RING) or 0 
local IMP = tonumber(genie.argv.IMP) or 1
assert (is_pwr_of_2(N_MEM)) 

-- dependent parameters 
local N_CPUBITS = math.max(1,util.clog2(N_CPU))
local N_MEMBITS = math.max(1,util.clog2(N_MEM))

-- static parameters; those we do not sweep over
local BURSTBITS = 6
local MEM_AWIDTH = 30 
local MEM_DWIDTH = 256


b = genie.Builder.new()

-- create a two versions of the CE: special CE0 flavor, and generic CEn flavor
define_cpu_parts(b, N_MEM, PERF_SIM)
define_cpu(b, N_MEM, true, 'bpu', PERF_SIM)
define_cpu(b, N_MEM, false, 'cpu', PERF_SIM)

-- define memory controller node
b:component('mem','mem_lu2phy')
	b:clock_sink('clk')
	b:reset_sink('reset')
    
    b:rs_sink('rdreq', 'clk')
		b:importance(IMP)
        b:signal('valid','i_net_rdreq_valid')
        b:signal('ready','o_net_rdreq_ready')
        b:signal('databundle','blkx', 'i_net_rdreq_blkx',MAX_BDIMBITS)
        b:signal('databundle','blky','i_net_rdreq_blky',MAX_BDIMBITS)
        b:signal('databundle','whichbufs','i_net_rdreq_whichbufs',3)
        b:signal('databundle','whichpage','i_net_rdreq_whichpage',1)
        b:signal('address','i_net_rdreq_lpid','LPID_BITS') 
    
    b:rs_src('rdresp', 'clk')
		b:packet_size(512)
        b:signal('databundle','data','o_net_rdresp_data',MEM_DWIDTH)
        b:signal('databundle','addr','o_net_rdresp_addr',CACHE_AWIDTH)
        b:signal('databundle','whichbufs','o_net_rdresp_whichbufs',3)
        b:signal('databundle','whichpage','o_net_rdresp_whichpage',1)
        b:signal('valid','o_net_rdresp_valid')
        b:signal('ready','i_net_rdresp_ready')
        b:signal('address','o_net_rdresp_lpid', 'LPID_BITS')

    b:rs_sink('wrreq', 'clk')
		b:packet_size(512)
        b:signal('databundle','data','i_net_wrreq_data',MEM_DWIDTH)
        b:signal('databundle','blkx','i_net_wrreq_x',MAX_BDIMBITS)
        b:signal('databundle','blky','i_net_wrreq_y',MAX_BDIMBITS)
        b:signal('databundle','sop','i_net_wrreq_sop',1)
		b:signal('valid','i_net_wrreq_valid')
        b:signal('ready','o_net_wrreq_ready')

    b:rs_sink('nblocks','clk')
        b:signal('data','i_nblocks',MAX_BDIMBITS)
        b:signal('valid','i_nblocks_valid')

    b:conduit_src('read')
        b:signal('in','data','i_read_data',MEM_DWIDTH)
        b:signal('in','valid','i_read_valid','1')
        b:signal('in','waitrequest','i_read_waitrequest','1') 
        b:signal('out','addr','o_read_addr','MEM_AWIDTH')
        b:signal('out','burstcount','o_read_burstcount','BURSTBITS') 
        b:signal('out','req','o_read_req','1') 
        
    b:conduit_src('write')
        b:signal('in','waitrequest','i_write_waitrequest','1')
        b:signal('out','addr','o_write_addr','MEM_AWIDTH') 
        b:signal('out','burstcount','o_write_burstcount','BURSTBITS') 
        b:signal('out','data','o_write_data',MEM_DWIDTH) 
        b:signal('out','req','o_write_req','1') 

-- define control node
b:component('ctrl','ctrl_top')
	b:clock_sink('clk')
	b:reset_sink('reset')
    b:rs_sink('i_rdreq','clk')
		b:importance(IMP)
        b:signal('valid','i_rdreq')
        b:signal('databundle','blkx','i_rdreq_blkx',MAX_BDIMBITS)
        b:signal('databundle','blky','i_rdreq_blky',MAX_BDIMBITS)
        b:signal('databundle','whichbufs','i_rdreq_whichbufs','3')
        b:signal('databundle','whichpage','i_rdreq_whichpage','1')
    
    b:rs_src('o_rdreq','clk')
        b:signal('valid','o_rdreq')
        b:signal('ready','i_rdreq_ack')
        b:signal('databundle','blkx','o_rdreq_blkx',MAX_BDIMBITS)
        b:signal('databundle','blky','o_rdreq_blky',MAX_BDIMBITS)
        b:signal('databundle','whichbufs','o_rdreq_whichbufs','3')
        b:signal('databundle','whichpage','o_rdreq_whichpage','1')
        b:signal('address','o_rdreq_lpid',N_MEMBITS)

    b:rs_src('cpu_go','clk')
        b:signal('valid','o_go')
        b:signal('ready','i_go_ack')
        b:signal('databundle','firstcol','o_go_firstcol','1')
        b:signal('databundle','blkx','o_go_blkx',MAX_BDIMBITS)
        b:signal('databundle','blkymax','o_go_blkymax',MAX_BDIMBITS)
        b:signal('databundle','blkxmin','o_go_blkxmin',MAX_BDIMBITS)
        b:signal('address','o_go_lpid',N_CPUBITS)
    
    b:rs_sink('cpu_done', 'clk')
        b:signal('valid','i_done')

    b:rs_src('nblocks', 'clk')
        b:signal('valid','o_nblocks_valid')
        b:signal('data','o_nblocks',MAX_BDIMBITS)
        b:signal('ready', 'i_nblocks_ack')
        
    b:conduit_src('ctrl_top')
		b:signal('in','nblocks','i_nblocks',MAX_BDIMBITS)
		b:signal('in','go','i_go','1')
		b:signal('out','done','o_done','1')

-- define system        
b:system('lu')
    -- embed these to allow simulator testbench to pick them up
    b:int_param('N_CPU', N_CPU);
    b:int_param('N_MEM', N_MEM);
    
    -- top-level clock and reset inputs
    b:clock_sink('clk_a', 'clk_a')
    b:clock_sink('clk_b', 'clk_b')
    b:reset_sink('reset', 'reset')
    
    -- instantiate control node
	b:instance('ctrl','ctrl_node')
        b:int_param('N', N_CPU)
        b:int_param('M', N_MEM)

    b:export('ctrl_node.ctrl_top', 'ctrl')
    b:clock_link('clk_b','ctrl_node.clk')
    b:reset_link('reset','ctrl_node.reset');
    
    -- instantiate memory controllers, connect them to control node
    for i = 0, N_MEM-1 do 
        local mem_inst = 'mem' .. i
        b:instance('mem', mem_inst)
            b:int_param('MEM_AWIDTH', MEM_AWIDTH)
            b:int_param('BURSTBITS', BURSTBITS)
            --b:int_param('READ_BURST', MEM_READ_BURST)
            --b:int_param('WRITE_BURST', MEM_WRITE_BURST)
            b:int_param('N', N_CPU)
            b:int_param('M', N_MEM)
            --b:int_param('RDREQ_BUF_DEPTH', MEM_RDREQ_BUF_DEPTH)
            --b:int_param('RDRESP_BUF_DEPTH', MEM_RDRESP_BUF_DEPTH)
            b:int_param('LPID_BITS', math.max(1, util.clog2(N_CPU+1)))
            b:int_param('BCAST_LPID', N_CPU)

        -- these are top-level conduit ports that drive the memory controller.
        -- the signal names are made explicit, and all N_MEM ports map to ranges of
        -- the same verilog ports
        b:conduit_src('read' .. i)
			b:signal_ex('in','data', 
				{name='i_read_data',width=MEM_DWIDTH,depth=N_MEM},
				{width=MEM_DWIDTH,lo_slice=i})
            b:signal_ex('in','valid',
				{name='i_read_valid',width=N_MEM},
				{width=1,lo_bit=i})
            b:signal_ex('in','waitrequest',
				{name='i_read_waitrequest',width=N_MEM},
				{width=1,lo_bit=i})
			b:signal_ex('out','addr',
				{name='o_read_addr',width=MEM_AWIDTH,depth=N_MEM},
				{width=MEM_AWIDTH,lo_slice=i})
            b:signal_ex('out','burstcount',
				{name='o_read_burstcount',width=BURSTBITS,depth=N_MEM},
				{width=BURSTBITS,lo_slice=i})
			b:signal_ex('out','req',
				{name='o_read_req',width=N_MEM},
				{width=1,lo_bit=i})
            
        b:conduit_src('write' .. i)
			b:signal_ex('in','waitrequest',
				{name='i_write_waitrequest',width=N_MEM},
				{width=1,lo_bit=i})
			b:signal_ex('out','addr',
				{name='o_write_addr',width=MEM_AWIDTH,depth=N_MEM},
				{width=MEM_AWIDTH,lo_slice=i})
            b:signal_ex('out','burstcount',
				{name='o_write_burstcount',width=BURSTBITS,depth=N_MEM},
				{width=BURSTBITS,lo_slice=i})
            b:signal_ex('out','data', 
				{name='o_write_data',width=MEM_DWIDTH,depth=N_MEM},
				{width=MEM_DWIDTH,lo_slice=i})
            b:signal_ex('out','req',
				{name='o_write_req',width=N_MEM},
				{width=1,lo_bit=i})
        
        b:clock_link('clk_b',mem_inst .. '.clk')
        b:reset_link('reset',mem_inst .. '.reset')
        if DISABLE_BCAST==0 then
            b:rs_link('ctrl_node.o_rdreq', mem_inst .. '.rdreq', i, N_CPU)
        end
        b:rs_link('ctrl_node.nblocks', mem_inst .. '.nblocks')
        b:conduit_link(mem_inst .. '.read', 'read' .. i)
        b:conduit_link(mem_inst .. '.write', 'write' .. i)
    end
    
    -- instantiate the CEs
    for i = 0, N_CPU-1 do
        local is_bpu = i==0
        local cpu_inst = 'cpu' .. i
        local model = is_bpu and 'bpu' or 'cpu'
        
        b:instance(model, cpu_inst)
            b:int_param('DISABLE_BCAST',DISABLE_BCAST)
            
        b:clock_link('clk_b', cpu_inst .. '.clk_b') 
        b:clock_link('clk_a', cpu_inst .. '.clk_a') 
        b:reset_link('reset', cpu_inst .. '.reset')
    
        -- links to control node
        b:rs_link('ctrl_node.cpu_go', cpu_inst .. '.go', i)
        b:rs_link(cpu_inst .. '.done', 'ctrl_node.cpu_done')
        if DISABLE_BCAST==0 then
            b:rs_link(cpu_inst .. '.rdreq', 'ctrl_node.i_rdreq', N_CPU)
        end
    
        -- links to memory controllers
		local rdresp_bcasts = {}
        for j = 0, N_MEM-1 do
            local mem_inst = 'mem' .. j
            
            -- write requests: ce0 goes to all, ce1+ goes to just one
            if is_bpu or i % N_MEM == j then
                b:rs_link(cpu_inst .. '.wrreq', mem_inst .. '.wrreq', j)
            end
            
            -- read requests: ce0 goes to all, ce1+ goes to just one
            -- read replies: all go to ce0, just one goes to ce1+
            -- no broadcast: ce1+ behaves like ce0
            if is_bpu or DISABLE_BCAST==1 or i % N_MEM == j then
                b:rs_link(cpu_inst .. '.rdreq', mem_inst .. '.rdreq', j, i)
                b:rs_link(mem_inst .. '.rdresp', cpu_inst .. '.rdresp', i)
            end
            
            -- read reply broadcasts: goes to all ce
            if DISABLE_BCAST==0 then
                local l = b:rs_link(mem_inst .. '.rdresp', cpu_inst .. '.rdresp', N_CPU)
				table.insert(rdresp_bcasts, l)
            end
        end
		
		-- the rdresp broadcasts never occur simultaneously
		if DISABLE_BCAST==0 then
			b:make_exclusive(rdresp_bcasts)
		end
    end 
	
	--[[
	if TOPOLOGY == 1 then 
		topo_xbar(sys, true, true, false)
	elseif TOPOLOGY == 2 then
		topo_lobe(sys,N_CPU,N_MEM,DISABLE_BCAST)
	end
	--]]