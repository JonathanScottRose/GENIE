require 'util'
require 'math' 

-- global CE-related constants (can be used by any calling scripts too)
CACHE_DWIDTH = 256
BSIZEBITS = 6
MAX_BDIMBITS = 8 
CACHE_AWIDTH = BSIZEBITS+BSIZEBITS

-- local CE-related constants private to this script
local CACHE_LATENCY = 4

-- list of forward/backward signals between Pipe Control and Pipeline.
-- used to simplify and clean up the creation of the matching Conduit interfaces at both ends
local sigs_ctrl_to_pipe = Set.make{'k_reset','k_inc','i_reset','i_inc',
            'i_load_k1','j_reset','j_inc','j_load_k','j_load_k1',
            'valid','norm','recip','wr_top','wr_left','wr_cur','whichpage'}
            
local sigs_pipe_to_ctrl = Set.make{'k_done','i_done','j_done','pipe_empty'}
            
-- linkpoint IDs used by the Pipeline to address various combinations of the five caches when writing.
-- each letter combination represents a series of cache blocks (tc=top+current) and the number is the buffer index.
-- When one-hot (multicast) linkpoints are implemented in the future, this exhaustive enumeration will be obsolete.
--local pipe_wrreq_linkpoints = {tcl0="4'b1110", cl0="4'b0110", tc0="4'b1100", c0="4'b0100", cl1="4'b0111", c1="4'b0101"};
local pipe_wrreq_linkpoints = {tcl0=14, cl0=6, tc0=12, c0=4, cl1=7, c1=5};

-- used to autmate the creation of the various linkpoints on the Net-to-Cache block to let it simultaneously target
-- all the combinations of cache buffers for writing
--local n2c_wrreq_linkpoints = {cl0="4'b0110", tcl0="4'b1110", tc0="4'b1100", c0="4'b0100", l0="4'b0010", cl1="4'b0111", c1="4'b0101", l1="4'b0011"};
local n2c_wrreq_linkpoints = {cl0=6, tcl0=14, tc0=12, c0=4, l0=2, cl1=7, c1=5, l1=3};								 

