import lu_new::*;

module cpu_pipeline #
(
	parameter IS_BPU = "TRUE",
    parameter L_CACHE_RD_TOP,
    parameter L_CACHE_RD_LEFT,
    parameter L_CACHE_RD_CUR
)
(
	input clk,
	input reset,
	
	// Read counter control
	input i_k_reset,
	input i_k_inc,
	output o_k_done,
	
	input i_i_reset,
	input i_i_inc,
	input i_i_load_k1,
	output o_i_done,
	
	input i_j_reset,
	input i_j_inc,
	input i_j_load_k,
	input i_j_load_k1,
	output o_j_done,
	
	// Pipeline control
	input i_valid,
	input i_norm,
	input i_recip,
	input i_wr_top,
	input i_wr_left,
	input i_wr_cur,
	input i_whichpage,
	output o_pipe_empty,
	
	// Memory accesses

	output  [CACHE_AWIDTH-1:0]  o_cur_rdreq_addr,
    output                      o_cur_rdreq_valid,
    input                       i_cur_rdreq_ready,
    output                      o_cur_rdreq_lp_id, 

	input   [LANES-1:0][31:0]   i_cur_rdresp_data,
    input                       i_cur_rdresp_valid, 
    output                      o_cur_rdresp_ready, 

	output  [CACHE_AWIDTH-1:0]  o_left_rdreq_addr,
    output                      o_left_rdreq_valid,
    input                       i_left_rdreq_ready,
    output                      o_left_rdreq_lp_id,

	input   [LANES-1:0][31:0]   i_left_rdresp_data,
    input                       i_left_rdresp_valid,
    output                      o_left_rdresp_ready, 

    output  [CACHE_AWIDTH-1:0]  o_top_rdreq_addr,
    input                       i_top_rdreq_ready,
    output                      o_top_rdreq_valid,

	input   [LANES-1:0][31:0]   i_top_rdresp_data,
    input                       i_top_rdresp_valid,
    output                      o_top_rdresp_ready, 

	output  [CACHE_AWIDTH-1:0]  o_wr_addr,
	output  [LANES-1:0][31:0]   o_wr_data,
    output  [3:0]               o_wr_lp_id, 
    output                      o_wr_valid,
    input                       i_wr_ready  
);

genvar gi;

//localparam L_CACHE_RD_TOP = 4;
//localparam L_CACHE_RD_LEFT = 5;
//localparam L_CACHE_RD_CUR = 6;
localparam C_TOP_RD = IS_BPU? 1 : 2;
localparam C_TOP_RD_MUX = C_TOP_RD + L_CACHE_RD_TOP;

localparam L_DIV = 33;
localparam C_DIV = C_TOP_RD_MUX + 1;
localparam C_RECIP_WR = C_DIV + L_DIV;

localparam C_MULTIN_MUX = C_TOP_RD_MUX + 1;

localparam L_RECIP_RD = 2;
localparam C_RECIP_RD = C_MULTIN_MUX - 2;

localparam L_MULT = 11;
localparam C_MULT = IS_BPU ? C_MULTIN_MUX + 1 : C_TOP_RD_MUX + 1;

//localparam C_LEFT_RD_MUX = C_MULT - 1;
localparam C_LEFT_RD = C_MULT - L_CACHE_RD_LEFT;

localparam L_SUB = 12;
localparam C_SUB = C_MULT + L_MULT;
//localparam C_CUR_RD_MUX = C_SUB - 1;
localparam C_CUR_RD = C_SUB - L_CACHE_RD_CUR;

localparam L_CACHE_WR = 3;
localparam C_CACHE_WR = L_SUB + C_SUB;
localparam C_WR_ADDR_SWIZ = C_CACHE_WR - 1;
localparam C_WR_WE_MASK = C_CACHE_WR - 1;
localparam C_WR_WE_SWIZ = C_WR_WE_MASK - 1;

localparam C_END = IS_BPU ? C_RECIP_WR : C_CACHE_WR;





