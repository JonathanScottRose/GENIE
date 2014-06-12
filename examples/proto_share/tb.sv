module tb();

	logic clk;
	logic reset;
	
	initial clk = '0;
	always #10 clk = ~clk;
	
	initial begin
		reset = '1;
		#11
		reset = '0;
	end
	
	sys dut (.*);
endmodule
