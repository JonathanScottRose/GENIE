require 'spec'
require 'topo_xbar'
require 'math' 

local CACHE_DWIDTH = 256
local BSIZEBITS = 6
local MAX_BDIMBITS = 8 
local CACHE_AWIDTH = BSIZEBITS+BSIZEBITS
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
local pipe_wrreq_linkpoints = {tcl0="4'b1110", cl0="4'b0110", tc0="4'b1100", c0="4'b0100", cl1="4'b0111", c1="4'b0101"};

-- used to autmate the creation of the various linkpoints on the Net-to-Cache block to let it simultaneously target
-- all the combinations of cache buffers for writing
local n2c_wrreq_linkpoints = {cl0="4'b0110", tcl0="4'b1110", tc0="4'b1100", c0="4'b0100", l0="4'b0010", 
                                 tcl1="4'b1111", cl1="4'b0111", c1="4'b0101", l1="4'b0011"};

                                 
-- Function which executes code that defines a Compute Element, given the number of total Compute Elements in the system,
-- number of memory controllers in the system, and whether or not this CE is the special unique version that needs
-- a floating-point divider in its pipeline.
function define_cpu(b, N_CPU, N_MEM, IS_BPU, sysname)
    -- define cache component
    b:component('cache','cpu_cache')
        b:clock_sink('clk','clk')
        b:reset_sink('reset','reset')
        b:interface('wrreq','data','in','clk')
            b:signal('valid','i_wrreq_valid')
            b:signal('data','i_wrreq_data',CACHE_DWIDTH,'data')
            b:signal('data','i_wrreq_addr',CACHE_AWIDTH,'addr')
        b:interface('rdreq','data','in','clk')
            b:signal('valid','i_rdreq_valid')
            b:signal('ready','o_rdreq_ready')
            b:signal('data','i_rdreq_addr',CACHE_AWIDTH)
            b:signal('lp_id','i_rdreq_lp_id',1)
            b:linkpoint('pipe',"1'b0",'broadcast')
            b:linkpoint('net',"1'b1",'broadcast')
        b:interface('rdresp','data','out','clk')
            b:signal('valid','o_rdresp_valid')
            b:signal('ready','i_rdresp_ready')
            b:signal('data','o_rdresp_data', CACHE_DWIDTH)
            b:signal('lp_id','o_rdresp_lp_id',1)
            b:linkpoint('pipe',"1'b0",'broadcast')
            b:linkpoint('net',"1'b1",'broadcast')

    -- define the Pipeline component
    b:component('pipe','cpu_pipeline')
        b:parameter('IS_BPU')
        b:parameter('L_CACHE_RD_TOP')
        b:parameter('L_CACHE_RD_LEFT')
        b:parameter('L_CACHE_RD_CUR')
        b:clock_sink('clk','clk')
        b:reset_sink('reset','reset')
        b:interface('ctrl','conduit','in')
            -- use the list of signals base-names defined earlier to automate this part
            for sig in Set.values(sigs_ctrl_to_pipe) do
                b:signal('out','i_' .. sig,1,sig)
            end
            for sig in Set.values(sigs_pipe_to_ctrl) do
                b:signal('in','o_' .. sig,1,sig)
            end
        for cache in values{'top','left','cur'} do
            -- this code creates a read request and read response interface for each cache block type.
            -- create the read request interface first.
            local iface = cache .. "_rdreq"
            local isig = 'i_' .. iface
            local osig = 'o_' .. iface
            local istop = cache == 'top'
            b:interface(iface,'data','out','clk')
                b:signal('valid',osig .. '_valid')
                b:signal('ready',isig .. '_ready')
                b:signal('data',osig .. '_addr',CACHE_AWIDTH)
                if not istop then
                    -- left/cur (but not top) ports need to have two linkpoints so they can address both buffers of
                    -- those caches
                    b:signal('lp_id',osig .. '_lp_id',1)
                    b:linkpoint('buf0',"1'b0",'broadcast')
                    b:linkpoint('buf1',"1'b1",'broadcast')
                end
            -- read interface
            iface = cache .. "_rdresp"
            isig = 'i_' .. iface
            osig = 'o_' .. iface
            b:interface(iface,'data','in','clk')
                b:signal('valid',isig .. '_valid')
                b:signal('ready',osig .. '_ready')
                b:signal('data',isig .. '_data',CACHE_DWIDTH)
        end
        -- write request interface. there's only one of these but it has a variety of linkpoints so it can 
        -- simultaneously target various combinations of the 5 possible cache buffers.
        b:interface('wrreq','data','out','clk')
            b:signal('valid','o_wr_valid')
            b:signal('ready','i_wr_ready')
            b:signal('data','o_wr_data',CACHE_DWIDTH,'data')
            b:signal('data','o_wr_addr',CACHE_AWIDTH,'addr')
            b:signal('lp_id','o_wr_lp_id',4)
            for name,encoding in pairs(pipe_wrreq_linkpoints) do 
                b:linkpoint(name,encoding,'broadcast') 
            end

    -- define Pipeline Control component
    b:component('pipe_ctrl','cpu_pipe_ctrl')
        b:parameter('IS_BPU')
        b:clock_sink('clk','clk')
        b:reset_sink('reset','reset')
        b:interface('go', 'data','in','clk')
            b:signal('valid','i_go')
            b:signal('data','i_flush','1','flush')
            b:signal('data','i_mode','2','mode')
            b:signal('data','i_whichpage','1','whichpage')
        b:interface('done', 'data','out','clk')
            b:signal('valid','o_done')
        b:interface('pipe','conduit','out')
            for sig in Set.values(sigs_ctrl_to_pipe) do
                b:signal('out','o_' .. sig,1,sig)
            end
            for sig in Set.values(sigs_pipe_to_ctrl) do
                b:signal('in','i_' .. sig,1,sig)
            end
    
    -- define Main Control component
    b:component('main_ctrl','cpu_main_ctrl')
        b:clock_sink('clk','clk_b')
        b:reset_sink('reset','reset')
        b:interface('net_go', 'data','in','clk')
            b:signal('valid','i_go','1')
            b:signal('data','i_go_firstcol','1','firstcol')
            b:signal('data','i_go_blkx',MAX_BDIMBITS,'blkx')
            b:signal('data','i_go_blkymax',MAX_BDIMBITS,'blkymax')
            b:signal('data','i_go_blkxmin',MAX_BDIMBITS,'blkxmin')
        b:interface('net_done','data','out','clk')
            b:signal('valid','o_done')
            b:signal('ready','i_done_ack')
        b:interface('rdreq','data','out','clk')
            b:signal('valid','o_rdreq')
            b:signal('ready','i_rdreq_ack')
            b:signal('lp_id','o_rdreq_toctrl','1')
            b:signal('data','o_rdreq_blkx',MAX_BDIMBITS,'blkx')
            b:signal('data','o_rdreq_blky',MAX_BDIMBITS,'blky')
            b:signal('data','o_rdreq_whichbufs','3','whichbufs')
            b:signal('data','o_rdreq_whichpage','1','whichpage')
            b:linkpoint('mem',"1'b0",'broadcast')
            b:linkpoint('ctrl',"1'b1",'broadcast')
        b:interface('rddone','data','in','clk')
            b:signal('valid','i_rddone')
            b:signal('data','i_rddone_whichbufs','3','whichbufs')
            b:signal('data','i_rddone_whichpage','1','whichpage')
        b:interface('pipe_go','data','out','clk')
            b:signal('valid','o_comp_go')
            b:signal('data','o_comp_flush','1','flush')
            b:signal('data','o_comp_mode','2','mode')
            b:signal('data','o_comp_page','1','whichpage')
        b:interface('pipe_done', 'data','in','clk')
            b:signal('valid','i_comp_done')
        b:interface('wrreq_msg', 'data','out','clk')
            b:signal('valid','o_wrreq')
            b:signal('data','o_wrreq_blkx',MAX_BDIMBITS,'blkx')
            b:signal('data','o_wrreq_blky',MAX_BDIMBITS,'blky')
            b:signal('data','o_wrreq_whichpage','1','whichpage')
        b:interface('wrdone', 'data','in','clk')
            b:signal('valid','i_wrdone')

    -- define Cache-to-Net component (one logical half of the Marshaller)
    b:component('cache_to_net','cpu_cache_to_net')
        b:clock_sink('clk','clk')
        b:reset_sink('reset','reset')
        -- TO OUTSIDE: wrreq
        b:interface('wrreq','data','out','clk')
            b:signal('data','o_net_wrreq_data',CACHE_DWIDTH,'data')
            b:signal('data','o_net_wrreq_x',MAX_BDIMBITS,'x')
            b:signal('data','o_net_wrreq_y',MAX_BDIMBITS,'y')
            b:signal('valid','o_net_wrreq_valid')
            b:signal('ready','i_net_wrreq_ready')
            b:signal('eop','o_net_wrreq_eop')
            b:signal('sop','o_net_wrreq_sop')
        
        -- TO CACHES: rdreq
        b:interface('rdreq', 'data', 'out', 'clk')
                b:signal('data','o_cache_rdreq_addr',CACHE_AWIDTH)
                b:signal('valid','o_cache_rdreq_valid')
                b:signal('ready','i_cache_rdreq_ready')
                b:signal('lp_id','o_cache_rdreq_which','2')
                b:linkpoint('c0',"1'b0",'broadcast');
                b:linkpoint('c1',"1'b1",'broadcast');
        
        -- FROM CACHES: rdresp
        b:interface('rdresp', 'data', 'in', 'clk')
                b:signal('data','i_cache_rdresp_data',CACHE_DWIDTH)
                b:signal('valid','i_cache_rdresp_valid')
                b:signal('ready','o_cache_rdresp_ready')
        
        -- FROM MAIN: WRREQ message
        b:interface('wrreq_msg', 'data', 'in', 'clk');
                b:signal('valid','i_msg_wrreq')
                b:signal('data','i_msg_wrreq_blkx',MAX_BDIMBITS,'blkx')
                b:signal('data','i_msg_wrreq_blky',MAX_BDIMBITS,'blky')
                b:signal('data','i_msg_wrreq_whichpage','1','whichpage')
        -- TO MAIN: WRDONE message 
        b:interface('wrdone', 'data', 'out', 'clk');
                b:signal('valid','o_msg_wrdone')

    -- Net-to-Cache component (the other logical half of the Marshaller)
    b:component('net_to_cache','cpu_net_to_cache')
        b:clock_sink('clk','clk')
        b:reset_sink('reset','reset')
        
        -- FROM OUTSIDE: rdresp
        b:interface('rdresp', 'data', 'in', 'clk')
                b:signal('data','i_net_rdresp_data', CACHE_DWIDTH,'data')
                b:signal('data','i_net_rdresp_whichbufs','3','whichbufs')
                b:signal('data','i_net_rdresp_whichpage','1','whichpage')
                b:signal('valid','i_net_rdresp_valid')
                b:signal('sop','i_net_rdresp_sop')
                b:signal('eop','i_net_rdresp_eop')
                b:signal('ready','o_net_rdresp_ready')
        
        -- TO CACHES: wrreq
        b:interface('wrreq', 'data', 'out', 'clk')
                b:signal('data','o_cache_wrreq_data',CACHE_DWIDTH,'data')
                b:signal('data','o_cache_wrreq_addr',CACHE_AWIDTH,'addr')
                b:signal('valid','o_cache_wrreq_valid')
                b:signal('ready','i_cache_wrreq_ready')
                b:signal('lp_id','o_cache_wrreq_which',4)
                for name, encoding in pairs(n2c_wrreq_linkpoints) do 
                    b:linkpoint(name,encoding,'broadcast') 
                end

        -- TO MAIN: RDDONE message
        b:interface('rddone', 'data', 'out', 'clk')
                b:signal('valid','o_msg_rddone')
                b:signal('data','o_msg_rddone_whichbufs',3,'whichbufs')
                b:signal('data','o_msg_rddone_whichpage',1,'whichpage')

    -- Enumerate which Instances belong to which of the two clock domains. This simplifies hooking up the right
    -- clocks. 
    local pipe_clock_domain = Set.make{'top','left0','left1','cur0','cur1','pipe','pipe_ctrl'}
    local net_clock_domain = Set.make{'main_ctrl','n2c','c2n'}

    -- Create the actual CE system (use a crossbar toplogy for now)
    b:system(sysname, topo_xbar)
        b:parameter('IS_BPU')
        
        -- Instantiate cache buffers
        b:instance('top','cache')
        b:instance('left0','cache')
        b:instance('left1','cache')
        b:instance('cur0','cache')
        b:instance('cur1','cache')
        
        -- Instantiate the Marshaller components
        b:instance('n2c','net_to_cache')
        b:instance('c2n','cache_to_net')
        
        -- Instantiate the Pipeline and Pipeline Control
        b:instance('pipe','pipe')
            b:defparam('IS_BPU', 'IS_BPU')
            b:defparam('L_CACHE_RD_TOP', 'L_PIPE_TOP_RDREQ + '..CACHE_LATENCY..' + L_PIPE_TOP_RDRESP')
            b:defparam('L_CACHE_RD_LEFT', 'L_PIPE_LEFT_RDREQ + '..CACHE_LATENCY..' + L_PIPE_LEFT_RDRESP')
            b:defparam('L_CACHE_RD_CUR', 'L_PIPE_CUR_RDREQ + '..CACHE_LATENCY..' + L_PIPE_CUR_RDRESP')
        b:instance('pipe_ctrl','pipe_ctrl')
            b:defparam('IS_BPU', 'IS_BPU')
            
        -- Instantiate the Main Control block
        b:instance('main_ctrl','main_ctrl')

        -- Create top-level clock and reset Interfaces
        b:export('clk_a','clock','in')
        b:export('clk_b','clock','in')
        b:export('reset','reset','in')

        -- Export the given Interfaces to the top system level.
        -- Also automatically creates links.
        b:export_if('rdreq','main_ctrl.rdreq')
        b:export_if('rdresp','n2c.rdresp')
        b:export_if('go','main_ctrl.net_go')
        b:export_if('done','main_ctrl.net_done')
        b:export_if('wrreq','c2n.wrreq')
        
        -- Hook up clocks and resets to Domain A
        -- This only works because each instantiated component has exactly one clock interface named "clk".
        -- Similar for Reset.
        for inst in Set.values(pipe_clock_domain) do
            b:link('clk_a', inst..'.clk')
            b:link('reset', inst..'.reset')
        end

        -- Hook up clocks and resets to Domain B
        for inst in Set.values(net_clock_domain) do
            b:link('clk_b', inst..'.clk')
            b:link('reset', inst..'.reset')
        end
        
        --
        -- Links: Caches RDRESP
        --
                
        -- Left Caches RDRESP to Pipeline. These are also mutually temporally exclusive.
        b:link('left0.rdresp.pipe','pipe.left_rdresp','l0_pipe')
        b:link('left1.rdresp.pipe','pipe.left_rdresp','l1_pipe')
        b:make_exclusive{'l0_pipe', 'l1_pipe'}
        
        -- Cur Caches RDRESP to Pipeline. Also mutually exclusive.
        b:link('cur0.rdresp.pipe','pipe.cur_rdresp','c0_pipe')
        b:link('cur1.rdresp.pipe','pipe.cur_rdresp','c1_pipe')
        b:make_exclusive{'c0_pipe', 'c1_pipe'}
        
        -- Top Cache RDRESP to Pipeline.
        b:link('top.rdresp.pipe','pipe.top_rdresp', 't_pipe')
        
        -- Cur Caches RDRESP to Cache2Net.
        b:link('cur0.rdresp.net','c2n.rdresp','c0_net')
        b:link('cur1.rdresp.net','c2n.rdresp','c1_net')
        b:make_exclusive{'c0_net', 'c1_net'}
        
        -- Create latency queries for all Cache->Pipeline RDRESP queries
        b:latency_query('c0_pipe', 'L_PIPE_CUR_RDRESP');
        b:latency_query('l0_pipe', 'L_PIPE_LEFT_RDRESP');
        b:latency_query('t_pipe', 'L_PIPE_TOP_RDRESP');

        --
        -- Links: Caches RDREQ
        --
        
        -- Cache2Net RDREQ to Cur Caches
        b:link('c2n.rdreq.c0','cur0.rdreq.net','netr_c0')
        b:link('c2n.rdreq.c1','cur1.rdreq.net','netr_c1')
        
        -- Pipeline RDREQ to Top+Left Caches
        for cache in values{'left','cur'} do
            b:link('pipe.'..cache..'_rdreq.buf0',cache..'0.rdreq.pipe','piper_'..cache..'0')
            b:link('pipe.'..cache..'_rdreq.buf1',cache..'1.rdreq.pipe','piper_'..cache..'1')
        end
        
        -- Pipeline RDREQ to Top Cache
        b:link('pipe.top_rdreq', 'top.rdreq.pipe', 'piper_top')
        
        -- Make the RDREQ links competing for the Cur Caches temporally-exclusive
        b:make_exclusive{'netr_c0', 'piper_cur0'}
        b:make_exclusive{'netr_c1', 'piper_cur1'}
        
        -- Create latency queries for all Pipeline->Cache RDREQ paths
        b:latency_query('piper_cur0', 'L_PIPE_CUR_RDREQ')
        b:latency_query('piper_left0', 'L_PIPE_LEFT_RDREQ')
        b:latency_query('piper_top', 'L_PIPE_TOP_RDREQ')
        
        --
        -- Links: Caches WRREQ
        --
        
        -- Gather sets of links that compete for each cache buffer's write port. These will later be made exclusive.
        local wr_groups = 
        {
            ['left0.wrreq'] = {}, ['left1.wrreq'] = {}, ['top.wrreq'] = {}, ['cur0.wrreq'] = {}, ['cur1.wrreq'] = {}
        }
    
        -- Connect all write request interfaces (from both the pipeline and from n2c) to the caches
        for source,v in pairs({["pipe.wrreq."]=pipe_wrreq_linkpoints, ["n2c.wrreq."]=n2c_wrreq_linkpoints}) do 
            for tuple in keys(v) do
                local whichpage = '0'
                local target
                local label
                --PARSE the linkpoint name 
                if string.find(tuple,'1') then
                    whichpage = '1'
                end
                if string.find(tuple,'l') then
                    target = 'left'..whichpage..'.wrreq'
                    label = source..tuple..target
                    b:link(source..tuple, target, label)
                    table.insert(wr_groups[target], label)
                end
                if string.find(tuple,'t') then
                    target = 'top'..'.wrreq'
                    label = source..tuple..target
                    b:link(source..tuple, target, label)
                    table.insert(wr_groups[target], label)
                end
                if string.find(tuple,'c') then
                    target = 'cur'..whichpage..'.wrreq'
                    label = source..tuple..target
                    b:link(source..tuple, target, label)
                    table.insert(wr_groups[target], label)
                end
            end
        end
        
        -- exclusive-ize write requests
        for group in svaluesk(wr_groups) do
            b:make_exclusive(group)
        end
        
        --
        -- Links: small leftover stuff
        --
        
        -- Cache2Net/Net2Cache communicating with Main Ctrl
        b:link('n2c.rddone','main_ctrl.rddone')
        b:link('c2n.wrdone','main_ctrl.wrdone')
        b:link('main_ctrl.wrreq_msg','c2n.wrreq_msg')
        
        -- Pipeline Control communicating with Main Ctrl
        b:link('pipe_ctrl.done','main_ctrl.pipe_done')
        b:link('main_ctrl.pipe_go','pipe_ctrl.go')
        
        -- Pipeline <-> Pipeline Control conduit link
        b:link('pipe_ctrl.pipe','pipe.ctrl')   
end




