import lu_new::*;

module cpu_standalone
(
    input clk_a,
	input clk_b,
	input reset,
	
    input i_go,
	input i_go_firstcol,
	input [MAX_BDIMBITS-1:0] i_go_blkx,
	input [MAX_BDIMBITS-1:0] i_go_blkymax,
	input [MAX_BDIMBITS-1:0] i_go_blkxmin,
    
    output o_done,
	input i_done_ack,
    
    output o_rdreq,
	input i_rdreq_ack,
	output o_rdreq_toctrl,
	output [MAX_BDIMBITS-1:0] o_rdreq_blkx,
	output [MAX_BDIMBITS-1:0] o_rdreq_blky,
	output t_buftrio o_rdreq_whichbufs,
	output o_rdreq_whichpage,
    
    input [NET_DWIDTH-1:0] i_rdresp_data,
    input t_buftrio i_rdresp_whichbufs,
    input i_rdresp_whichpage,
    input i_rdresp_valid,
    output o_rdresp_ready,
    input i_rdresp_sop,
    input i_rdresp_eop,
    
    output [NET_DWIDTH-1:0] o_wrreq_data,
    output [MAX_BDIMBITS-1:0] o_wrreq_x,
    output [MAX_BDIMBITS-1:0] o_wrreq_y,
    output o_wrreq_valid,
    input i_wrreq_ready,
    output o_wrreq_sop,
    output o_wrreq_eop
);

cpu # (.IS_BPU(1)) the_cpu
(
    .clk_a(clk_a),
    .clk_b(clk_b),
    .reset(reset),
    
    .go_valid(i_go),
    .go_firstcol(i_go_firstcol),
    .go_blkx(i_go_blkx),
    .go_blkymax(i_go_blkymax),
    .go_blkxmin(i_go_blkxmin),
    
    .done_valid(o_done),
    .done_ready(i_done_ack),
    
    .rdreq_valid(o_rdreq),
    .rdreq_ready(i_rdreq_ack),
    .rdreq_lp_id(o_rdreq_toctrl),
    .rdreq_blkx(o_rdreq_blkx),
    .rdreq_blky(o_rdreq_blky),
    .rdreq_whichbufs(o_rdreq_whichbufs),
    .rdreq_whichpage(o_rdreq_whichpage),
    
    .rdresp_valid(i_rdresp_valid),
    .rdresp_ready(o_rdresp_ready),
    .rdresp_data(i_rdresp_data),
    .rdresp_whichbufs(i_rdresp_whichbufs),
    .rdresp_whichpage(i_rdresp_whichpage),
    .rdresp_sop(i_rdresp_sop),
    .rdresp_eop(i_rdresp_eop),
    
    .wrreq_valid(o_wrreq_valid),
    .wrreq_ready(i_wrreq_ready),
    .wrreq_data(o_wrreq_data),
    .wrreq_x(o_wrreq_x),
    .wrreq_y(o_wrreq_y),
    .wrreq_sop(o_wrreq_sop),
    .wrreq_eop(o_wrreq_eop)
);

endmodule