-- creates definitions for all the components inside a CE (but not the CE system itself)
-- it depends on the number of memory controllers in the outside system
function define_cpu_parts(b, N_MEM, PERF_SIM)
    -- define cache component
    b:component('cache','cpu_cache')
        b:clock_sink('clk')
        b:reset_sink('reset')
        b:rs_sink('wrreq','clk')
            b:signal('valid','i_wrreq_valid')
            b:signal('databundle','data','i_wrreq_data',CACHE_DWIDTH)
            b:signal('databundle','addr','i_wrreq_addr',CACHE_AWIDTH)
        b:rs_sink('rdreq','clk')
            b:signal('valid','i_rdreq_valid')
            b:signal('ready','o_rdreq_ready')
            b:signal('data','i_rdreq_addr',CACHE_AWIDTH)
            b:signal('address','i_rdreq_lp_id',1)
        b:rs_src('rdresp','clk')
            b:signal('valid','o_rdresp_valid')
            b:signal('ready','i_rdresp_ready')
            b:signal('data','o_rdresp_data', CACHE_DWIDTH)
            b:signal('address','o_rdresp_lp_id',1)
		--b:internal_link('rdreq', 'rdresp', 4)
    
    -- define the Pipeline component
    b:component('pipe','cpu_pipeline')
        b:clock_sink('clk')
        b:reset_sink('reset')
        b:conduit_sink('ctrl')
            -- use the list of signals base-names defined earlier to automate this part
            for sig in Set.values(sigs_ctrl_to_pipe) do
                b:signal('fwd',sig,'i_' .. sig,1)
            end
            for sig in Set.values(sigs_pipe_to_ctrl) do
                b:signal('rev',sig,'o_' .. sig,1)
            end
        for i,cache in ipairs{'top','left','cur'} do
            -- this code creates a read request and read response interface for each cache block type.
            -- create the read request interface first.
            local iface = cache .. "_rdreq"
            local isig = 'i_' .. iface
            local osig = 'o_' .. iface
            local istop = cache == 'top'
            b:rs_src(iface,'clk')
                b:signal('valid',osig .. '_valid')
                b:signal('ready',isig .. '_ready')
                b:signal('data',osig .. '_addr',CACHE_AWIDTH)
                if not istop then
                    -- left/cur (but not top) ports need to have two linkpoints so they can address both buffers of
                    -- those caches
                    b:signal('address',osig .. '_lp_id',1)
                end
            -- read interface
            iface = cache .. "_rdresp"
            isig = 'i_' .. iface
            osig = 'o_' .. iface
            b:rs_sink(iface,'clk')
                b:signal('valid',isig .. '_valid')
                b:signal('ready',osig .. '_ready')
                b:signal('data',isig .. '_data',CACHE_DWIDTH)
        end
        -- write request interface. there's only one of these but it has a variety of linkpoints so it can 
        -- simultaneously target various combinations of the 5 possible cache buffers.
        b:rs_src('wrreq','clk')
            b:signal('valid','o_wr_valid')
            b:signal('ready','i_wr_ready')
            b:signal('databundle','data','o_wr_data',CACHE_DWIDTH)
            b:signal('databundle','addr','o_wr_addr',CACHE_AWIDTH)
            b:signal('address','o_wr_lp_id',4)

    -- define Pipeline Control component
    b:component('pipe_ctrl', PERF_SIM and 'cpu_pipe_ctrl_dummy' or 'cpu_pipe_ctrl')
        b:clock_sink('clk')
        b:reset_sink('reset')
        b:rs_sink('go','clk')
            b:signal('valid','i_go')
            b:signal('databundle','flush','i_flush',1)
            b:signal('databundle','mode','i_mode',2)
            b:signal('databundle','whichpage','i_whichpage',1)
		b:rs_src('done','clk')
			b:signal('valid','o_done')
        if not PERF_SIM then
            b:conduit_src('pipe')
                for sig in Set.values(sigs_ctrl_to_pipe) do
                    b:signal('fwd',sig,'o_' .. sig,1)
                end
                for sig in Set.values(sigs_pipe_to_ctrl) do
                    b:signal('rev',sig,'i_' .. sig,1)
                end
        end
 
    -- define Main Control component.
    -- when talking to the outside world, it needs to be able to address:
    --   M memory controlles + CTRL node for read requests
    local RDREQ_ADDR_WIDTH = math.max(1, util.clog2(N_MEM+1))
    b:component('main_ctrl','cpu_main_ctrl')
        b:clock_sink('clk','clk_b')
        b:reset_sink('reset')
        b:rs_sink('net_go','clk')
            b:signal('valid','i_go',1)
            b:signal('databundle','firstcol','i_go_firstcol',1)
            b:signal('databundle','blkx','i_go_blkx',MAX_BDIMBITS)
            b:signal('databundle','blkymax','i_go_blkymax',MAX_BDIMBITS)
            b:signal('databundle','blkxmin','i_go_blkxmin',MAX_BDIMBITS)
        b:rs_src('net_done','clk')
            b:signal('valid','o_done')
            b:signal('ready','i_done_ack')
        b:rs_src('rdreq','clk')
            b:signal('valid','o_rdreq')
            b:signal('ready','i_rdreq_ack')
            b:signal('databundle','blkx','o_rdreq_blkx',MAX_BDIMBITS)
            b:signal('databundle','blky','o_rdreq_blky',MAX_BDIMBITS)
            b:signal('databundle','whichbufs','o_rdreq_whichbufs',3)
            b:signal('databundle','whichpage','o_rdreq_whichpage',1)
            b:signal('address', 'o_rdreq_lpid', RDREQ_ADDR_WIDTH)
        b:rs_sink('rddone','clk')
            b:signal('valid','i_rddone')
            b:signal('databundle','whichbufs','i_rddone_whichbufs',3)
            b:signal('databundle','whichpage','i_rddone_whichpage',1)
        b:rs_src('pipe_go','clk')
            b:signal('valid','o_comp_go')
            b:signal('databundle','flush','o_comp_flush',1)
            b:signal('databundle','mode','o_comp_mode',2)
            b:signal('databundle','whichpage','o_comp_page',1)
        b:rs_sink('pipe_done','clk')
            b:signal('valid','i_comp_done')
        b:rs_src('wrreq_msg','clk')
            b:signal('valid','o_wrreq')
            b:signal('databundle','blkx','o_wrreq_blkx',MAX_BDIMBITS)
            b:signal('databundle','blky','o_wrreq_blky',MAX_BDIMBITS)
            b:signal('databundle','whichpage','o_wrreq_whichpage',1)
        b:rs_sink('wrdone','clk')
            b:signal('valid','i_wrdone')

    -- define Cache-to-Net component (one logical half of the Marshaller)
    -- when writing to memory, it has N_MEM possible memory controllers to write to
    local WRREQ_ADDR_WIDTH = math.max(1, util.clog2(N_MEM))
    b:component('cache_to_net',PERF_SIM and 'cpu_cache_to_net_dummy' or 'cpu_cache_to_net')
        b:clock_sink('clk')
        b:reset_sink('reset')
        -- TO OUTSIDE: wrreq
        b:rs_src('wrreq','clk')
            b:signal('databundle','data','o_net_wrreq_data',CACHE_DWIDTH)
            b:signal('databundle','blkx','o_net_wrreq_blkx',MAX_BDIMBITS)
            b:signal('databundle','blky','o_net_wrreq_blky',MAX_BDIMBITS)
			b:signal('databundle','sop','o_net_wrreq_sop',1)
            b:signal('valid','o_net_wrreq_valid')
            b:signal('ready','i_net_wrreq_ready')
            b:signal('eop','o_net_wrreq_eop')
            b:signal('address','o_net_wrreq_lpid',WRREQ_ADDR_WIDTH)
        
        if not PERF_SIM then
            -- TO CACHES: rdreq
            b:rs_src('rdreq','clk')
				b:signal('data','o_cache_rdreq_addr',CACHE_AWIDTH)
				b:signal('valid','o_cache_rdreq_valid')
				b:signal('ready','i_cache_rdreq_ready')
				b:signal('address','o_cache_rdreq_which',1)
            
            -- FROM CACHES: rdresp
            b:rs_sink('rdresp','clk')
				b:signal('data','i_cache_rdresp_data',CACHE_DWIDTH)
				b:signal('valid','i_cache_rdresp_valid')
				b:signal('ready','o_cache_rdresp_ready')
        end
        
        -- FROM MAIN: WRREQ message
        b:rs_sink('wrreq_msg','clk');
			b:signal('valid','i_msg_wrreq')
			b:signal('databundle','blkx','i_msg_wrreq_blkx',MAX_BDIMBITS)
			b:signal('databundle','blky','i_msg_wrreq_blky',MAX_BDIMBITS)
			b:signal('databundle','whichpage','i_msg_wrreq_whichpage',1)
        -- TO MAIN: WRDONE message 
        b:rs_src('wrdone','clk');
			b:signal('valid','o_msg_wrdone')
    
    -- Net-to-Cache component (the other logical half of the Marshaller)
    b:component('net_to_cache', PERF_SIM and 'cpu_net_to_cache_dummy' or 'cpu_net_to_cache')
        b:clock_sink('clk')
        b:reset_sink('reset')
        
        -- FROM OUTSIDE: rdresp
        b:rs_sink('rdresp','clk')
			b:signal('databundle','data','i_net_rdresp_data', CACHE_DWIDTH)
			b:signal('databundle','whichbufs','i_net_rdresp_whichbufs',3)
			b:signal('databundle','whichpage','i_net_rdresp_whichpage',1)
			b:signal('databundle','addr','i_net_rdresp_addr', CACHE_AWIDTH)
			b:signal('valid','i_net_rdresp_valid')
			b:signal('ready','o_net_rdresp_ready')
        
        if not PERF_SIM then
        -- TO CACHES: wrreq
            b:rs_src('wrreq','clk')
				b:signal('databundle','data','o_cache_wrreq_data',CACHE_DWIDTH)
				b:signal('databundle','addr','o_cache_wrreq_addr',CACHE_AWIDTH)
				b:signal('valid','o_cache_wrreq_valid')
				b:signal('ready','i_cache_wrreq_ready')
				b:signal('address','o_cache_wrreq_which',4)
        end

        -- TO MAIN: RDDONE message
        b:rs_src('rddone','clk')
			b:signal('valid','o_msg_rddone')
			b:signal('databundle','whichbufs','o_msg_rddone_whichbufs',3)
			b:signal('databundle','whichpage','o_msg_rddone_whichpage',1)
