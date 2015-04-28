require 'topo_xbar'
require 'math' 

local CACHE_DWIDTH = 256
local BSIZEBITS = 6
local MAX_BDIMBITS = 8 
local CACHE_AWIDTH = BSIZEBITS+BSIZEBITS

function define_cpu(b, N_CPU, N_MEM, IS_BPU, sysname)
    b:component('cache','cpu_cache')
        b:clock_sink('clk','clk')
        b:reset_sink('reset','reset')
        b:rs_sink('wrreq','clk')
            b:signal('valid','i_wrreq_valid')
            b:signal('databundle','data','i_wrreq_data',CACHE_DWIDTH)
            b:signal('databundle','addr','i_wrreq_addr',CACHE_AWIDTH)
        b:rs_sink('rdreq','clk')
            b:signal('valid','i_rdreq_valid')
            b:signal('ready','o_rdreq_ready')
            b:signal('data','i_rdreq_addr',CACHE_AWIDTH)
            b:signal('lpid','i_rdreq_lpid',1)
            b:linkpoint('pipe',"1'b0",'broadcast')
            b:linkpoint('net',"1'b1",'broadcast')
        b:rs_src('rdresp','clk')
            b:signal('valid','o_rdresp_valid')
            b:signal('ready','i_rdresp_ready')
            b:signal('data','o_rdresp_data', CACHE_DWIDTH)
            b:signal('lpid','o_rdresp_lpid',1)
            b:linkpoint('pipe',"1'b0",'broadcast')
            b:linkpoint('net',"1'b1",'broadcast')
    
    local sigs_ctrl_to_pipe = Set.make{'k_reset','k_inc','i_reset','i_inc',
                'i_load_k1','j_reset','j_inc','j_load_k','j_load_k1',
                'valid','norm','recip','wr_top','wr_left','wr_cur','whichpage'}
                
    local sigs_pipe_to_ctrl = Set.make{'k_done','i_done','j_done','pipe_empty'}

    local pipe_wrreq_linkpoints = {tcl0="4'b1110", cl0="4'b0110", tc0="4'b1100", c0="4'b0100", cl1="4'b0111", c1="4'b0101"};

    b:component('pipe','cpu_pipeline')
        b:parameter('IS_BPU')
        b:clock_sink('clk','clk')
        b:reset_sink('reset','reset')
        b:conduit_sink('ctrl')
            for sig in Set.values(sigs_ctrl_to_pipe) do
                b:signal('fwd',sig,'i_' .. sig,1)
            end
            for sig in Set.values(sigs_pipe_to_ctrl) do
                b:signal('rev',sig,'o_' .. sig,1)
            end
        for i,cache in ipairs{'top','left','cur'} do
            local iface = cache .. "_rdreq"
            local isig = 'i_' .. iface
            local osig = 'o_' .. iface
            local istop = cache == 'top'
            b:rs_src(iface,'clk')
                b:signal('valid',osig .. '_valid')
                b:signal('ready',isig .. '_ready')
                b:signal('data',osig .. '_addr',CACHE_AWIDTH)
                if not istop then
                    b:signal('lpid',osig .. '_lpid',1)
                    b:linkpoint('buf0',"1'b0",'broadcast')
                    b:linkpoint('buf1',"1'b1",'broadcast')
                end
            iface = cache .. "_rdresp"
            isig = 'i_' .. iface
            osig = 'o_' .. iface
            b:rs_sink(iface,'clk')
                b:signal('valid',isig .. '_valid')
                b:signal('ready',osig .. '_ready')
                b:signal('data',isig .. '_data',CACHE_DWIDTH)
        end
        b:rs_src('wrreq','clk')
            b:signal('valid','o_wr_valid')
            b:signal('ready','i_wr_ready')
            b:signal('databundle','data','o_wr_data',CACHE_DWIDTH)
            b:signal('databundle','addr','o_wr_addr',CACHE_AWIDTH)
            b:signal('lpid','o_wr_lpid',4)
            for name, encoding in pairs(pipe_wrreq_linkpoints) do 
                b:linkpoint(name,encoding,'broadcast') 
            end

    b:component('pipe_ctrl','cpu_pipe_ctrl')
        b:parameter('IS_BPU')
        b:clock_sink('clk','clk')
        b:reset_sink('reset','reset')
        b:rs_sink('go','clk')
            b:signal('valid','i_go')
            b:signal('databundle','flush','i_flush',1)
            b:signal('databundle','mode','i_mode',2)
            b:signal('databundle','whichpage','i_whichpage',1)
        b:rs_src('done','clk')
            b:signal('valid','o_done')
        b:conduit_src('pipe')
            for sig in Set.values(sigs_ctrl_to_pipe) do
                b:signal('fwd',sig,'o_' .. sig,1)
            end
            for sig in Set.values(sigs_pipe_to_ctrl) do
                b:signal('rev',sig,'i_' .. sig,1)
            end
            
    b:component('main_ctrl','cpu_main_ctrl')
        b:clock_sink('clk','clk_b')
        b:reset_sink('reset','reset')
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
            b:signal('lpid','o_rdreq_toctrl',1)
            b:signal('databundle','blkx','o_rdreq_blkx',MAX_BDIMBITS)
            b:signal('databundle','blky','o_rdreq_blky',MAX_BDIMBITS)
            b:signal('databundle','whichbufs','o_rdreq_whichbufs',3)
            b:signal('databundle','whichpage','o_rdreq_whichpage',1)
            b:linkpoint('mem',"1'b0",'broadcast')
            b:linkpoint('ctrl',"1'b1",'broadcast')
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

    b:component('cache_to_net','cpu_cache_to_net')
        b:clock_sink('clk','clk')
        b:reset_sink('reset','reset')
        -- TO OUTSIDE: wrreq
        b:rs_src('wrreq','clk')
            b:signal('databundle','data','o_net_wrreq_data',CACHE_DWIDTH)
            b:signal('databundle','x','o_net_wrreq_x',MAX_BDIMBITS)
            b:signal('databundle','y','o_net_wrreq_y',MAX_BDIMBITS)
            b:signal('valid','o_net_wrreq_valid')
            b:signal('ready','i_net_wrreq_ready')
            b:signal('eop','o_net_wrreq_eop')
            b:signal('sop','o_net_wrreq_sop')
        
        -- TO CACHES: rdreq
        b:rs_src('rdreq','clk')
                b:signal('data','o_cache_rdreq_addr',CACHE_AWIDTH)
                b:signal('valid','o_cache_rdreq_valid')
                b:signal('ready','i_cache_rdreq_ready')
                b:signal('lpid','o_cache_rdreq_which',2)
                b:linkpoint('c0',"1'b0",'broadcast');
                b:linkpoint('c1',"1'b1",'broadcast');
        
        -- FROM CACHES: rdresp
        b:rs_sink('rdresp','clk')
                b:signal('data','i_cache_rdresp_data',CACHE_DWIDTH)
                b:signal('valid','i_cache_rdresp_valid')
                b:signal('ready','o_cache_rdresp_ready')
        
        -- FROM MAIN: WRREQ message
        b:rs_sink('wrreq_msg','clk');
                b:signal('valid','i_msg_wrreq')
                b:signal('databundle','blkx','i_msg_wrreq_blkx',MAX_BDIMBITS)
                b:signal('databundle','blky','i_msg_wrreq_blky',MAX_BDIMBITS)
                b:signal('databundle','whichpage','i_msg_wrreq_whichpage',1)
        -- TO MAIN: WRDONE message 
        b:rs_src('wrdone','clk');
                b:signal('valid','o_msg_wrdone')

    local n2c_wrreq_linkpoints = {cl0="4'b0110", tcl0="4'b1110", tc0="4'b1100", c0="4'b0100", l0="4'b0010", 
                                     tcl1="4'b1111", cl1="4'b0111", c1="4'b0101", l1="4'b0011"};

    b:component('net_to_cache','cpu_net_to_cache')
        b:clock_sink('clk','clk')
        b:reset_sink('reset','reset')
        
        -- FROM OUTSIDE: rdresp
        b:rs_sink('rdresp','clk')
                b:signal('databundle','data','i_net_rdresp_data', CACHE_DWIDTH)
                b:signal('databundle','whichbufs','i_net_rdresp_whichbufs',3)
                b:signal('databundle','whichpage','i_net_rdresp_whichpage',1)
                b:signal('valid','i_net_rdresp_valid')
                b:signal('sop','i_net_rdresp_sop')
                b:signal('eop','i_net_rdresp_eop')
                b:signal('ready','o_net_rdresp_ready')
        
        -- TO CACHES: wrreq
        b:rs_src('wrreq','clk')
                b:signal('databundle','data','o_cache_wrreq_data',CACHE_DWIDTH)
                b:signal('databundle','addr','o_cache_wrreq_addr',CACHE_AWIDTH)
                b:signal('valid','o_cache_wrreq_valid')
                b:signal('ready','i_cache_wrreq_ready')
                b:signal('lpid','o_cache_wrreq_which',4)
                for name, encoding in pairs(n2c_wrreq_linkpoints) do 
                    b:linkpoint(name,encoding,'broadcast') 
                end

        -- TO MAIN: RDDONE message
        b:rs_src('rddone','clk')
                b:signal('valid','o_msg_rddone')
                b:signal('databundle','whichbufs','o_msg_rddone_whichbufs',3)
                b:signal('databundle','whichpage','o_msg_rddone_whichpage',1)

    local pipe_clock_domain = Set.make{'top','left0','left1','cur0','cur1','pipe','pipe_ctrl'}
    local net_clock_domain = Set.make{'main_ctrl','n2c','c2n'}

    b:system(sysname, topo_xbar)
        b:parameter('IS_BPU')
        
        b:clock_sink('clk_a', 'clk_a')
        b:clock_sink('clk_b', 'clk_b')
        b:reset_sink('reset', 'reset')
        
        b:instance('top','cache')
        b:instance('left0','cache')
        b:instance('left1','cache')
        b:instance('cur0','cache')
        b:instance('cur1','cache')
        b:instance('n2c','net_to_cache')
        b:instance('c2n','cache_to_net')
        b:instance('pipe','pipe')
            b:parameter('IS_BPU', 'IS_BPU')
        b:instance('pipe_ctrl','pipe_ctrl')
            b:parameter('IS_BPU', 'IS_BPU')
        b:instance('main_ctrl','main_ctrl')
        
        for inst in Set.values(pipe_clock_domain) do
            b:link('clk_a', inst..'.clk')
            b:link('reset', inst..'.reset')
        end

        for inst in Set.values(net_clock_domain) do
            b:link('clk_b', inst..'.clk')
            b:link('reset', inst..'.reset')
        end
        
        b:export('main_ctrl.rdreq', 'rdreq')
        b:export('n2c.rdresp', 'rdresp')
        b:export('main_ctrl.net_go', 'go')
        b:export('main_ctrl.net_done', 'done')
        b:export('c2n.wrreq', 'wrreq')
        --b:export('main_ctrl.rdreq.ctrl','rs','out')
        
        -- Feature: Implicitly connect interfaces of the same name? 
        -- Intra-CE links, ordered by instantiation order & source 
        -- Cache Sources
        b:link('left0.rdresp.pipe','pipe.left_rdresp','l0_pipe')
        b:link('left1.rdresp.pipe','pipe.left_rdresp','l1_pipe')
        b:make_exclusive_by_labels{'l0_pipe', 'l1_pipe'}
        
        b:link('cur0.rdresp.pipe','pipe.cur_rdresp','c0_pipe')
        b:link('cur0.rdresp.net','c2n.rdresp','c0_net')
        b:link('cur1.rdresp.pipe','pipe.cur_rdresp','c1_pipe')
        b:link('cur1.rdresp.net','c2n.rdresp','c1_net')
        b:make_exclusive_by_labels{'c0_pipe', 'c1_pipe'}
        b:make_exclusive_by_labels{'c0_net', 'c1_net'}

        b:link('top.rdresp.pipe','pipe.top_rdresp', 't_pipe')
        
        --Network interfaces
        b:link('n2c.rddone','main_ctrl.rddone')

        b:link('c2n.rdreq.c0','cur0.rdreq.net','netr_c0')
        b:link('c2n.rdreq.c1','cur1.rdreq.net','netr_c1')
        b:link('c2n.wrdone','main_ctrl.wrdone')

        --this bundles up wrreq to caches from net and mem due to 
        --the sheer number and duplication of linkpoints 
    
        local wr_groups = 
        {
            ['left0.wrreq'] = {}, ['left1.wrreq'] = {}, ['top.wrreq'] = {}, ['cur0.wrreq'] = {}, ['cur1.wrreq'] = {}
        }
    
        for source,v in pairs({["pipe.wrreq."]=pipe_wrreq_linkpoints, ["n2c.wrreq."]=n2c_wrreq_linkpoints}) do 
            for tuple in keys(v) do
                local whichpage = '0'
                local target
                local link
                --PARSE the linkpoint name 
                if string.find(tuple,'1') then
                    whichpage = '1'
                end
                if string.find(tuple,'l') then
                    target = 'left'..whichpage..'.wrreq'
                    link = b:link(source..tuple, target, label)
                    table.insert(wr_groups[target], link)
                end
                if string.find(tuple,'t') then
                    target = 'top'..'.wrreq'
                    link = b:link(source..tuple, target, label)
                    table.insert(wr_groups[target], link)
                end
                if string.find(tuple,'c') then
                    target = 'cur'..whichpage..'.wrreq'
                    link = b:link(source..tuple, target, label)
                    table.insert(wr_groups[target], link)
                end
            end
        end
        
        -- Pipeline 
        for i,cache in pairs{'left','cur'} do
            b:link('pipe.'..cache..'_rdreq.buf0',cache..'0.rdreq.pipe','piper_'..cache..'0')
            b:link('pipe.'..cache..'_rdreq.buf1',cache..'1.rdreq.pipe','piper_'..cache..'1')
        end
        b:link('pipe.top_rdreq', 'top.rdreq.pipe', 'piper_t')
        
        -- exclusive-ize read requests
        b:make_exclusive_by_labels{'netr_c0', 'piper_cur0'}
        b:make_exclusive_by_labels{'netr_c1', 'piper_cur1'}
        
        -- exclusive-ize write requests
        for group in svaluesk(wr_groups) do
            b:make_exclusive(group)
        end
        
        -- Pipeline control
        b:link('pipe_ctrl.done','main_ctrl.pipe_done')
        b:link('pipe_ctrl.pipe','pipe.ctrl')

        -- Main control 
        b:link('main_ctrl.pipe_go','pipe_ctrl.go')
        b:link('main_ctrl.wrreq_msg','c2n.wrreq_msg')
end




