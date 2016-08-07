import lu_new::*;

module cpu_main_ctrl #
(
    parameter IS_BPU = 1'b1
)
(
    input   logic                       clk_b,
    input   logic                       reset,
    
    // GO message from CTRL node
    input   logic                       i_go,
    input   logic                       i_go_firstcol,
    input   logic   [MAX_BDIMBITS-1:0]  i_go_blkx,
    input   logic   [MAX_BDIMBITS-1:0]  i_go_blkymax,
    input   logic   [MAX_BDIMBITS-1:0]  i_go_blkxmin,
    
    // RDREQ message
    output  logic                       o_rdreq,
    input   logic                       i_rdreq_ack,
    output  logic                       o_rdreq_toctrl,
    output  logic   [MAX_BDIMBITS-1:0]  o_rdreq_blkx,
    output  logic   [MAX_BDIMBITS-1:0]  o_rdreq_blky,
    output  t_buftrio                   o_rdreq_whichbufs,
    output  logic                       o_rdreq_whichpage,
    
    // Read Completion
    input   logic                       i_rddone,
    input   t_buftrio                   i_rddone_whichbufs,
    input   logic                       i_rddone_whichpage,
    
    // GO message to pipeline control
    output  logic                       o_comp_go,
    output  t_cpu_comp_mode             o_comp_mode,
    output  logic                       o_comp_flush,
    output  logic                       o_comp_page,
    
    // DONE message from pipeline control
    input   logic                       i_comp_done,
    
    // WRREQ message
    output  logic                       o_wrreq,
    output  logic   [MAX_BDIMBITS-1:0]  o_wrreq_blkx,
    output  logic   [MAX_BDIMBITS-1:0]  o_wrreq_blky,
    output  logic                       o_wrreq_whichpage,
    
    // WRREQ DONE message
    input   logic                       i_wrdone,
    
    // DONE message to CTRL node
    output  logic                       o_done,
    input   logic                       i_done_ack
);

// Mailbox registers for GO message
logic go; bit go_clr;    // begin processing
logic firstcol;            // is this a BPU column?
logic [MAX_BDIMBITS-1:0] blkx;    // x block coordinate of this column
logic [MAX_BDIMBITS-1:0] blkymax;    // bottom y block coordinate of whole matrix
logic [MAX_BDIMBITS-1:0] blkxmin;    // left x block coordinate for this pass
logic [MAX_BDIMBITS-1:0] blky_req; bit blky_req_inc, blky_req_done;        // current block y for requests
logic [MAX_BDIMBITS-1:0] blky_comp; bit blky_comp_inc, blky_comp_done;        // current block y for requests
logic [MAX_BDIMBITS-1:0] blky_wb; bit blky_wb_inc, blky_wb_done;    // current block y for writeback

always_ff @ (posedge clk_b or posedge reset) begin
    if (reset) begin
        go <= '0;
    end
    else begin
        if (i_go) go <= '1;
        else if (go_clr) go <= '0;
        
        if (i_go) begin
            firstcol <= i_go_firstcol;
            blkx <= i_go_blkx;
            blkymax <= i_go_blkymax;
            blkxmin <= i_go_blkxmin;
        end
        
        if (i_go) blky_req <= i_go_blkxmin;
        else if (blky_req_inc) blky_req <= blky_req + (MAX_BDIMBITS)'(1);
        
        if (i_go) blky_wb <= i_go_blkxmin;
        else if (blky_wb_inc) blky_wb <= blky_wb + (MAX_BDIMBITS)'(1);
        
        if (i_go) blky_comp <= i_go_blkxmin;
        else if (blky_comp_inc) blky_comp <= blky_comp + (MAX_BDIMBITS)'(1);
    end
end

assign blky_req_done = (blky_req == blkymax);
assign blky_wb_done = (blky_wb == blkymax);
assign blky_comp_done = (blky_comp == blkymax);


// Block page statuses
bit cur_req_page, cur_req_page_flip, cur_req_page_reset;        // current page being read-requested-into
bit cur_work_page, cur_work_page_flip, cur_work_page_reset;    // current page being calculated on
bit cur_wb_page, cur_wb_page_flip, cur_wb_page_reset;            // current page being written back

bit page_alloced, page_alloced0, page_alloced1;    // page allocated for a read request to come in
t_buftrio page_filled, page_filled0, page_filled1;    // page filled with data from network ready for processing
bit page_processed, page_processed0, page_processed1;    // page has processed data init ready for writeback

bit page_alloc, page_free, page_consume;    // page lifecycle control from the 3 state machines

