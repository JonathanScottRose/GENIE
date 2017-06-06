module genie_mux #
(
	parameter lpm_widths = 0,
	parameter lpm_size = 0,
	parameter lpm_pipeline = 0,
	parameter lpm_width = 0
)
(
	input [(lpm_width*lpm_size)-1:0] data,
	input [lpm_widths-1:0] sel,
	output [lpm_width-1:0] result
);

reg [lpm_width-1:0] tmp;
reg match;

always @* begin : mux
	integer i;
	tmp = 0;
	
	for (i = 0; i < lpm_size; i = i + 1) begin
		match = i[lpm_widths-1:0] == sel;
		tmp = tmp | (data[i*lpm_width +: lpm_width] & {(lpm_width){match}});
	end
end

assign result = tmp;

endmodule



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
	wire [NI-1:0][NIBITS-1:0] sorted_idxs;

	genvar gi;
	generate
		for (gi = 0; gi < NI; gi = gi + 1) begin : sort
			// Generate a packed vector of all the input valid/IDX pairs
			logic [NI-1:0][NIBITS+1-1:0] sort_in;

			always @* begin : mksort_in
				for (integer i = 0; i < NI; i = i + 1) begin : mksort_in_loop
					integer in_idx;
					in_idx = (gi + i + 1) % NI;
					sort_in[i] = { i_valid[in_idx], in_idx[NIBITS-1:0] };
				end
			end
			
			// Do the sorting 
			assign {sorted_valids[gi], sorted_idxs[gi]} = sort_in[i_last_input];
		end
	endgenerate

	// Once we have sorted_valids/sorted_idx, this priority encoder
	// scans through sorted_valids and finds the first nonzero entry.
	// This guy will get the grant next. If no one is making a request,
	// cur_input stays at last_input
	always @* begin : pri_enc
		o_cur_input = sorted_idxs[NI-1]; // this corresponds to the last input
		
		for (integer i = NI-2; i >= 0; i = i - 1) begin
			if (sorted_valids[i]) o_cur_input = sorted_idxs[i];
		end
	end
endmodule
	
	

module genie_merge #
(
	parameter NI = 1,
	parameter WIDTH = 1
)
(
	input clk,
	input reset,
	
	input [NI-1:0][WIDTH-1:0] i_data,
	input [NI-1:0] i_valid,
	output reg [NI-1:0] o_ready,
	input [NI-1:0] i_eop,
	
	output o_valid,
	output [WIDTH-1:0] o_data,
	input i_ready,
	output o_eop
);

localparam NIBITS = $clog2(NI);

//
// Datapath
//

// Cur input: controls the mux, can be fed by either calced_input or last_input
// Calced input: calculated from valids and last_input by cur_input_calc
// Last input: last input to hold the grant
logic [NIBITS-1:0] cur_input;
logic [NIBITS-1:0] calced_input;
logic [NIBITS-1:0] last_input;
logic last_input_load;

// Mux select calculator (arbiter function)
cur_input_calc #
(
	.NI(NI),
	.NIBITS(NIBITS)
) calc
(
	.i_valid(i_valid),
	.i_last_input(last_input),
	.o_cur_input(calced_input)
);

// Output muxes
generate if (WIDTH > 0)
	assign o_data = i_data[cur_input];
endgenerate

assign o_eop = i_eop[cur_input];
assign o_valid = i_valid[cur_input];

// Controls last_input register
always_ff @ (posedge clk or posedge reset) begin
	if (reset) begin
		last_input <= 'd0;
	end
	else begin
		if (last_input_load) last_input <= calced_input;
	end
end

// Controls the readies of the inputs
always_comb begin
	for (integer i = 0; i < NI; i++) begin
		o_ready[i] = i_ready && (i == cur_input);
    end
end

// State machine to control packet handling
enum int unsigned
{
    S_FLOW_THROUGH,
    S_LOCKED
} state, nextstate;

always @ (posedge clk or posedge reset) begin
    if (reset) state <= S_FLOW_THROUGH;
    else state <= nextstate;
end

wire data_sent = (o_valid && o_ready);

always_comb begin
    nextstate = state;
    last_input_load = '0;
    cur_input = 'x;
    
    case (state)
        S_FLOW_THROUGH: begin
            cur_input = calced_input;
            last_input_load = '1;
            if (data_sent && !o_eop) nextstate = S_LOCKED;
        end
        
        S_LOCKED: begin
            cur_input = last_input;
            if (data_sent && o_eop) nextstate = S_FLOW_THROUGH;
        end
    endcase 
end

endmodule

