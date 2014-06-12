local CACHE_DWIDTH = 256
local BSIZEBITS = 8
local MAX_BDIMBITS = 8 
local CACHE_AWIDTH = BSIZEBITS*BSIZEBITS
local LANES = 8 
---[[
b:component('cache','cpu_cache')
    b:clock_sink('clk','clk')
    b:reset_sink('reset','reset')
    b:interface('wrreq','data','in','clk')
        b:signal('valid','i_wrreq_valid')
        b:signal('data','i_wrreq_data',CACHE_DWIDTH,'data')
        b:signal('data','i_wrreq_addr',CACHE_AWIDTH,'addr')
        --b:signal('data','i_wrreq_enable',LANES,'enable')
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
---[[
local sigs_ctrl_to_pipe = Set.make{'k_reset','k_inc','i_reset','i_inc',
            'i_load_k1','j_reset','j_inc','j_load_k','j_load_k1',
            'valid','norm','recip','wr_top','wr_left','wr_cur','whichpage'}
local sigs_pipe_to_ctrl = Set.make{'k_done','i_done','j_done','pipe_empty'}
---[[        
b:component('pipe','cpu_pipeline')
    b:clock_sink('clk','clk')
    b:reset_sink('reset','reset')
    b:interface('ctrl','conduit','in')
        for sig in Set.values(sigs_ctrl_to_pipe) do
            b:signal('out','i_' .. sig,1,sig)
        end
        for sig in Set.values(sigs_pipe_to_ctrl) do
            b:signal('in','o_' .. sig,1,sig)
        end
    for i,cache in ipairs{'top','left','cur'} do
        local iface = cache .. "_rdreq"
        local isig = 'i_' .. iface
        local osig = 'o_' .. iface
        local istop = cache == 'top'
        b:interface(iface,'data','out','clk')
            b:signal('valid',osig .. '_valid')
            b:signal('ready',isig .. '_ready')
            b:signal('data',osig .. '_addr',CACHE_AWIDTH)
            if not istop then
                b:signal('lp_id',osig .. '_lp_id',1)
                b:linkpoint('buf0',"1'b0",'broadcast')
                b:linkpoint('buf1',"1'b1",'broadcast')
            end
        iface = cache .. "_rdresp"
        isig = 'i_' .. iface
        osig = 'o_' .. iface
        b:interface(iface,'data','in','clk')
            b:signal('valid',isig .. '_valid')
            b:signal('ready',osig .. '_ready')
            b:signal('data',isig .. '_data',CACHE_DWIDTH)
    end
    b:interface('wrreq','data','out','clk')
        b:signal('valid','o_wr_valid')
        b:signal('ready','i_wr_ready')
        b:signal('data','o_wr_data',CACHE_DWIDTH,'data')
        b:signal('data','o_wr_addr',CACHE_AWIDTH,'addr')
        b:signal('lp_id','o_wr_lp_id',4)
        b:linkpoint('tcl0',"4'b1110",'broadcast')
        b:linkpoint('cl0',"4'b0110",'broadcast')
        b:linkpoint('tc0',"4'b1100",'broadcast')
        b:linkpoint('c0',"4'b0100",'broadcast')
        b:linkpoint('tcl1',"4'b1111",'broadcast')
        b:linkpoint('cl1',"4'b0111",'broadcast')
        b:linkpoint('tc1',"4'b1101",'broadcast')
        b:linkpoint('c1',"4'b0101",'broadcast')
---[[
b:component('pipe_ctrl','cpu_pipe_ctrl')
    b:clock_sink('clk','clk')
    b:reset_sink('reset','reset')
    b:interface('go', 'data','in','clk')
        b:signal('valid','i_go')
        b:signal('data','i_flush','1','flush')
        b:signal('data','i_mode','2','mode')
        b:signal('data','i_whichpage','1','whichpage')
    b:interface('done', 'data','out','clk')
        b:signal('valid','o_done')
    b:interface('ctrl','conduit','out')
        for sig in Set.values(sigs_ctrl_to_pipe) do
            b:signal('out','o_' .. sig,1,sig)
        end
        for sig in Set.values(sigs_pipe_to_ctrl) do
            b:signal('in','i_' .. sig,1,sig)
        end
---[[
b:component('main_ctrl','cpu_main_ctrl')
    b:clock_sink('clk','clk_b')
    b:reset_sink('reset','reset')
    b:interface('net_go', 'data','in','clk')
        b:signal('valid','i_go','1')
        b:signal('data','i_go_firstcol','1','firstcol')
        b:signal('data','i_go_blkx','8','blkx')
        b:signal('data','i_go_blky','8','blky')
        b:signal('data','i_go_blkymax','8','blkymax')
        b:signal('data','i_go_blkxmin','8','blkxmin')
    b:interface('net_done','data','out','clk')
        b:signal('valid','o_done')
        b:signal('ready','i_done_ack')
    b:interface('rdreq','data','out','clk')
        b:signal('valid','o_rdreq')
        b:signal('ready','i_rdreq_ack')
        -- DB HACK: work around linkpoint exports until supported 
        --b:signal('lp_id','o_rdreq_toctrl','1')
        b:signal('data','o_rdreq_toctrl','1','toctrl')
        b:signal('data','o_rdreq_blkx','8','blkx')
        b:signal('data','o_rdreq_blky','8','blky')
        b:signal('data','o_rdreq_whichbufs','3','whichbufs')
        b:signal('data','o_rdreq_whichpage','1','whichpage')
        --b:linkpoint('mem',"1'b0",'broadcast')
        --b:linkpoint('ctrl',"1'b1",'broadcast')
    b:interface('rddone','data','in','clk')
        b:signal('valid','i_rddone')
        b:signal('data','i_rddone_whichbufs','3','whichbufs')
        b:signal('data','i_rddone_whichpage','1','whichpage')
    b:interface('pipe_go','data','out','clk')
        b:signal('valid','o_comp_go')
        b:signal('data','o_comp_flush','1','comp_flush')
        b:signal('data','o_comp_mode','2','comp_mode')
        b:signal('data','o_comp_page','1','comp_page')
    b:interface('pipe_done', 'data','in','clk')
        b:signal('valid','i_comp_done')
    b:interface('wrreq', 'data','out','clk')
        b:signal('valid','o_wrreq')
        b:signal('data','o_wrreq_blkx','8','blkx')
        b:signal('data','o_wrreq_blky','8','blky')
        b:signal('data','o_wrreq_whichpage','1','whichpage')
        b:signal('data','o_wrreq_whichbufs_top','1','buftop')
    b:interface('wrdone', 'data','in','clk')
        b:signal('valid','i_wrdone')

---[[
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
    
    -- TO CACHES: rdreq
    b:interface('rdreq', 'data', 'out', 'clk')
            b:signal('data','o_cache_rdreq_addr',CACHE_AWIDTH,'addr')
            b:signal('valid','o_cache_rdreq_valid')
            b:signal('ready','i_cache_rdreq_ready')
            b:signal('lp_id','o_cache_rdreq_which','2')
            b:linkpoint('c0',"2'b00",'broadcast');
            b:linkpoint('c1',"2'b10",'broadcast');
            b:linkpoint('t', "2'b01",'broadcast');
            b:linkpoint('t_alias',"2'b11",'broadcast');
    
    -- FROM CACHES: rdresp
    b:interface('rdresp', 'data', 'in', 'clk')
            b:signal('data','i_cache_rdresp_data',CACHE_DWIDTH,'data')
            b:signal('valid','i_cache_rdresp_valid')
            b:signal('ready','o_cache_rdresp_ready')
    
    -- FROM MAIN: WRREQ message
    b:interface('wrreq_msg', 'data', 'in', 'clk');
            b:signal('valid','i_msg_wrreq')
            b:signal('data','i_msg_wrreq_blkx',MAX_BDIMBITS,'blkx')
            b:signal('data','i_msg_wrreq_blky',MAX_BDIMBITS,'blky')
            b:signal('data','i_msg_wrreq_whichpage','1','whichpage')
            b:signal('data','i_msg_wrreq_buftop','1','buftop')
    -- TO MAIN: WRDONE message 
    b:interface('wrdone', 'data', 'out', 'clk');
            b:signal('valid','o_msg_wrdone')

---[[
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
            b:linkpoint('cl0',"4'b0110",'broadcast')
            b:linkpoint('tc0',"4'b1100",'broadcast')
            b:linkpoint('c0',"4'b0100",'broadcast')
            b:linkpoint('tcl1',"4'b1111",'broadcast')
            b:linkpoint('cl1',"4'b0111",'broadcast')
            b:linkpoint('tc1',"4'b1101",'broadcast')
            b:linkpoint('c1',"4'b0101",'broadcast')
    
    -- TO MAIN: RDDONE message
    b:interface('rddone', 'data', 'out', 'clk')
			b:signal('valid','o_msg_rddone')
			b:signal('data','o_msg_rddone_whichbufs',3,'whichbufs')
			b:signal('data','o_msg_rddone_whichpage',1,'whichpage')

local pipe_clock_domain = Set.make{'top','left0','left1','cur0','cur1','pipe','pipe_ctrl'}
local net_clock_domain = Set.make{'main_ctrl','n2c','c2n'}
local cache_tuples = Set.make{'cl0','tc0','c0','tcl1','cl1','tc1','c1'}

b:system('cpu', topo_xbar)

    b:parameter('IS_BPU')
    b:instance('top','cache')
    b:instance('left0','cache')
    b:instance('left1','cache')
    b:instance('cur0','cache')
    b:instance('cur1','cache')
    b:instance('n2c','net_to_cache')
    b:instance('c2n','cache_to_net')
    b:instance('pipe','pipe')
        b:defparam('IS_BPU', 'IS_BPU')
    b:instance('pipe_ctrl','pipe_ctrl')
        b:defparam('IS_BPU', 'IS_BPU')
    b:instance('main_ctrl','main_ctrl')

    b:export('clk_a','clock','in')
    b:export('clk_b','clock','in')
    b:export('reset','reset','in')

    --b:export('main_ctrl.rdreq.mem','data','out')
    --b:export('main_ctrl.rdreq.ctrl','data','out')
    
    for inst in Set.values(pipe_clock_domain) do
        b:link('clk_b', inst..'.clk')
        b:link('reset', inst..'.reset')
    end

    for inst in Set.values(net_clock_domain) do
        b:link('clk_a', inst..'.clk')
        b:link('reset', inst..'.reset')
    end
    
    -- Feature: Implicitly connect interfaces of the same name? 
    -- Intra-CE links, ordered by instantiation order & source 
    -- Cache Sources
    b:link('left0.rdresp.pipe','pipe.left_rdresp')
    b:link('left1.rdresp.pipe','pipe.left_rdresp')

    b:link('cur0.rdresp.pipe','pipe.cur_rdresp')
    b:link('cur0.rdresp.net','c2n.rdresp')
    b:link('cur1.rdresp.pipe','pipe.cur_rdresp')
    b:link('cur1.rdresp.net','c2n.rdresp')

    b:link('top.rdresp.pipe','pipe.top_rdresp')
    b:link('top.rdresp.net','c2n.rdresp')


    
    --Network interfaces
    b:link('n2c.rddone','main_ctrl.rddone')

    b:link('c2n.rdreq.c0','cur0.rdreq.net')
    b:link('c2n.rdreq.c1','cur1.rdreq.net')
    b:link('c2n.rdreq.t','top.rdreq.net')
    b:link('c2n.rdreq.t_alias','top.rdreq.net')
    b:link('c2n.wrdone','main_ctrl.wrdone')

    --this bundles up wrreq to caches from net and mem due to 
    --the sheer number and duplication of linkpoints 
    for tuple in Set.values(cache_tuples) do
        local whichpage = '0'
        local top = nil
        local left = nil 
        --PARSE the linkpoint name 
        if string.find(tuple,'1') then
            whichpage = '1'
        end
        if string.find(tuple,'l') then
            top = true
            left = true
        elseif string.find(tuple,'t') then
            top = true
        end
        -- Connect to the appropriate caches
        for i,source in pairs({'pipe.wrreq.','n2c.wrreq.'}) do 
            b:link(source..tuple, 'cur'..whichpage..'.wrreq')
            if left then
                b:link(source..tuple, 'left'..whichpage..'.wrreq')
            end
            if top then
                b:link(source..tuple, 'top'..'.wrreq')
            end
        end
    end
    -- Pipeline 
    for i,cache in pairs{'left','cur'} do
        b:link('pipe.'..cache..'_rdreq.buf0',cache..'0.rdreq.pipe')
        b:link('pipe.'..cache..'_rdreq.buf1',cache..'1.rdreq.pipe')
    end
    b:link('pipe.top_rdreq', 'top.rdreq.pipe')
    
    -- Pipeline control
    b:link('pipe_ctrl.done','main_ctrl.pipe_done')
    b:link('pipe_ctrl.ctrl','pipe.ctrl')

    -- Main control 
    b:link('main_ctrl.pipe_go','pipe_ctrl.go')
    --b:link('main_ctrl.rdreq.cache','data','out','clk')


--]]


   
