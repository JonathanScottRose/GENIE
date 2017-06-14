module harness #
(
	integer WIDTH=1
)
(
	input i_clk,
	input i_reset,
	
	input [WIDTH-1:0] i_data,
	input i_valid,
	
	output logic [WIDTH-1:0] o_data,
	output logic o_valid
);

genie_pipe_stage #
(
	.WIDTH(WIDTH)
)
dut
(
	.i_ready('1),
	.o_ready(),
	.*
);

endmodule
