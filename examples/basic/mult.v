module mult #
(
	parameter WIDTH
)
(
	input clk,
	
	input i_a_valid,
	output o_a_ready,
	input [WIDTH-1:0] i_a,
	
	input i_b_valid,
	output o_b_ready,
	input [WIDTH-1:0] i_b,
	
	output o_result_valid,
	output i_result_ready,
	output [WIDTH-1:0] o_result
);

wire pipe_enable;
reg valid;
reg [2*WIDTH-1:0] result;

always @ (posedge clk) begin
	if (pipe_enable) begin
		valid <= i_a_valid && i_b_valid;
		result <= i_a * i_b;
	end
end

assign o_a_ready = pipe_enable;
assign o_b_ready = pipe_enable;

assign o_result = result[2*WIDTH-1:WIDTH];
assign o_result_valid = valid;

assign pipe_enable = !(valid && !i_result_ready);

endmodule