// Register some pipeline control signals
logic norm;
always_ff @ (posedge clk) begin
	norm <= i_norm;
end

// Read address counters
logic [BSIZEBITS-1:0] k_1, i_1, j_1;
logic [BSIZEBITS-1:0] k_1_plus_1;

// Tapped delays for valid signal
logic valid [2:C_END];

// Tapped delays for i, j, k counters
logic [BSIZEBITS-1:0] icnt [2:C_CACHE_WR];
logic [BSIZEBITS-1:0] jcnt [2:C_RECIP_WR];
logic [BSIZEBITS-1:0] kcnt [2:C_TOP_RD_MUX];

// Tapped delays for page address
logic whichpage [2:C_CACHE_WR];

// Write enables for end of pipeline
logic wr_left [2:C_CACHE_WR];
logic wr_cur [2:C_CACHE_WR];
logic wr_top [2:C_CACHE_WR];

always_ff @ (posedge clk or posedge reset) begin
    if (reset) begin
        for (int i = $low(valid); i < $high(valid); i++) valid[i] <= '0;
    end
    else begin
        valid[2] <= i_valid;
        icnt[2] <= i_1;
        jcnt[2] <= j_1;
        kcnt[2] <= k_1;
        whichpage[2] <= i_whichpage;
        wr_left[2] <= i_wr_left;
        wr_top[2] <= i_wr_top;
        wr_cur[2] <= i_wr_cur;
        
        for (int i = $low(valid)+1; i <= $high(valid); i++) valid[i] <= valid[i-1];
        for (int i = $low(icnt)+1; i <= $high(icnt); i++) icnt[i] <= icnt[i-1];
        for (int i = $low(jcnt)+1; i <= $high(jcnt); i++) jcnt[i] <= jcnt[i-1];
        for (int i = $low(kcnt)+1; i <= $high(kcnt); i++) kcnt[i] <= kcnt[i-1];
        for (int i = $low(whichpage)+1; i <= $high(whichpage); i++) whichpage[i] <= whichpage[i-1];
        for (int i = $low(wr_left)+1; i <= $high(wr_left); i++) wr_left[i] <= wr_left[i-1];
        for (int i = $low(wr_cur)+1; i <= $high(wr_cur); i++) wr_cur[i] <= wr_cur[i-1];
        for (int i = $low(wr_top)+1; i <= $high(wr_top); i++) wr_top[i] <= wr_top[i-1];
    end
end


