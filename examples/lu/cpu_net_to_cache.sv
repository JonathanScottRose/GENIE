import lu_new::*;

module cpu_net_to_cache
(
    input clk,
    input reset,
    
    // FROM OUTSIDE: rdresp
    input   logic   [LANES-1:0][31:0]   i_net_rdresp_data,
    input   t_buftrio                   i_net_rdresp_whichbufs,
    input   logic                       i_net_rdresp_whichpage,
    input   logic                       i_net_rdresp_valid,
    input   logic                       i_net_rdresp_sop,
    input   logic                       i_net_rdresp_eop,
    output  logic                       o_net_rdresp_ready,
    
    // TO CACHES: wrreq
    output  logic   [LANES-1:0][31:0]  o_cache_wrreq_data,
    output  logic   [CACHE_AWIDTH-1:0]  o_cache_wrreq_addr,
    output  logic                       o_cache_wrreq_valid,
    input   logic                       i_cache_wrreq_ready,
    output  logic   [3:0]               o_cache_wrreq_which,
    
    // TO MAIN: RDDONE message
    output  logic                       o_msg_rddone,
    output  t_buftrio                   o_msg_rddone_whichbufs,
    output  logic                       o_msg_rddone_whichpage
);

// Holds registered EOP,whichbufs/page from net
logic eop;
t_buftrio whichbufs;
logic whichpage;

assign o_cache_wrreq_which = {whichbufs.top,whichbufs.cur, whichbufs.left, whichpage};    // LPID is just a concat of these

/*
// Propagate signals through an interlock to reduce combinational path
pipe_interlock #
(
    .WIDTH(CACHE_DWIDTH + $bits(t_buftrio) + 1 + 1),
    .REGISTERED(1)
)
ilk
(
    .i_clk(clk),
    .i_reset(reset),
    .i_in({i_net_rdresp_data, i_net_rdresp_whichpage, i_net_rdresp_whichbufs, i_net_rdresp_eop}),
    .i_have(i_net_rdresp_valid),
    .o_want(o_net_rdresp_ready),
    .o_out({o_cache_wrreq_data, whichpage, whichbufs, eop}),
    .o_have(o_cache_wrreq_valid),
    .i_want(i_cache_wrreq_ready)
);
*/

assign o_cache_wrreq_data = i_net_rdresp_data;
assign whichpage = i_net_rdresp_whichpage;
assign whichbufs = i_net_rdresp_whichbufs;
assign eop = i_net_rdresp_eop;
assign o_cache_wrreq_valid = i_net_rdresp_valid;
assign o_net_rdresp_ready = i_cache_wrreq_ready;


wire wrote_word = o_cache_wrreq_valid & i_cache_wrreq_ready;
wire accepted_word = i_net_rdresp_valid & o_net_rdresp_ready;

// Address counter to cache
always_ff @ (posedge clk or posedge reset) begin
    if (reset) o_cache_wrreq_addr <= '0;
    else begin
        // Reset address counter after finishing writing a packet
        // Increment counter on successful write of a single word to the cache.
        // Increment by (LANES) since we write that many 32-bit words at once
        if (wrote_word && eop) o_cache_wrreq_addr <= '0;
        else if (wrote_word) o_cache_wrreq_addr <= o_cache_wrreq_addr + (CACHE_AWIDTH)'(LANES);
    end
end

// ReadDone message to main
always_ff @ (posedge clk or posedge reset) begin
    if (reset) o_msg_rddone <= '0;
    else begin
        // Read is done when we successfully wrote the last word to the cache
        o_msg_rddone <= wrote_word && eop;
        o_msg_rddone_whichbufs <= whichbufs;
        o_msg_rddone_whichpage <= whichpage;
    end
end


//Debug info
// synthesis translate_off
shortreal f_wrreq_to_cache[LANES];
always_comb begin
    for( int i =0; i<LANES; i++) begin
        f_wrreq_to_cache[i] = $bitstoshortreal(o_cache_wrreq_data[i]);
    end
end
// synthesis translate_on


endmodule
