module genie_split #
(
	parameter NO = 1, // Number of outputs
	parameter WO = 1, // Width of data input/output
	parameter NF = 1, // Number of flows registered with this node
	parameter WF = 1, // Width of the flow_id signal
	parameter [NF*WF-1:0] FLOWS = 0, // Vector of flow_id for each flow
	parameter [NF*NO-1:0] ENABLES = 0 // Vector of targeted outputs for each flow
)
(
	input clk,
	input reset,
	
	input logic [WO-1:0] i_data,
	input logic i_valid,
	input logic [WF-1:0] i_flow,
	output logic o_ready,

	output logic [WO-1:0] o_data,
	output logic [NO-1:0] o_valid,
	output logic [WF-1:0] o_flow,
	input logic [NO-1:0] i_ready
);

// Calculate which outputs the incoming flow is trying to send data on.
// There are NF flows known to this split node. The FLOWS parameter
// contains the flow_id of each of these flows. The ENABLES parameter
// contains a bit vector, for each flow, which says which combination
// of NO outputs is targeted by that flow.
logic [NO-1:0] enabled_outputs;
always_comb begin : mux
    // Up to one of the statically known flows matches the dynamic incoming flow_id.
	// For this output, if it exists, match=1
	// Use this to mux the correct vector-of-enabled-outputs for this flow.
	bit[NF-1:0] match;
    match = '0;
	enabled_outputs = '0;
	
	for (integer i = 0; i < NF; i++) begin
		match[i] = FLOWS[WF*i +: WF] == i_flow;
		enabled_outputs = enabled_outputs | (ENABLES[NO*i +: NO] & {NO{match[i]}});
	end
    
    assert (reset !== 1'b0 || i_valid !== 1'b1 || match) else begin
        $error("unknown flow_id %u at %0t", i_flow, $time);
    end
end

// This implements lazy forking. If multiple outputs are trying to be multicasted to,
// then not all of them may be ready during one cycle. For the ones that are, they
// get one cycle's worth of valid data, and then are cut off until everyone else
// also becomes ready and receives that same data. 
//
// done_outputs keeps track of which outputs have already received their data
// and are simply waiting for everyone else to be ready and get their copy
// of that same data successfully transmitted.
reg [NO-1:0] done_outputs;
always_ff @ (posedge clk or posedge reset) begin : forking
	if (reset) done_outputs <= 0;
	else begin
		for (integer i = 0; i < NO; i++) begin
			// Set when output i successfully transfers data, but at least one
			// other output doesn't (which causes o_ready to be low)
			if (o_valid[i] && i_ready[i] && !o_ready) done_outputs[i] <= 1'b1;
			// Clear if during this cycle, everyone who hasn't got their data yet gets
			// their data (covers the ideal 'everyone' case too).
			else if (i_valid && o_ready) done_outputs[i] <= 1'b0;
		end
	end
end

// Assign all outputs
always_comb begin : assignment
	// Same data gets broadcast to everyone
	o_data = i_data;
	o_flow = i_flow;
	
	// Initialize to 1 to perform a wide-AND
	o_ready = 1'b1;
	
	for (integer i = 0; i < NO; i++) begin
		// Broadcast the valid signal if the output is targeted/enabled and it hasn't already
		// received/transferred the data yet
		o_valid[i] = i_valid & enabled_outputs[i] & !done_outputs[i];
		
		// From the point of view of the input, this output is not-stalling if any of these hold:
		// - it's not targeted by the current data stream
		// - is's already received the data
		// - the output is ready (not-stalling) itself
		o_ready = o_ready & (!enabled_outputs[i] | done_outputs[i] | i_ready[i]);
	end
end

endmodule

