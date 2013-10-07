module ct_merge #
(
	parameter RADIX = 1,
	parameter WIDTH = 1,
	parameter EOP_LOC = 0
)
(
	input clk,
	input reset,
	
	input [RADIX*WIDTH-1:0] i_data,
	input [RADIX-1:0] i_valid,
	output [RADIX-1:0] o_ready,
	
	output o_valid,
	output [WIDTH-1:0] o_data,
	input i_ready
);

function integer CLogB2;
	input [31:0] Depth;
	integer i;
	begin
		i = Depth;		
		for(CLogB2 = 0; i > 0; CLogB2 = CLogB2 + 1)
			i = i >> 1;
	end
endfunction

localparam RADBITS = CLogB2(RADIX-1);

// Currently serviced/granted input and next input
reg [RADBITS-1:0] cur_input, next_input;
reg cur_input_load;

always @ (posedge clk or posedge reset) begin
	if (reset) cur_input <= 0;
	else if (cur_input_load) cur_input <= next_input;
end

// This generate block creates RADIX valid/idx pairs, one for each
// of the RADIX inputs. The IDXs are just the numbers 0 to RADIX-1.
// Each generated valid/idx pair is fed by a mux that selects from
// all of the RADIX valid inputs. The one that gets selected is
// based on who holds the current grant.
//
// Example, if RADIX is 4, and input 2 currently holds the grant,
// the outputs of this generate block will be:
//
//     sorted_valid	sorted_idx
// 0       valid[3]           3
// 1       valid[0]           0
// 2       valid[1]           1
// 3       valid[2]           2
//
// Note that the inputs have been sorted by a fair round-robin
// priority, such that the currently granted input (2) gets put at
// the end of the list, and the next adjacent input (3) gets
// first dibs.

wire [RADIX-1:0] sorted_valids;
wire [RADIX*RADBITS-1:0] sorted_idxs;

genvar gi;
generate
	for (gi = 0; gi < RADIX; gi = gi + 1) begin : sort
		// Generate a packed vector of all the input valid/IDX pairs
		reg [ (RADBITS+1)*RADIX - 1 : 0 ] sort_in;

		always @* begin : mksort_in
			integer i;
			for (i = 0; i < RADIX; i = i + 1) begin : mksort_in_loop
				integer in_idx;
				in_idx = (gi + i + 1) % RADIX;
				sort_in[ (RADBITS+1)*i +: (RADBITS+1) ] = { i_valid[in_idx], in_idx[RADBITS-1:0] };
			end
		end
		
		// Do the sorting 
		ct_mux #
		(
			.lpm_width(1 + RADBITS),
			.lpm_size(RADIX),
			.lpm_widths(RADBITS),
			.lpm_pipeline(0)
		)
		sort_mux
		(
			.data(sort_in),
			.result({sorted_valids[gi], sorted_idxs[gi*RADBITS +: RADBITS]}),
			.sel(cur_input)
		);
	end
endgenerate

// Once we have sorted_valids/sorted_idx, this priority encoder
// scans through sorted_valids and finds the first nonzero entry.
// This guy will get the grant next. If no one is making a request,
// next_input remains at current_input.
always @* begin : pri_enc
	integer i;
	next_input = cur_input;
	
	for (i = RADIX-1; i >= 0; i = i - 1) begin
		if (sorted_valids[i]) next_input = sorted_idxs[RADBITS*i +: RADBITS];
	end
end
	
// Given the currently-granted input, make a mux that will
// connect the granted input to the output. The input
// includes the data and valid signals.

wire [RADIX*(WIDTH+1) - 1:0] out_mux_input;

ct_mux #
(
	.lpm_width(WIDTH + 1),
	.lpm_size(RADIX),
	.lpm_widths(RADBITS),
	.lpm_pipeline(0)
)
out_mux
(
	.data(out_mux_input),
	.result({o_data, o_valid}),
	.sel(cur_input)
);

// This generate block builds the out_mux_input vector for the forward-travelling signals,
// and gates the backward-travelling ready signal based on who holds the grant.
generate
	for (gi = 0; gi < RADIX; gi = gi + 1) begin : connect
		assign out_mux_input [(WIDTH+1)*gi +: (WIDTH+1)] = {i_data[WIDTH*gi +: WIDTH], i_valid[gi]};
		assign o_ready[gi] = i_ready && (cur_input == gi);
	end
endgenerate


// Main arbiter state machine.
// We need to know when we're in the middle
// of servicing a long data packet so we can
// hold arbitration steady, hence two states.

localparam [1:0] S_NORMAL = 2'd0, S_WAIT_EOP = 2'd1;
reg [1:0] state, nextstate;	

always @ (posedge clk or posedge reset) begin
	if (reset) state <= S_NORMAL;
	else state <= nextstate;
end

wire o_eop = o_data[EOP_LOC];

always @* begin
	nextstate = state;
	cur_input_load = 1'b0;
	
	case (state)
		S_NORMAL: begin
			cur_input_load = 1'b1;
			if (o_valid && o_ready && !o_eop) begin
				cur_input_load = 1'b0;
				nextstate = S_WAIT_EOP;
			end
		end
		
		S_WAIT_EOP: begin
			if (o_valid && o_ready && o_eop) begin
				cur_input_load = 1'b1;
				nextstate = S_NORMAL;
			end
		end
	endcase
end

endmodule

