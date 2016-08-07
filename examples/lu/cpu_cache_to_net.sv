import lu_new::*;

module cpu_cache_to_net
(
    input clk,
    input reset,
    
    // TO OUTSIDE: wrreq
    output  logic   [LANES-1:0][31:0]   o_net_wrreq_data,
    output  logic   [MAX_BDIMBITS-1:0]  o_net_wrreq_x,
    output  logic   [MAX_BDIMBITS-1:0]  o_net_wrreq_y,
    output  logic                       o_net_wrreq_valid,
    input   logic                       i_net_wrreq_ready,
    output  logic                       o_net_wrreq_eop,
    output  logic                       o_net_wrreq_sop,

    
    // TO CACHES: rdreq
    output  logic   [CACHE_AWIDTH-1:0]  o_cache_rdreq_addr,
    output  logic                       o_cache_rdreq_valid,
    input   logic                       i_cache_rdreq_ready,
    output  logic   [3:0]               o_cache_rdreq_which,
    
    // FROM CACHES: rdresp
    input   logic   [LANES-1:0][31:0]   i_cache_rdresp_data,
    input   logic                       i_cache_rdresp_valid,
    output  logic                       o_cache_rdresp_ready,
    
    // FROM MAIN: WRREQ message
    input   logic                       i_msg_wrreq,
    input   logic   [MAX_BDIMBITS-1:0]  i_msg_wrreq_blkx,
    input   logic   [MAX_BDIMBITS-1:0]  i_msg_wrreq_blky,
    input   logic                       i_msg_wrreq_whichpage,
    
    // TO MAIN: WRDONE message
    output  logic                       o_msg_wrdone
);

localparam LAST_ADDRESS = BSIZE*BSIZE-LANES; 
// Whichbufs/page registers
t_buftrio whichbufs;
logic whichpage;
logic [MAX_BDIMBITS-1:0] blkx, blky;

always_ff @ (posedge clk) begin
    if (i_msg_wrreq) begin
        whichpage <= i_msg_wrreq_whichpage;
        whichbufs.top <= '0;
        whichbufs.cur <= '1;
        whichbufs.left <= '0;
        blkx <= i_msg_wrreq_blkx;
        blky <= i_msg_wrreq_blky;
    end
end

// Pipeline enable for read side
wire read_accepted = o_cache_rdreq_valid && i_cache_rdreq_ready;

// Read address counter for caches
logic is_reading;
logic [CACHE_AWIDTH-1:0] read_addr;
wire read_last = read_addr == LAST_ADDRESS; 

always_ff @ (posedge clk or posedge reset) begin
    if (reset) begin
        is_reading <= '0;
        read_addr <= '0; 
    end 
    else begin
        if (i_msg_wrreq) is_reading <= '1;
        else if (read_accepted && read_last) is_reading <= '0;
        
        if ((read_last && read_accepted)) read_addr <= '0;
        else if (read_accepted) read_addr <= read_addr + LANES;
    end
end

// Do the cache read
assign o_cache_rdreq_addr = read_addr;
assign o_cache_rdreq_valid = is_reading;
assign o_cache_rdreq_which = {whichpage};


// Cache rdresp -> net wrreq data
pipe_interlock #
(
    .WIDTH(CACHE_DWIDTH),
    .REGISTERED(1)
)
ilk
(
    .i_clk(clk),
    .i_reset(reset),
    .i_in(i_cache_rdresp_data),
    .i_have(i_cache_rdresp_valid),
    .o_want(o_cache_rdresp_ready),
    .o_out(o_net_wrreq_data),
    .o_have(o_net_wrreq_valid),
    .i_want(i_net_wrreq_ready)
);

// Pipeline enable
wire write_accepted = o_net_wrreq_valid && i_net_wrreq_ready;

// Write counter, for knowing when writes finish and generating eop
logic [CACHE_AWIDTH-1:0] write_cnt;
wire write_cnt_last = write_cnt == LAST_ADDRESS;
wire write_cnt_start = write_cnt == '0;

always_ff @ (posedge clk) begin
    if (i_msg_wrreq) write_cnt <= '0;
    else if (write_accepted) write_cnt <= write_cnt + LANES;
end

// Generate wrdone message
assign o_msg_wrdone = write_accepted && write_cnt_last;

// Do the net write
assign o_net_wrreq_x = blkx;
assign o_net_wrreq_y = blky;
assign o_net_wrreq_eop = write_cnt_last;
assign o_net_wrreq_sop = write_cnt_start; 

//Debug info
// synthesis translate_off
shortreal f_wrreq_to_net[LANES], f_rdresp_from_cache[LANES];
always_comb begin
    for( int i =0; i<LANES; i++) begin
        f_wrreq_to_net[i] = $bitstoshortreal(o_net_wrreq_data[i]);
        f_rdresp_from_cache[i] = $bitstoshortreal(i_cache_rdresp_data[i]);
    end
end
// synthesis translate_on

endmodule
