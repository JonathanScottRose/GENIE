module harness #
(
	integer WIDTH=1,
	integer NI=2
)
(
	input clk,
	input reset,
	
	input [NI-1:0][WIDTH-1:0] i_data,
	input [NI-1:0] i_valid,
	output reg [NI-1:0] o_ready,
	input [NI-1:0] i_eop,
	
	output o_valid,
	output [WIDTH-1:0] o_data,
	input i_ready,
	output o_eop
);

genie_merge #
(
	.WIDTH(WIDTH),
	.NI(NI)
)
dut
(
	.*
);

endmodule
