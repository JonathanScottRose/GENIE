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
	
	input logic [WD-1:0] i_data,	// excludes input field!
	input logic [WIF-1:0] i_field,
	input logic i_valid,
	output logic o_ready,

	output logic [WD-1:0] o_data, // excludes output field!
	output logic [WOF-1:0] o_field,
	output logic o_valid,
	input logic i_ready
);

always_comb begin : mux
	// Do a parallel match of the i_field value against the parameter table
	// and mux in the single correct value of the output field value
    bit [N_ENTRIES-1:0] match;
	o_field = 0;
    match = '0;
		
	for (integer i = 0; i < N_ENTRIES; i++) begin
		match[i] = IF[WIF*i +: WIF] == i_field;
		o_field = o_field | (OF[WOF*i +: WOF] & {WOF{match[i]}});
	end
    
    assert(!i_valid || match) begin
        $error("invalid index %d at %0t", i_field, $time);
    end
end

// Assign outputs
always @* begin
	o_data = i_data;
	o_ready = i_ready;
	o_valid = i_valid;
end

endmodule

