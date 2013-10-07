module reverser #
(
	parameter WIDTH = 0
)
(
	input clk,
	input reset,
	
	input i_valid,
	output o_ready,
	input [WIDTH-1:0] i_data,
	
	output o_valid,
	input i_ready,
	output [WIDTH-1:0] o_data
);

reg [WIDTH-1:0] result;
reg valid;
wire pipeline_enable;

always @ (posedge clk or posedge reset) begin : seq
	integer i;
	
	if (reset) valid <= 1'b0;
	else begin
		if (pipeline_enable) begin
			valid <= i_valid;
			
			for (i = 0; i < WIDTH; i = i + 1) begin
				result[i] <= i_data[WIDTH-1-i];
			end
		end
	end
end

assign o_valid = valid;
assign o_data = result;

assign pipeline_enable = !(o_valid && !i_ready);
assign o_ready = pipeline_enable;

endmodule