always_ff @ (posedge clk_b or posedge reset) begin
    if (reset) begin
        cur_req_page <= '0;
        cur_work_page <= '0;
        cur_wb_page <= '0;
        {page_alloced0, page_alloced1} <= '0;
        {page_filled0, page_filled1} <= '0;
        {page_processed0, page_processed1} <= '0;
    end
    else begin
        if (cur_req_page_reset) cur_req_page <= '0;
        else if (cur_req_page_flip) cur_req_page <= ~cur_req_page;
        
        if (cur_work_page_reset) cur_work_page <= '0;
        else if (cur_work_page_flip) cur_work_page <= ~cur_work_page;
        
        if (cur_wb_page_reset) cur_wb_page <= '0;
        else if (cur_wb_page_flip) cur_wb_page <=  ~cur_wb_page;
        
        if (page_alloc && cur_req_page == 1'b0) page_alloced0 <= 1'b1;
        else if (page_free && cur_wb_page == 1'b0) page_alloced0 <= 1'b0;
        
        if (page_alloc && cur_req_page == 1'b1) page_alloced1 <= 1'b1;
        else if (page_free && cur_wb_page == 1'b1) page_alloced1 <= 1'b0;
                    
        if (page_consume && cur_work_page == 1'b0) page_filled0 <= '0;
        else if (i_rddone && i_rddone_whichpage == 1'b0) page_filled0 <= (page_filled0 | i_rddone_whichbufs);
        
        if (page_consume && cur_work_page == 1'b1) page_filled1 <= '0;
        else if (i_rddone && i_rddone_whichpage == 1'b1) page_filled1 <= (page_filled1 | i_rddone_whichbufs);
        
        if (page_consume && cur_work_page == 1'b0) page_processed0 <= 1'b1;
        else if (page_free && cur_wb_page == 1'b0) page_processed0 <= 1'b0;
        
        if (page_consume && cur_work_page == 1'b1) page_processed1 <= 1'b1;
        else if (page_free && cur_wb_page == 1'b1) page_processed1 <= 1'b0;
    end
end

assign page_alloced = (cur_req_page == 1'b0) ? page_alloced0 : page_alloced1;
assign page_filled = (cur_work_page == 1'b0) ? page_filled0 : page_filled1;
assign page_processed = (cur_wb_page == 1'b0) ? page_processed0 : page_processed1;

// Read request generator state machine

enum int unsigned
{
    S_RQ_IDLE,
    S_RQ_BPU_1,
    S_RQ_BPU_2,
    S_RQ_BPU_3,
    S_RQ_BPU_4,
    S_RQ_BPU_5,
    S_RQ_CPU_1,
    S_RQ_CPU_2,
    S_RQ_CPU_3,
    S_RQ_CPU_4,
    S_RQ_CPU_5,
    S_RQ_CPU_6
} rq_state, rq_nextstate;


always_ff @ (posedge clk_b or posedge reset) begin
    if (reset) rq_state <= S_RQ_IDLE;
    else rq_state <= rq_nextstate;
end


always_comb begin
    rq_nextstate = rq_state;
    o_rdreq = '0;
    o_rdreq_whichbufs = 'x;
    blky_req_inc = '0;
    go_clr = '0;
    page_alloc = '0;
    cur_req_page_flip = '0;
    cur_req_page_reset = '0;
    
    o_rdreq_blkx = 'x;
    o_rdreq_blky = 'x;
    o_rdreq_whichpage = cur_req_page;
    o_rdreq_toctrl = 'x;
    
    case (rq_state)
        S_RQ_IDLE: begin
            cur_req_page_reset = '1;
            
            if (go && firstcol && IS_BPU) rq_nextstate = S_RQ_BPU_1;
            else if (go && !firstcol) rq_nextstate = S_RQ_CPU_1;
        end
        
        // Request block to be loaded into all 3 mem blocks (mode 1)
        S_RQ_BPU_1: begin
            go_clr = '1;
            
            o_rdreq = '1;
            o_rdreq_toctrl = '0;
            o_rdreq_blkx = blkxmin;
            o_rdreq_blky = blky_req;
            o_rdreq_whichbufs.top = '1;
            o_rdreq_whichbufs.left = '1;
            o_rdreq_whichbufs.cur = '1;
            
            // No need to check for alloced page the first time
            if (i_rdreq_ack) begin
                rq_nextstate = S_RQ_BPU_2;
            end
        end
        
        S_RQ_BPU_2: begin
            blky_req_inc = '1;    // advance block y counter
            page_alloc = '1;        // mark page as allocated
            cur_req_page_flip = '1;    // flip current request-page
            
            // Last block already?
            if (blky_req_done) rq_nextstate = S_RQ_IDLE;
            else rq_nextstate = S_RQ_BPU_3;
        end
        
        // Request block to be loaded into just left/current (mode 2)
        S_RQ_BPU_3: begin
            o_rdreq = '1;
            o_rdreq_toctrl = '0;
            o_rdreq_blkx = blkxmin;
            o_rdreq_blky = blky_req;
            o_rdreq_whichbufs.top = '0;
            o_rdreq_whichbufs.cur = '1;
            o_rdreq_whichbufs.left = '1;
            
            // Second page - no need to check for alloced page either
            if (i_rdreq_ack) rq_nextstate = S_RQ_BPU_4;
        end
        
        S_RQ_BPU_4: begin
            blky_req_inc = '1;
            page_alloc = '1;
            cur_req_page_flip = '1;
            
            // Reached bottom of matrix
            if (blky_req_done) begin
                rq_nextstate = S_RQ_IDLE;
            end
            else begin
                rq_nextstate = S_RQ_BPU_5;
            end
        end
        
        // Wait for page to be computed on and freed
        S_RQ_BPU_5: begin
            if (!page_alloced) rq_nextstate = S_RQ_BPU_3;
        end
        
        // Get the first left block
        S_RQ_CPU_1: begin
            go_clr = '1;
            
            o_rdreq = '1;
            o_rdreq_toctrl = '1;
            o_rdreq_blkx = blkxmin;
            o_rdreq_blky = blky_req;
            o_rdreq_whichbufs.top = '0;
            o_rdreq_whichbufs.left = '1;
            o_rdreq_whichbufs.cur = '0;
            
            // No need to check if alloced page for the first two allocs
            if (i_rdreq_ack) begin
                rq_nextstate = S_RQ_CPU_2;
            end
        end
        
        // Get the first top/current block
        S_RQ_CPU_2: begin
            o_rdreq = '1;
            o_rdreq_toctrl = '0;
            o_rdreq_blkx = blkx;
            o_rdreq_blky = blky_req;
            o_rdreq_whichbufs.top = '1;
            o_rdreq_whichbufs.left = '0;
            o_rdreq_whichbufs.cur = '1;
            
            // No need to check if alloced page for the first two allocs
            if (i_rdreq_ack) begin
                rq_nextstate = S_RQ_CPU_3;
            end
        end
        
        S_RQ_CPU_3: begin
            blky_req_inc = '1;
            page_alloc = '1;
            cur_req_page_flip = '1;
            
            // Reached bottom of matrix
            if (blky_req_done) begin
                rq_nextstate = S_RQ_IDLE;
            end
            else begin
                rq_nextstate = S_RQ_CPU_4;
            end
        end
        
        // Wait for page to be free before we request into it
        S_RQ_CPU_4: begin
            if (!page_alloced) rq_nextstate = S_RQ_CPU_5;
        end
        
        // Get the left block (similar to state 1)
        S_RQ_CPU_5: begin
            o_rdreq = '1;
            o_rdreq_toctrl = '1;
            o_rdreq_blkx = blkxmin;
            o_rdreq_blky = blky_req;
            o_rdreq_whichbufs.top = '0;
            o_rdreq_whichbufs.left = '1;
            o_rdreq_whichbufs.cur = '0;

            if (i_rdreq_ack) rq_nextstate = S_RQ_CPU_6;
        end
        
        // Get the current block but keep top block (similar to state 2)
        S_RQ_CPU_6: begin
            o_rdreq = '1;
            o_rdreq_toctrl = '0;
            o_rdreq_blkx = blkx;
            o_rdreq_blky = blky_req;
            o_rdreq_whichbufs.top = '0;
            o_rdreq_whichbufs.left = '0;
            o_rdreq_whichbufs.cur = '1;
            
            if (i_rdreq_ack) rq_nextstate = S_RQ_CPU_3;
        end        
    endcase
end
    

// Computation state machine

enum int unsigned
{
    S_CMP_IDLE,
    S_CMP_BPU_1,
    S_CMP_BPU_2,
    S_CMP_BPU_3,
    S_CMP_CPU_1,
    S_CMP_CPU_2,
    S_CMP_CPU_3
} cmp_state, cmp_nextstate;

always_ff @ (posedge clk_b or posedge reset) begin
    if (reset) cmp_state <= S_CMP_IDLE;
    else cmp_state <= cmp_nextstate;
end

always_comb begin
    cmp_nextstate = cmp_state;
    
    o_comp_go = '0;
    o_comp_flush = '1;
    o_comp_mode = t_cpu_comp_mode'('x);
    o_comp_page = cur_work_page;
    cur_work_page_flip = '0;
    cur_work_page_reset = '0;
    page_consume = '0;
    blky_comp_inc = '0;
    
    case (cmp_state)
        S_CMP_IDLE: begin
            cur_work_page_reset = '1;

            if (go && firstcol && IS_BPU) cmp_nextstate = S_CMP_BPU_1;
            else if (go && !firstcol) cmp_nextstate = S_CMP_CPU_1;
        end
        
        // Wait for all 3 blocks to be filled and precalc reciprocals
        S_CMP_BPU_1: begin
            if (page_filled.cur) begin     // cur/left/top all filled simultaneously, just check 1
                o_comp_go = '1;
                o_comp_mode = MODE_1;
                cmp_nextstate = S_CMP_BPU_2;
            end
        end
        
        // Wait for mode1 to finish and mark the block as consumed
        S_CMP_BPU_2: begin
            if (i_comp_done) begin
                page_consume = '1;
                blky_comp_inc = '1;
                cur_work_page_flip = '1;
                
                if (blky_comp_done) begin
                    cmp_nextstate = S_CMP_IDLE;
                end
                else begin
                    cmp_nextstate = S_CMP_BPU_3;
                end
            end
        end
        
        // Wait for current/left block to be filled, and top block to be copied, do a mode2
        S_CMP_BPU_3: begin
            // cur+left filled simultaneously, just check 1 of them.
            if (page_filled.cur) begin    
                o_comp_go = '1;
                o_comp_mode = MODE_2;
                cmp_nextstate = S_CMP_BPU_2;
            end
        end
        
        // Wait for all 3 buffers to have data, then do a mode3
        S_CMP_CPU_1: begin
            if (page_filled.cur && page_filled.left && page_filled.top) begin
                o_comp_go = '1;
                o_comp_mode = MODE_3;
                cmp_nextstate = S_CMP_CPU_2;
            end
        end
        
        // Wait for computation to finish and flip page, and maybe finish
        S_CMP_CPU_2: begin
            if (i_comp_done) begin
                page_consume = '1;
                cur_work_page_flip = '1;
                blky_comp_inc = '1;
                
                if (blky_comp_done) begin
                    cmp_nextstate = S_CMP_IDLE;
                end
                else begin
                    cmp_nextstate = S_CMP_CPU_3;
                end
            end
        end
        
        // Wait for left/cur to have data, then do a mode4
        // The result of the mode3 computation needs to have been copied to the top block,
        // which is why we check wb_donefirst.
        S_CMP_CPU_3: begin
            if (page_filled.cur && page_filled.left) begin
                o_comp_go = '1;
                o_comp_mode = MODE_4;
                cmp_nextstate = S_CMP_CPU_2;
            end
        end
    endcase
end


// Writeback state machine

enum int unsigned
{
    S_WB_IDLE,
    S_WB_1,
    S_WB_2,
    S_WB_3,
    S_WB_4
} wb_state, wb_nextstate;

always_ff @ (posedge clk_b or posedge reset) begin
    if (reset) wb_state <= S_WB_IDLE;
    else wb_state <= wb_nextstate;
end

always_comb begin
    wb_nextstate = wb_state;
    o_done = '0;
    o_wrreq = '0;
    o_wrreq_blkx = blkx;
    o_wrreq_blky = blky_wb;
    o_wrreq_whichpage = cur_wb_page;
    cur_wb_page_flip = '0;
    cur_wb_page_reset = '0;
    blky_wb_inc = '0;
    page_free = '0;
    
    case (wb_state)
        S_WB_IDLE: begin
            cur_wb_page_reset = '1;
            if (go) wb_nextstate = S_WB_1;
        end
        
        S_WB_1: begin
            if (page_processed) begin
                o_wrreq = '1;
                wb_nextstate = S_WB_2;
            end
        end
        
        S_WB_2: begin
            if (i_wrdone) wb_nextstate = S_WB_3;
        end
        
        S_WB_3: begin
            page_free = '1;
            blky_wb_inc = '1;
            cur_wb_page_flip = '1;
            
            if (blky_wb_done) wb_nextstate = S_WB_4;
            else wb_nextstate = S_WB_1;
        end
        
        S_WB_4: begin
            o_done = '1;
            if (i_done_ack) wb_nextstate = S_WB_IDLE;
        end
    endcase
end
    

endmodule
