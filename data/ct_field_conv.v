module ct_field_conv #
(
	parameter WD = 0,	// width of passthrough data (excluding the input and output fields!)
	parameter WIF = 0,	// width of input field
	parameter WOF = 0,	// width of output field
	parameter N_ENTRIES = 0,	// number of conversion table pairs
	parameter [N_ENTRIES*WIF-1:0] IF = 0,	// input field values
	parameter [N_ENTRIES*WOF-1:0] OF = 0	// corresponding output field values
)
(
	input clk,
	input reset,
	
	input [WD-1:0] i_data,	// excludes input field!
	input [WIF-1:0] i_field,
	input i_valid,
	output reg o_ready,

	output reg [WD-1:0] o_data, // excludes output field!
	output reg [WOF-1:0] o_field,
	output reg o_valid,
	input i_ready
);

// Extract input field from data stream (nevermind...)
wire [WIF-1:0] in_field = i_field;
reg [WOF-1:0] out_field;

always @* begin : mux
	integer i;
	reg match;
	
	// Do a parallel match of the in_field value against the parameter table
	// and mux in the single correct value of the output field value
	out_field = 0;
		
	for (i = 0; i < N_ENTRIES; i = i + 1) begin
		match = IF[WIF*i +: WIF] == in_field;
		out_field = out_field | (OF[WOF*i +: WOF] & {WOF{match}});
	end
end

// Assign output data. Copy the everthing-but-the-field stuff
always @* begin
	o_data = i_data;
	o_field = out_field;
	o_ready = i_ready;
	o_valid = i_valid;
end

endmodule