// Pipeline headend counter logic
assign k_1_plus_1 = k_1 + ((BSIZEBITS)'(1));
assign o_k_done = (k_1 == ((BSIZEBITS)'(BSIZE-1)));
assign o_i_done = (i_1[BSIZEBITS-1:LANESBITS] == '1); // i_1 >= 24, 48, 96, etc...
assign o_j_done = (j_1 == ((BSIZEBITS)'(BSIZE-1)));

always_ff @ (posedge clk or posedge reset) begin
	if (reset) begin
		k_1 <= '0;
		i_1 <= '0;
		j_1 <= '0;
	end
	else begin
		if (i_k_reset) k_1 <= '0;
		else if (i_k_inc) k_1 <= k_1_plus_1;
		
		if (i_i_reset) i_1 <= '0;
		else if (i_i_load_k1) i_1 <= k_1_plus_1;
		else if (i_i_inc) i_1 <= i_1 + ((BSIZEBITS)'(LANES)); // !SIMD!
		
		if (i_j_reset) j_1 <= '0;
		else if (i_j_load_k1) j_1 <= k_1_plus_1;
		else if (i_j_load_k) j_1 <= k_1;
		else if (i_j_inc) j_1 <= j_1 + ((BSIZEBITS)'(1));
	end
end


// Left block read
logic [LANES-1:0][31:0] left_rd_data; 
assign o_left_rdreq_lp_id = C_LEFT_RD==1? i_whichpage : whichpage[C_LEFT_RD]; 
assign o_left_rdreq_addr  = C_LEFT_RD==1? {k_1, i_1} : {kcnt[C_LEFT_RD], icnt[C_LEFT_RD]};
assign o_left_rdreq_valid = C_LEFT_RD==1? i_valid : valid[C_LEFT_RD];
assign o_left_rdresp_ready = '1;

assign left_rd_data = i_left_rdresp_data; 
//always_ff @ (posedge clk) left_rd_data <= i_left_rdresp_data;

assert property ( @(posedge clk) disable iff (reset) !(o_left_rdreq_valid && !i_left_rdreq_ready));


// Cur block read
logic [LANES-1:0][31:0] cur_rd_data;
assign o_cur_rdreq_lp_id = whichpage[C_CUR_RD]; 
assign o_cur_rdreq_addr  = {jcnt[C_CUR_RD], icnt[C_CUR_RD]};
assign o_cur_rdreq_valid = valid[C_CUR_RD];
assign o_cur_rdresp_ready = '1;

assign cur_rd_data = i_cur_rdresp_data;
//always_ff @ (posedge clk) cur_rd_data <= i_cur_rdresp_data;

assert property (@(posedge clk) disable iff (reset) !(o_cur_rdreq_valid && !i_cur_rdreq_ready));

// Read top block
assign o_top_rdreq_addr = (C_TOP_RD == 1) ? 
    {j_1, k_1} : {jcnt[C_TOP_RD], kcnt[C_TOP_RD]};
assign o_top_rdreq_valid = (C_TOP_RD == 1) ? i_valid : valid[C_TOP_RD];
assign o_top_rdresp_ready = '1;
assert property (@(posedge clk) disable iff (reset) !(o_top_rdreq_valid && !i_top_rdreq_ready));

// Top read data mux (256 to 32)
// Use lower 3 bits of top read addr (lower 3 bits of k) to select SIMD lane
logic [31:0] top_rd_data;
lpm_mux #
(
	.lpm_pipeline(1),
	.lpm_size(8),
	.lpm_width(32),
	.lpm_widths(3)
) top_read_data_mux
(
	.data(i_top_rdresp_data),
	.clock(clk),
	.sel(kcnt[C_TOP_RD_MUX][2:0]),
	.result(top_rd_data)
);

logic [31:0] mult_input;

generate
if (IS_BPU) begin : bpu_only
	// Reciprocal generator
	logic [31:0] div_result;
	divsp fpdiv
	(
		.clock(clk),
		.dataa(32'h3F800000),	// 32'h3F800000
		.datab(top_rd_data),
		.result(div_result)
	);

	// Propagate reciprocal signal (simplifies state machine... trust me)
	logic recip_delayed;
	pipe_delay #(C_RECIP_WR - 1, 1) recip_pipe_delay (clk, reset, 1'b1, i_recip, recip_delayed);

	// (Delayed j counter used as address input for recip mem read and write)

	// Reciprocal memory
	logic [31:0] recip_out;
	altsyncram #
	(
		.operation_mode("DUAL_PORT"),
		.width_a(32),
		.widthad_a(BSIZEBITS),
		.width_b(32),
		.widthad_b(BSIZEBITS),
		.address_reg_b("CLOCK0"),
		.outdata_reg_b("CLOCK0"),
		.ram_block_type("MLAB")
	)
	recip_mem
	(
		.clock0(clk),
		.address_a(jcnt[C_RECIP_WR]),
		.data_a(div_result),
		.wren_a(recip_delayed & valid[C_RECIP_WR]),
		.address_b(jcnt[C_RECIP_RD]),
		.q_b(recip_out)
	);

	// Negate reciprocal. When passed through subtractor, result of multiplication will
	// have correct sign again.
	logic [31:0] recip_out_neg;
	assign recip_out_neg = {~recip_out[31], recip_out[30:0]};
	
	// Choose: reciprocal output or top memory output, as input into multiplier
	always_ff @ (posedge clk) begin
		mult_input <= norm ? recip_out_neg : top_rd_data;
	end
	
	// synthesis translate_off
	shortreal f_recip_in, f_recip_out;
	always_comb begin
		f_recip_in = $bitstoshortreal(div_result);
		f_recip_out = $bitstoshortreal(recip_out);
	end
	// synthesis translate_on
end
else begin : cpu_only
	assign mult_input = top_rd_data;
end
endgenerate

// Floating-point multiplier array, 11 cycle latency
logic [LANES-1:0][31:0] mult_result;
generate
for (gi = 0; gi < LANES; gi++) begin : mults
	multsplat11 fpmult
	(
		.clock(clk),
		.dataa(left_rd_data[gi]),
		.datab(mult_input),
		.result(mult_result[gi])
	);
end
endgenerate

// Floating-point subtractor array, 11 cycle latency
logic [LANES-1:0][31:0] sub_input;
logic [LANES-1:0][31:0] sub_result;
generate
for (gi = 0; gi < LANES; gi++) begin : subs
	// Choose input to subtractor: all zeros (during normalization mode) or data from cur block
	assign sub_input[gi] = norm ? '0 : cur_rd_data[gi];
	
	subsplat11 fpsub
	(
		.clock(clk),
		.dataa(sub_input[gi]),
		.datab(mult_result[gi]),
		.result(sub_result[gi])
	);
end
endgenerate


assign o_wr_addr = {jcnt[C_CACHE_WR], icnt[C_CACHE_WR]};
assign o_wr_data = sub_result;
assign o_wr_lp_id = {wr_top[C_CACHE_WR],wr_cur[C_CACHE_WR],wr_left[C_CACHE_WR],whichpage[C_CACHE_WR]};
assign o_wr_valid = valid[C_CACHE_WR] & |{o_wr_lp_id};
assert property (@(posedge clk) disable iff (reset) !(o_wr_valid && !i_wr_ready));
	

// Pipeline occupancy monitor.
// Tokens are considered eaten once they have been written to memory.
// In reality, the pipeline is still active a little bit even after it's
// reported as 'empty', but there shouldn't be any dependencies between
// those things being written and anything that's just starting to come
// down the pipeline at the head.
//
localparam PIPECAP_BITS = $clog2(C_END);
logic [PIPECAP_BITS-1:0] pipe_capacity;
always_ff @ (posedge clk or posedge reset) begin
	if (reset) begin
		pipe_capacity <= '0;
	end
	else begin
		case ({i_valid, valid[C_END]})
			2'b00: ;
			2'b10: pipe_capacity <= pipe_capacity + ((PIPECAP_BITS)'(1));
			2'b01: pipe_capacity <= pipe_capacity - ((PIPECAP_BITS)'(1));
			2'b11: ;
		endcase
	end
end

assign o_pipe_empty = (pipe_capacity == '0);


// synthesis translate_off
// For simulation debug only

shortreal f_left_rd_data[LANES], f_cur_rd_data[LANES], f_top_rd_data;
shortreal f_wr_data[LANES];
shortreal f_mult_input_b;
shortreal f_sub_input_a_cur[LANES];
shortreal f_sub_input_b_mult[LANES];

always_comb begin
	for (integer ii = 0; ii < LANES; ii++) begin
		f_left_rd_data[ii] = $bitstoshortreal(left_rd_data[ii]);
		f_cur_rd_data[ii] = $bitstoshortreal(cur_rd_data[ii]);
		f_sub_input_a_cur[ii] = $bitstoshortreal(sub_input[ii]);
		f_sub_input_b_mult[ii] = $bitstoshortreal(mult_result[ii]);
		f_wr_data[ii] = $bitstoshortreal(sub_result[ii]);
	end
	
	f_top_rd_data = $bitstoshortreal(top_rd_data);
	f_mult_input_b = $bitstoshortreal(mult_input);
end
// synthesis translate_on
endmodule

