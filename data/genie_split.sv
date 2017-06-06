module genie_split #
(
	integer N = 1, // Number of outputs
	integer WIDTH = 1, // Width of data input/output
	integer NO_MULTICAST = 0
)
(
	input clk,
	input reset,
	
	input logic [WIDTH-1:0] i_data,
	input logic i_valid,
	input logic [N-1:0] i_mask,
	output logic o_ready,

	output logic [WIDTH-1:0] o_data,
	output logic [N-1:0] o_valid,
	input logic [N-1:0] i_ready
);

// This implements lazy forking. If multiple outputs are trying to be multicasted to,
// then not all of them may be ready during one cycle. For the ones that are, they
// get one cycle's worth of valid data, and then are cut off until everyone else
// also becomes ready and receives that same data. 
//
// done_outputs keeps track of which outputs have already received their data
// and are simply waiting for everyone else to be ready and get their copy
// of that same data successfully transmitted.
reg [N-1:0] done_outputs;
generate if (NO_MULTICAST) begin : no_mcast
	always_comb done_outputs <= '0;
end
else begin : mcast
	always_ff @ (posedge clk or posedge reset) begin : forking
		if (reset) done_outputs <= 0;
		else begin
			for (integer i = 0; i < N; i++) begin
				// Set when output i successfully transfers data, but at least one
				// other output doesn't (which causes o_ready to be low)
				if (o_valid[i] && i_ready[i] && !o_ready) done_outputs[i] <= 1'b1;
				// Clear if during this cycle, everyone who hasn't got their data yet gets
				// their data (covers the ideal 'everyone' case too).
				else if (i_valid && o_ready) done_outputs[i] <= 1'b0;
			end
		end
	end
end
endgenerate

// Assign all outputs
always_comb begin : assignment
	// Same data gets broadcast to everyone
	o_data = i_data;
	
	// Initialize to 1 to perform a wide-AND
	o_ready = 1'b1;
	
	for (integer i = 0; i < N; i++) begin
		// Broadcast the valid signal if the output is targeted/enabled and it hasn't already
		// received/transferred the data yet
		o_valid[i] = i_valid & i_mask[i] & !done_outputs[i];
		
		// From the point of view of the input, this output is not-stalling if any of these hold:
		// - it's not targeted by the current data stream
		// - is's already received the data
		// - the output is ready (not-stalling) itself
		o_ready = o_ready & (!i_mask[i] | done_outputs[i] | i_ready[i]);
	end
end

endmodule

