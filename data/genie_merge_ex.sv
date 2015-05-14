module genie_merge_ex #
(
	parameter NI = 1,
	parameter WIDTH = 1
)
(
	input clk,
	input reset,
	
	input [NI*WIDTH-1:0] i_data,
	input [NI-1:0] i_valid,
	output logic [NI-1:0] o_ready,
	input [NI-1:0] i_eop,
	
	output logic o_valid,
	output logic [WIDTH-1:0] o_data,
	input i_ready,
	output logic o_eop
);

// This version of the merge node is for merging inputs that have been
// guaranteed to never compete.

// Payload: assuming up to one valid[] is true at any time, make a very simple mux
generate if (WIDTH > 0)
always_comb begin : one_hot_mux
    o_eop = '0;
    o_data = '0;
    
    for (int i = 0; i < NI; i++) begin
        o_eop = o_eop | (i_valid[i] & i_eop[i]);
        o_data = o_data | ({WIDTH{i_valid[i]}} & i_data[WIDTH*i +: WIDTH]);
    end
end
endgenerate

assign o_valid = |i_valid;
assign o_ready = {NI{i_ready}};

endmodule

