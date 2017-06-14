module genie_mem_delay #
(
	integer WIDTH,
	integer CYCLES
)
(
	input clk,
	input reset,
	
	input logic [WIDTH-1:0] i_data,
	output logic o_ready,
	input logic i_valid,
	
	output logic [WIDTH-1:0] o_data,
	output logic o_valid,
	input logic i_ready
);

localparam CYCLES3 = $clog2(CYCLES);
localparam MEMSIZE = 1 << CYCLES3;

logic [CYCLES3-1:0] rdptr;
logic [CYCLES3-1:0] wrptr;
logic [CYCLES-1:0] valid;

logic stalled; // state bit
logic saved_valid; // gets copy of incoming i_valid at input of shiftreg

always_ff @ (posedge clk or posedge reset) begin
	if (reset) begin
		rdptr <= '0;
		wrptr <= (CYCLES3)'(CYCLES);
		valid <= '0;
		stalled <= '0;
		saved_valid <= '0;
	end
	else begin
		// Output accepted = i_ready.
		if (i_ready) begin
			rdptr <= rdptr + (CYCLES3)'(1);
			saved_valid <= i_valid;
			
			// Advance shiftreg chain only when output is ready.
			// The first bit can come from one of two places
			valid <= {valid[CYCLES-2:0], stalled? saved_valid : valid[0]};
		end
			
		stalled <= !i_ready;
		
		// Advance write pointer only if read side is not in stalled state
		if (!stalled) begin
			wrptr <= wrptr + (CYCLES3)'(1);
			// saved_valid holds the trampled-over version of this
			valid[0] <= i_valid;
		end
	end
end

// Data storage
(* ramstyle = "MLAB,no_rw_check" *)
logic [WIDTH-1:0] mem [CYCLES:0];

always_ff @ (posedge clk)
	if (!stalled) mem[wrptr] <= i_data;

assign o_valid = valid[CYCLES-1];
assign o_data = mem[rdptr];
assign o_ready = !stalled;


endmodule
