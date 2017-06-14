module genie_merge_ex #
(
	parameter NI = 1,
	parameter WIDTH = 1
)
(
	input clk,
	input reset,
	
	input [NI-1:0][WIDTH-1:0] i_data,
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
generate 
if (WIDTH > 0) begin
    if (NI <= 1) begin
        error must_have_more_than_one_input();
    end
    if (NI == 2) begin
        assign o_eop = i_valid[0] ? i_eop[0] : i_eop[1];
        assign o_data = i_valid[0] ? i_data[0] : i_data[1];
		assert property (@(posedge clk) disable iff (reset) (!i_valid[0] || !i_valid[1]));
    end
    else if (NI > 2) begin
		// synthesis translate_off
		int n_valids;
		// synthesis translate_on
		
        always_comb begin : one_hot_mux	
            o_eop = '0;
            o_data = '0;
			// synthesis translate_off
			n_valids = 0;
			// synthesis translate_on
            
            for (int i = 0; i < NI; i++) begin
				if (i_valid[i]) begin
					o_eop |= i_eop[i];
					o_data |= i_data[i];
					// synthesis translate_off
					n_valids++;
					// synthesis translate_on
				end
            end
        end
		
		// synthesis translate_off
		assert property (@(posedge clk) disable iff (reset) (n_valids <= 1));
		// synthesis translate_on
    end
end
endgenerate

assign o_valid = |i_valid;
assign o_ready = {NI{i_ready}};

endmodule

