module harness #
(
	integer WIDTH=1,
	integer CYCLES=2
)
(
	input clk,
	input reset,
	
	input logic [WIDTH-1:0] i_data,
	input logic i_valid,
	
	output logic [WIDTH-1:0] o_data,
	output logic o_valid
);

genie_mem_delay #
(
	.WIDTH(WIDTH),
	.CYCLES(CYCLES)
)
dut
(
	.i_ready('1),
	.o_ready(),
	.*
);

endmodule
