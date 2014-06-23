module cur_input_calc #
(
	parameter NI = 1,
	parameter NIBITS = 1
)
(
	input [NI-1:0] i_valid,
	input [NIBITS-1:0] i_last_input,
	output reg [NIBITS-1:0] o_cur_input
);
	// This generate block creates NI valid/idx pairs, one for each
	// of the NI inputs. The IDXs are just the numbers 0 to NI-1.
	// Each generated valid/idx pair is fed by a mux that selects from
	// all of the NI valid inputs. The one that gets selected is
	// based on who holds the current grant.
	//
	// Example, if NI is 4, and input 2 currently holds the grant,
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

	wire [NI-1:0] sorted_valids;
	wire [NI*NIBITS-1:0] sorted_idxs;

	genvar gi;
	generate
		for (gi = 0; gi < NI; gi = gi + 1) begin : sort
			// Generate a packed vector of all the input valid/IDX pairs
			reg [ (NIBITS+1)*NI - 1 : 0 ] sort_in;

			always @* begin : mksort_in
				integer i;
				for (i = 0; i < NI; i = i + 1) begin : mksort_in_loop
					integer in_idx;
					in_idx = (gi + i + 1) % NI;
					sort_in[ (NIBITS+1)*i +: (NIBITS+1) ] = { i_valid[in_idx], in_idx[NIBITS-1:0] };
				end
			end
			
			// Do the sorting 
			ct_mux #
			(
				.lpm_width(1 + NIBITS),
				.lpm_size(NI),
				.lpm_widths(NIBITS),
				.lpm_pipeline(0)
			)
			sort_mux
			(
				.data(sort_in),
				.result({sorted_valids[gi], sorted_idxs[gi*NIBITS +: NIBITS]}),
				.sel(i_last_input)
			);
		end
	endgenerate

	// Once we have sorted_valids/sorted_idx, this priority encoder
	// scans through sorted_valids and finds the first nonzero entry.
	// This guy will get the grant next. If no one is making a request,
	// cur_input stays at last_input
	always @* begin : pri_enc
		integer i;
		o_cur_input = sorted_idxs[NIBITS*(NI-1) +: NIBITS]; // this corresponds to the last input
		
		for (i = NI-2; i >= 0; i = i - 1) begin
			if (sorted_valids[i]) o_cur_input = sorted_idxs[NIBITS*i +: NIBITS];
		end
	end
endmodule
	
	

module ct_merge #
(
	parameter NI = 1,
	parameter WIDTH = 1
)
(
	input clk,
	input reset,
	
	input [NI*WIDTH-1:0] i_data,
	input [NI-1:0] i_valid,
	output reg [NI-1:0] o_ready,
	input [NI-1:0] i_eop,
	
	output o_valid,
	output [WIDTH-1:0] o_data,
	input i_ready,
	output o_eop
);

function integer CLogB2;
	input [31:0] Depth;
	integer i;
	begin
		i = Depth;		
		for(CLogB2 = 0; i > 0; CLogB2 = CLogB2 + 1)
			i = i >> 1;
		if (CLogB2 < 1) CLogB2 = 1;
	end
endfunction

localparam NIBITS = CLogB2(NI-1);

//
// Datapath
//

// Current input
reg [NIBITS-1:0] last_input;
wire [NIBITS-1:0] cur_input;
wire last_input_load;

// Mux select calculator (arbiter function)
cur_input_calc #
(
	.NI(NI),
	.NIBITS(NIBITS)
) calc
(
	.i_valid(i_valid),
	.i_last_input(last_input),
	.o_cur_input(cur_input)
);

// Output muxes
ct_mux #
(
	.lpm_width(WIDTH),
	.lpm_size(NI),
	.lpm_widths(NIBITS),
	.lpm_pipeline(0)
) out_mux_data
(
	.data(i_data),
	.sel(cur_input),
	.result(o_data)
);

ct_mux #
(
	.lpm_width(1),
	.lpm_size(NI),
	.lpm_widths(NIBITS),
	.lpm_pipeline(0)
) out_mux_eop
(
	.data(i_eop),
	.sel(cur_input),
	.result(o_eop)
);

ct_mux #
(
	.lpm_width(1),
	.lpm_size(NI),
	.lpm_widths(NIBITS),
	.lpm_pipeline(0)
) out_mux_valid
(
	.data(i_valid),
	.sel(cur_input),
	.result(o_valid)
);

always @ (posedge clk or posedge reset) begin
	if (reset) begin
		last_input <= 'd0;
	end
	else begin
		if (last_input_load) last_input <= cur_input;
	end
end

always @* begin : dp_comb
	integer i;
	for (i = 0; i < NI; i = i + 1) begin
        // Broadcast ready
		o_ready[i] = i_ready;
    end
end

assign last_input_load = o_valid && i_ready && o_eop;

endmodule

