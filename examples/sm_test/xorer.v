module xorer #
(
	parameter WIDTH = 0
)
(
	input clk,
	input reset,
	
	input i_valid,
	output o_ready,
	input [WIDTH-1:0] i_data,
	input [3:0] i_lp,
	
	output o_valid,
	input i_ready,
	output [WIDTH-1:0] o_data
);

reg [WIDTH-1:0] result;
reg valid;
wire pipeline_enable;

always @ (posedge clk or posedge reset) begin
	if (reset) begin
		valid <= 1'b0;
		result <= 0;
	end
	else begin
		if (pipeline_enable) begin
			valid <= i_valid;
			if (i_valid) result <= (result ^ i_data) + i_lp;
		end
	end
end

assign o_valid = valid;
assign o_data = result;

assign pipeline_enable = !(o_valid && !i_ready);
assign o_ready = pipeline_enable;

endmodule
