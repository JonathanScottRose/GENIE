module harness #
(
	integer WIDTH=1
)
(
	input arst,
	input wrclk,
	input rdclk,
	
	input [WIDTH-1:0] i_data,
	input i_valid,
	output logic o_ready,
	
	output logic [WIDTH-1:0] o_data,
	output logic o_valid,
	input i_ready
);

genie_clockx #
(
	.WIDTH(WIDTH)
)
dut
(
	.*
);

endmodule
