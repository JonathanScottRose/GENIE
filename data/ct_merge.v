module next_input_calc #
(
	parameter NI = 1,
	parameter NIBITS = 1
)
(
	input [NI-1:0] i_valid,
	input [NIBITS-1:0] i_cur_input,
	output reg [NIBITS-1:0] o_next_input
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
				.sel(i_cur_input)
			);
		end
	endgenerate

	// Once we have sorted_valids/sorted_idx, this priority encoder
	// scans through sorted_valids and finds the first nonzero entry.
	// This guy will get the grant next. If no one is making a request,
	// next_input remains at the current input by virtue of sorting.
	always @* begin : pri_enc
		integer i;
		o_next_input = NI-1; // this corresponds to the current input
		
		for (i = NI-2; i >= 0; i = i - 1) begin
			if (sorted_valids[i]) o_next_input = sorted_idxs[NIBITS*i +: NIBITS];
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
	end
endfunction

localparam NIBITS = CLogB2(NI-1);

//
// Datapath
//

// Current input
reg [NIBITS-1:0] cur_input;
wire [NIBITS-1:0] next_input;
wire cur_input_load;

// Staging registers
reg [NI*(WIDTH+1)-1:0] staging_datas, staging_datas_din;
reg [NI-1:0] staging_valids, staging_valids_din;

// Next input calculator
next_input_calc #
(
	.NI(NI),
	.NIBITS(NIBITS)
) calc
(
	.i_valid(staging_valids_din),
	.i_cur_input(cur_input),
	.o_next_input(next_input)
);

// Output muxes
ct_mux #
(
	.lpm_width(WIDTH+1),
	.lpm_size(NI),
	.lpm_widths(NIBITS),
	.lpm_pipeline(0)
) out_mux_data
(
	.data(staging_datas),
	.sel(cur_input),
	.result({o_data, o_eop})
);

ct_mux #
(
	.lpm_width(1),
	.lpm_size(NI),
	.lpm_widths(NIBITS),
	.lpm_pipeline(0)
) out_mux_valid
(
	.data(staging_valids),
	.sel(cur_input),
	.result(o_valid)
);

always @ (posedge clk or posedge reset) begin
	if (reset) begin
		cur_input <= 'd0;
		staging_valids <= 'd0;
	end
	else begin
		if (cur_input_load) cur_input <= next_input;
		
		staging_valids <= staging_valids_din;
		staging_datas <= staging_datas_din;
	end
end

always @* begin : dp_comb
	integer i;
	for (i = 0; i < NI; i = i + 1) begin
		// Staging register can be filled from external input (instead of recirculated) if:
		// - it's empty (not valid), or
		// - it's the currently granted input and the output is ready to accept it
		o_ready[i] = !staging_valids[i] || (cur_input == i[NIBITS-1:0] && i_ready);
		
		// Select input from outside, or busywait recirculation
		staging_valids_din[i] = o_ready[i]? i_valid[i] : staging_valids[i];
		staging_datas_din[(WIDTH+1)*i +: WIDTH+1] = o_ready[i]? 
			{i_data[WIDTH*i+:WIDTH], i_eop[i]} :
			staging_datas[(WIDTH+1)*i +: WIDTH+1];
	end
end

wire pipe_enable = !(o_valid && !i_ready);
assign cur_input_load = pipe_enable && !(o_valid && i_ready && !o_eop);

endmodule

