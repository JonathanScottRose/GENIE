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
	parameter WIDTH = 1,
	parameter EOP_LOC = 0
)
(
	input clk,
	input reset,
	
	input [NI*WIDTH-1:0] i_data,
	input [NI-1:0] i_valid,
	output reg [NI-1:0] o_ready,
	
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

localparam NIBITS = CLogB2(NI-1);

//
// Datapath
//

// Current input
reg [NIBITS-1:0] cur_input;
wire cur_input_load;

// Staging registers
reg [NI*WIDTH-1:0] staging_datas;
reg [NI-1:0] staging_valids;

// Next inputs: 1 is from inputs, 2 is from staging registers
localparam FROM_INPUT = 1'b0, FROM_STAGING = 1'b1;
wire [NIBITS-1:0] next_input_1, next_input_2;
wire next_input_sel;

// Counts how many of the staging registers contain live inputs
reg [NIBITS:0] staging_count;
wire staging_count_calc;
wire staging_count_dec;
wire staging_count_thresh = (|staging_count[NIBITS:1]); // 2 or more live

// Next input calculators
next_input_calc #
(
	.NI(NI),
	.NIBITS(NIBITS)
) calc_in
(
	.i_valid(i_valid),
	.i_cur_input(cur_input),
	.o_next_input(next_input_1)
);

next_input_calc #
(
	.NI(NI),
	.NIBITS(NIBITS)
) calc_staging
(
	.i_valid(staging_valids),
	.i_cur_input(cur_input),
	.o_next_input(next_input_2)
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
	.data(staging_datas),
	.sel(cur_input),
	.result(o_data)
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
		staging_count <= 'd0;
	end
	else begin : dp
		integer i;
		
		if (cur_input_load) begin
			cur_input <= next_input_sel==FROM_INPUT ? next_input_1 : next_input_2;
		end
		
		for (i = 0; i < NI; i = i + 1) begin
			if (o_ready[i]) begin
				staging_datas[i*WIDTH +: WIDTH] <= i_data[i*WIDTH +: WIDTH];
				staging_valids[i] <= i_valid[i];
			end
		end
		
		if (staging_count_calc) begin
			staging_count = 'd0;
			for (i = 0; i < NI; i = i + 1) begin
				if (i_valid[i]) staging_count = staging_count + 1;
			end
		end
		else if (staging_count_dec) begin
			staging_count <= staging_count - 1;
		end
	end
end


wire pipe_enable = !(o_valid && !i_ready);
wire o_eop = o_data[EOP_LOC];
assign cur_input_load = pipe_enable && !(o_valid && i_ready && !o_eop);
assign next_input_sel = staging_count_thresh? FROM_STAGING : FROM_INPUT;
assign staging_count_calc = !staging_count_thresh && cur_input_load;
assign staging_count_dec = staging_count_thresh && cur_input_load;

always @* begin : readies
	integer i;
	for (i = 0; i < NI; i = i + 1) begin
		o_ready[i] = pipe_enable && (!staging_count_thresh || cur_input == i[NIBITS-1:0]);
	end
end

endmodule

