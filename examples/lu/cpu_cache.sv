import lu_new::*;

module cpu_cache_slice # 
(
    parameter AWIDTH=8 
)
(
    input clk,
    input [AWIDTH-1:0] waddr, 
    input [AWIDTH-1:0] raddr, 
    input [31:0] wdata, 
    input we,
    input ce,
    output [31:0] rdata
);

// synthesis translate_off
    logic [31:0] ram [2**AWIDTH];
    logic [AWIDTH-1:0] waddr_reg; 
    logic [AWIDTH-1:0] raddr_reg; 
    logic [31:0] wdata_reg; 
    logic we_reg;
    logic [31:0] output_registers;	
    
    //Input (clock1) registers
    always_ff @(posedge clk) begin
        if (ce) begin
            raddr_reg <= raddr; 
        end
    end

    //RAM read, Output (clock1) registers
    always_ff @(posedge clk) 
    begin
        if (ce) begin
            output_registers <= ram[raddr_reg]; 
        end 
    end

    //RAM write. 
    always_ff @(posedge clk)
    begin
        if (we) begin
            ram[waddr] <= wdata; 
        end
    end

    assign rdata = output_registers; 

// synthesis translate_on 

// synthesis read_comments_as_HDL on
//    altsyncram #
//    (
//        .operation_mode("DUAL_PORT"),
//        .width_a(32),
//        .widthad_a(AWIDTH),
//        .width_b(32),
//        .widthad_b(AWIDTH),
//        .outdata_reg_b("CLOCK1"),
//        .ram_block_type("M9K")
//    ) mem_slice
//    (
//        .clock0(clk),
//        .clock1(clk),
//        .address_a(waddr),
//        .data_a(wdata),
//        .wren_a(we),
//        .address_b(raddr),
//        .q_b(rdata),
//        .clocken1(ce)
//	);
//*/
// synthesis read_comments_as_HDL off

endmodule

module cpu_cache 
(
    input clk,
    input reset, 

    //Write request ( DATA + VALID) 
	input   [CACHE_AWIDTH-1:0]  i_wrreq_addr,
	input   [LANES-1:0] [31:0]  i_wrreq_data,
	//input   [LANES-1:0]         i_wrreq_enable,
	input                       i_wrreq_valid,
	
	//Read request (DATA + VALID + READY + LPID)
    //Rdreq can be stalled (o_ready goes low) due to 
    //propogation of the CPU stalling on the consumption of
    //rdresp data 

	input   [CACHE_AWIDTH-1:0]  i_rdreq_addr,
    input                       i_rdreq_valid,
    input                       i_rdreq_lp_id, 
    output                      o_rdreq_ready,
    
    //Read response (DATA + VALID + READY + LPID)
    //CPU can stall rdresp through de-assertion of ready 
	output  [LANES-1:0] [31:0]  o_rdresp_data,
	output                      o_rdresp_valid,
	output                      o_rdresp_lp_id, 
    input                       i_rdresp_ready
); 

// INTERNAL LATENCIES
localparam L_RDRESP = 4; 

// Pipeline registers for read req-reply feedthrough
logic                       rd_lp_id [L_RDRESP];
logic                       rd_valid [L_RDRESP];

//WIRES
//Staged read request signals (post interlock) 
logic  [CACHE_AWIDTH-1:0]   ilked_rdreq_addr;
logic                       ilked_rdreq_valid;
logic                       ilked_rdreq_lp_id; 

//Read pipeline enable (stalls to wait for CPU)
logic                       rpipe_enable; 

always_ff @ (posedge clk or posedge reset) begin
    if (reset) begin
        for (int i = 0; i <= $high(rd_valid); i++) rd_valid[i] <= '0;
        for (int i = 0; i <= $high(rd_lp_id); i++) rd_lp_id[i] <= '0;
    end
    else if (rpipe_enable) begin
        rd_valid [0] = ilked_rdreq_valid; 
        rd_lp_id [0] = ilked_rdreq_lp_id; 
	    for (int i = 1; i <= $high(rd_valid); i++) rd_valid[i] <= rd_valid[i-1];
	    for (int i = 1; i <= $high(rd_lp_id); i++) rd_lp_id[i] <= rd_lp_id[i-1];
    end
end

assign o_rdresp_lp_id = rd_lp_id[L_RDRESP-1];
assign o_rdresp_valid = rd_valid[L_RDRESP-1];

//Stage the read request, in case the pipeline is current stalled
pipe_interlock #
(   //ADDRESS        FROM WHOM       VALID 
    .WIDTH(CACHE_AWIDTH + 1),
    .REGISTERED(1)
)
ilk
(
    .i_clk(clk),
    .i_reset(reset),
    .i_in({i_rdreq_addr, i_rdreq_lp_id}),
    .i_have(i_rdreq_valid),
    .o_want(o_rdreq_ready),
    .o_out({ilked_rdreq_addr, ilked_rdreq_lp_id}),
    .o_have(ilked_rdreq_valid),
    .i_want(rpipe_enable)
);

// Pipeline enable logic
assign rpipe_enable = ~(~i_rdresp_ready && o_rdresp_valid); 

genvar gi;
generate
	for (gi = 0; gi < LANES; gi++) begin : slices
		logic [CACHE_AWIDTH-LANESBITS-1:0] slice_raddr;
		logic [CACHE_AWIDTH-LANESBITS-1:0] slice_waddr;
		logic [31:0] slice_wdata;
        logic slice_we; 
   
        //READ ADDRESS SWIZ (to handle read word to net word alignment) 
        cpu_ram_addr_swiz #
        (
            .TOTAL_BITS(2*BSIZEBITS),
            .SWIZ_BITS(LANESBITS),
            .LANE(gi)
        ) read_address_swiz
        (
            .clk(clk),
            .reset(reset),
            .i_addr(ilked_rdreq_addr),
            .o_addr(slice_raddr)
        );
        //Write address swiz (to handle write to net word alignment) 
        cpu_ram_addr_swiz #
        (
            .TOTAL_BITS(2*BSIZEBITS),
            .SWIZ_BITS(LANESBITS),
            .LANE(gi)
        ) write_address_swiz
        (
            .clk(clk),
            .reset(reset),
            .i_addr(i_wrreq_addr),
            .o_addr(slice_waddr)
        );
        //write enable swiz (prevent writing to an address that's too large)
        cpu_ram_we_swiz #
        (
            .COORD_BITS(BSIZEBITS),
            .SWIZ_BITS(LANESBITS),
            .LANE(gi)
        ) we_swiz
        (
            .clk(clk),
            .reset(reset),
            .i_addr(i_wrreq_addr),
            .i_we(i_wrreq_valid),
            .o_we(slice_we)
        );

        always_ff @ (posedge clk) begin   
            slice_wdata <= i_wrreq_data[gi];
		end
		
        cpu_cache_slice #(
            .AWIDTH(CACHE_AWIDTH-LANESBITS)
        ) mem_slice (
            .clk(clk), 
			.waddr(slice_waddr),
			.wdata(slice_wdata),
			.we(slice_we),
			.raddr(slice_raddr),
			.rdata(o_rdresp_data[gi]),
			.ce(rpipe_enable)
        );
	end
endgenerate

//Debug info
// synthesis translate_off
shortreal f_wrreq_in[LANES], f_rdresp_out[LANES];
always_comb begin
    for( int i =0; i<LANES; i++) begin
        f_wrreq_in[i] = $bitstoshortreal(i_wrreq_data[i]);
        f_rdresp_out[i] = $bitstoshortreal(o_rdresp_data[i]);
    end
end
// synthesis translate_on

endmodule 


