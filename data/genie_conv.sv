module genie_conv #
(
	integer WIDTH_DATA = 0,	// width of carried data
	integer WIDTH_IN = 0,	// width of input field
	integer WIDTH_OUT = 0,	// width of output field
	integer N_ENTRIES = 0,	// number of conversion table pairs
	bit [N_ENTRIES-1:0][WIDTH_IN-1:0] IN_VALS,  // input field values
	bit [N_ENTRIES-1:0][WIDTH_OUT-1:0] OUT_VALS // corresponding output field values
)
(
	input clk,
	input reset,
	
	input logic i_valid,
	output logic o_ready,
	input logic [WIDTH_DATA-1:0] i_data,
	input logic [WIDTH_IN-1:0] i_in,
	
	output logic o_valid,
	input logic i_ready,
	output logic [WIDTH_DATA-1:0] o_data,
	output logic [WIDTH_OUT-1:0] o_out
);

bit [N_ENTRIES-1:0] match;

always_comb begin : mux
	// Do a parallel match of the i_field value against the parameter table
	// and mux in the single correct value of the output field value
	o_out = '0;
    match = '0;
	
	for (integer i = 0; i < N_ENTRIES; i++) begin
		// Check for a match (there should only be one).
		// Use this behavior to try and coax a parallel tree instead of a priority encoder.
		if (i_in == IN_VALS[i]) begin
			match[i] = '1;
			o_out |= OUT_VALS[i];
		end
		// match[i] = i_in == IN_VALS[i];
		// o_out = o_out | (  & {WIDTH_OUT{match[i]}} );
	end
end

// For sanity checking during simulation
assert property (@(posedge clk) disable iff (reset) !(i_valid && !match));

// Data pass-through
assign o_data = i_data;
assign o_valid = i_valid;
assign o_ready = i_ready;

endmodule

