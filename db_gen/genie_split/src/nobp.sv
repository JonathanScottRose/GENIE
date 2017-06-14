module harness #
(
	parameter WIDTH=1,
	parameter N=2,
	parameter NO_MULTICAST=0
)
(
	input clk,
	input reset,
	
	input logic [WIDTH-1:0] i_data,
	input logic i_valid,
	input logic [N-1:0] i_mask,

	output logic [WIDTH-1:0] o_data,
	output logic [N-1:0] o_valid
);

genie_split #
(
	.WIDTH(WIDTH),
	.N(N),
	.NO_MULTICAST(NO_MULTICAST)
)
dut
(
	.i_ready('1),
	.o_ready(),
	.*
);

endmodule
