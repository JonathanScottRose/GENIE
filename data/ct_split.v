module ct_split #
(
	parameter NO, // Number of outputs
	parameter WO, // Width of data input/output
	parameter NF, // Number of flows registered with this node
	parameter WF, // Width of the flow_id signal
	parameter [NF*WF-1:0] FLOWS, // Vector of flow_id for each flow
	parameter [NF*NO-1:0] ENABLES, // Vector of targeted outputs for each flow
	parameter FLOW_LOC // Bit location of flow_id within data stream
)
(
	input clk,
	input reset,
	
	input [WO-1:0] i_data,
	input i_valid,
	output reg o_ready,

	output reg [(NO*WO)-1:0] o_data,
	output reg [NO-1:0] o_valid,
	input [NO-1:0] i_ready
);

// Extract flow_id from data stream
wire flow_id = i_data[FLOW_LOC +: WF];

// Calculate which outputs the incoming flow is trying to send data on.
// There are NF flows known to this split node. The FLOWS parameter
// contains the flow_id of each of these flows. The ENABLES parameter
// contains a bit vector, for each flow, which says which combination
// of NO outputs is targeted by that flow.
reg [NO-1:0] enabled_outputs;
always @* begin
	integer i;
	reg match;
	
	// Up to one of the statically known flows matches the dynamic incoming flow_id.
	// For this output, if it exists, match=1
	// Use this to mux the correct vector-of-enabled-outputs for this flow.
	enabled_outputs = 0;
	
	for (i = 0; i < NO; i = i + 1) begin
		match = FLOWS[WF*i +: WF] == flow_id;
		enabled_outputs = enabled_outputs | (ENABLES[NO*i +: NO] & {NO{match}});
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
always @ (posedge clk or posedge reset) begin
	integer i;

	if (reset) done_outputs = 0;
	else begin
		for (i = 0; i < NO; i = i + 1) begin
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
always @* begin
	integer i;
	
	// Initialize to 1 to perform a wide-AND
	o_ready = 1'b1;
	
	for (i = 0; i < NO; i = i + 1) begin
		// Same data gets broadcast to everyone
		o_data[i*WO +: WO] = i_data;
		
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