end
                                 
-- This function executes code that defines a Compute Element. The structure of the system is influenced by: 1) whether or not it is the special 0th compute element with the floating-point stuff and 2) how many memory controllers are in the outside system
function define_cpu(b, N_MEM, IS_BPU, sysname, PERF_SIM)
    -- Enumerate which Instances belong to which of the two clock domains. This simplifies hooking up the right
    -- clocks. 
    local pipe_clock_domain = Set.make{'top','left0','left1','cur0','cur1','pipe','pipe_ctrl'}
    local net_clock_domain = Set.make{'main_ctrl','n2c','c2n'}

    if PERF_SIM then
        pipe_clock_domain = Set.make{'pipe_ctrl'}
    end
    
    -- Create the actual CE system 
    b:system(sysname)
	b:int_param('IS_BPU', IS_BPU and '1' or '0')
	b:sys_param('DISABLE_BCAST')
	
	-- Create top-level clock and reset Interfaces
	b:clock_sink('clk_a')
	b:clock_sink('clk_b')
	b:reset_sink('reset')
	
	-- Instantiate cache buffers
	if not PERF_SIM then
		b:instance('cache','top')
		b:instance('cache','left0')
		b:instance('cache','left1')
		b:instance('cache','cur0')
		b:instance('cache','cur1')
	end
	
	-- Instantiate the Marshaller components
	b:instance('net_to_cache','n2c')
	b:instance('cache_to_net','c2n')
		b:int_param('N_MEM', N_MEM)
	
	-- Instantiate the Pipeline and Pipeline Control
	if not PERF_SIM then
		b:instance('pipe','pipe')
			b:lit_param('IS_BPU', 'IS_BPU')
			b:int_param('L_CACHE_RD_TOP', 'L_PIPE_TOP_RDREQ + '..CACHE_LATENCY..' + L_PIPE_TOP_RDRESP')
			b:int_param('L_CACHE_RD_LEFT', 'L_PIPE_LEFT_RDREQ + '..CACHE_LATENCY..' + L_PIPE_LEFT_RDRESP')
			b:int_param('L_CACHE_RD_CUR', 'L_PIPE_CUR_RDREQ + '..CACHE_LATENCY..' + L_PIPE_CUR_RDRESP')
	end
	
	b:instance('pipe_ctrl','pipe_ctrl')
		b:lit_param('IS_BPU', 'IS_BPU')
   
	-- Instantiate the Main Control block
	b:instance('main_ctrl','main_ctrl')
		b:lit_param('IS_BPU', 'IS_BPU')
		b:int_param('N_MEM', N_MEM)
		b:lit_param('DISABLE_BCAST', 'DISABLE_BCAST')
	
	-- Hook up clocks and resets to Domain A
	-- This only works because each instantiated component has exactly one clock interface named "clk".
	-- Similar for Reset.
	for inst in Set.values(pipe_clock_domain) do
		b:clock_link('clk_a', inst..'.clk')
		b:reset_link('reset', inst..'.reset')
	end

	for inst in Set.values(net_clock_domain) do
		b:clock_link('clk_b', inst..'.clk')
		b:reset_link('reset', inst..'.reset')
	end
	
	-- Export the given Interfaces to the top system level.
	-- Also automatically creates links.
	b:export('main_ctrl.rdreq', 'rdreq')
	b:export('n2c.rdresp', 'rdresp')
	b:export('main_ctrl.net_go', 'go')
	b:export('main_ctrl.net_done', 'done')
	b:export('c2n.wrreq', 'wrreq')
	
	if not PERF_SIM then
		--
		-- Links: Caches RDRESP
		--

		local clinks = {}
		
		-- Left Caches RDRESP to Pipeline. These are also mutually temporally exclusive.
		clinks.l0_pipe = b:rs_link('left0.rdresp','pipe.left_rdresp', 0)
		clinks.l1_pipe = b:rs_link('left1.rdresp','pipe.left_rdresp', 0)
		b:make_exclusive{clinks.l0_pipe, clinks.l1_pipe}
		
		-- Cur Caches RDRESP to Pipeline. Also mutually exclusive.
		clinks.c0_pipe = b:rs_link('cur0.rdresp','pipe.cur_rdresp', 0)
		clinks.c1_pipe = b:rs_link('cur1.rdresp','pipe.cur_rdresp', 0)
		b:make_exclusive{clinks.c0_pipe, clinks.c1_pipe}
		
		-- Top Cache RDRESP to Pipeline.
		clinks.t_pipe = b:rs_link('top.rdresp','pipe.top_rdresp', 0)
		
		-- Cur Caches RDRESP to Cache2Net.
		clinks.c0_net = b:rs_link('cur0.rdresp','c2n.rdresp', 1)
		clinks.c1_net = b:rs_link('cur1.rdresp','c2n.rdresp', 1)
		b:make_exclusive{clinks.c0_net, clinks.c1_net}

		-- Create latency queries for all Cache->Pipeline RDRESP queries
		b:latency_query(clinks.c0_pipe, 'L_PIPE_CUR_RDRESP');
		b:latency_query(clinks.l0_pipe, 'L_PIPE_LEFT_RDRESP');
		b:latency_query(clinks.t_pipe, 'L_PIPE_TOP_RDRESP');
		
		--
		-- Links: Caches RDREQ
		--
		
		-- Cache2Net RDREQ to Cur Caches
		clinks.netr_c0 = b:rs_link('c2n.rdreq','cur0.rdreq', 0, 1)
		clinks.netr_c1 = b:rs_link('c2n.rdreq','cur1.rdreq', 1, 1)
		
		-- Pipeline RDREQ to Cur Caches
		clinks.piper_cur0 = b:rs_link('pipe.cur_rdreq', 'cur0.rdreq', 0, 0)
		clinks.piper_cur1 = b:rs_link('pipe.cur_rdreq', 'cur1.rdreq', 1, 0)
		
		-- Pipeline RDREQ to Left Caches
		clinks.piper_left0 = b:rs_link('pipe.left_rdreq', 'left0.rdreq', 0)
		clinks.piper_left1 = b:rs_link('pipe.left_rdreq', 'left1.rdreq', 1)
		
		-- Pipeline RDREQ to Top Cache
		clinks.piper_top = b:rs_link('pipe.top_rdreq', 'top.rdreq')
		
		-- Make the RDREQ links competing for the Cur Caches temporally-exclusive
		b:make_exclusive{clinks.netr_c0, clinks.piper_cur0}
		b:make_exclusive{clinks.netr_c1, clinks.piper_cur1}
		
		-- Create latency queries for all Pipeline->Cache RDREQ paths
		b:latency_query(clinks.piper_cur0, 'L_PIPE_CUR_RDREQ')
		b:latency_query(clinks.piper_left0, 'L_PIPE_LEFT_RDREQ')
		b:latency_query(clinks.piper_top, 'L_PIPE_TOP_RDREQ')

		--
		-- Links: Caches WRREQ
		--
	
		-- Gather sets of links that compete for each cache buffer's write port. These will later be made exclusive.
		local wr_groups = 
		{
			['0'] = {}, ['1'] = {}
		}
		
		-- Connect all write request interfaces (from both the pipeline and from n2c) to the caches
		for source,v in pairs({["pipe.wrreq"]=pipe_wrreq_linkpoints, ["n2c.wrreq"]=n2c_wrreq_linkpoints}) do 
			for tuple,addr in pairs(v) do
				local whichpage = '0'
				local target
				local link
				--PARSE the linkpoint name 
				if string.find(tuple,'1') then
					whichpage = '1'
				end
				if string.find(tuple,'l') then
					target = 'left'..whichpage..'.wrreq'
					link = b:rs_link(source, target, addr)
					table.insert(wr_groups[whichpage], link)
				end
				if string.find(tuple,'t') then
					target = 'top'..'.wrreq'
					link = b:rs_link(source, target, addr)
					table.insert(wr_groups[whichpage], link)
				end
				if string.find(tuple,'c') then
					target = 'cur'..whichpage..'.wrreq'
					link = b:rs_link(source, target, addr)
					table.insert(wr_groups[whichpage], link)
				end
			end
		end
		
		-- exclusive-ize write requests
		for group in svaluesk(wr_groups) do
			b:make_exclusive(group)
		end
	end
	
	--
	-- Links: small leftover stuff
	--
	
	-- Cache2Net/Net2Cache communicating with Main Ctrl
	b:rs_link('n2c.rddone','main_ctrl.rddone')
	b:rs_link('c2n.wrdone','main_ctrl.wrdone')
	b:rs_link('main_ctrl.wrreq_msg','c2n.wrreq_msg')
	
	-- Pipeline Control communicating with Main Ctrl
	b:rs_link('pipe_ctrl.done','main_ctrl.pipe_done')
	b:rs_link('main_ctrl.pipe_go','pipe_ctrl.go')
	
	if not PERF_SIM then
		-- Pipeline <-> Pipeline Control conduit link
		b:conduit_link('pipe_ctrl.pipe','pipe.ctrl')
	end
end




