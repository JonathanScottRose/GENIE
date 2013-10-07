module tb();

logic xorro_out_valid;
logic xorro_out_ready;
logic clk;
logic [15:0] xorro_out_data;
logic reset;

sm_test dut
(
	.*
);

initial clk = 1'b1;
always #5 clk = ~clk;

initial begin
	xorro_out_ready = '1;

	reset = '1;
	#50;
	reset = '0;
end

endmodule
