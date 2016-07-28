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
)

localparam CYCLES3 = $clog2(CYCLES);

logic [CYCLES3-1:0] rdptr;
logic [CYCLES3-1:0] wrptr;
logic [CYCLES-1:0] valid;

wire pipe_enable = !o_valid || i_ready;

always_ff @ (posedge clk or posedge reset) begin
	if (reset) begin
		rdptr <= '0;
		wrptr <= (CYCLES3)'(CYCLES);
		valid <= '0;
	end
	else if (pipe_enable) begin
		// Shift register for valid signal
		valid <= {valid[CYCLES-2:1], i_valid};
		
		// Pointers always increasing
		rdptr <= (rdptr == (CYCLES3)'(CYCLES-1))? '0 : rdptr + (CYCLES3)'(1);
		wrptr <= (wrptr == (CYCLES3)'(CYCLES-1))? '0 : wrptr + (CYCLES3)'(1);
	end
end

// Data storage
(* ramstyle = "MLAB,no_rw_check" *)
logic [WIDTH-1:0] mem [CYCLES:0];

always_ff @ (posedge clk)
	if (pipe_enable) mem[wrptr] <= i_data;

assign o_valid = valid[CYCLES-1];
assign o_data = mem[rdptr];


endmodule
