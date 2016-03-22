module dispatch #
(
	parameter WIDTH = 0
)
(
	input clk,
	input reset,
	
	output o_valid,
	input i_ready,
	output [WIDTH-1:0] o_data,
	output o_lp
);

reg [127:0] d_reg;
reg [15:0] v_reg;

always @ (posedge clk or posedge reset) begin
	if (reset) begin
		d_reg <= 128'hDEADBEEFBAADF00DFACEFEEDCAFEBABE;
		v_reg <= 16'hABED;
	end
	else begin
		if (o_valid && i_ready) begin
			d_reg <= {d_reg[WIDTH-1:0], d_reg[127:WIDTH]};
		end
		
		v_reg <= {v_reg[0], v_reg[15:1]};
	end
end

assign o_valid = v_reg[0];
assign o_data = d_reg[WIDTH-1:0];
assign o_lp = d_reg[0] & d_reg[1] | d_reg[2] ^ d_reg[3];

endmodule
