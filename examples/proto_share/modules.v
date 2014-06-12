module period_gen #
(
	parameter PERIOD=1,
	parameter WIDTH=1,
	parameter [WIDTH*PERIOD-1:0] PATTERN=1
)
(
	input clk,
	input reset,
	input enable,
	output [WIDTH-1:0] out
);
	reg [WIDTH*PERIOD-1:0] sr;
	
	always @ (posedge clk or posedge reset) begin
		if (reset) sr <= PATTERN;
		else if (enable) sr <= {sr[WIDTH-1:0], sr[WIDTH*PERIOD-1 : WIDTH]};
	end
	
	assign out = sr[WIDTH-1:0];
endmodule



module A_out
(
	input clk,
	input reset,
	output o_valid,
	input i_ready
);
	wire strobe;
	period_gen #(2, 1, 2'b1) p (clk, reset, 1'b1, strobe);
	assign o_valid = strobe;
endmodule



module A_in
(
	input clk,
	input reset,
	input i_valid,
	output o_ready
);
	period_gen #(3, 1, 3'b1) p (clk, reset, 1'b1, o_ready);
endmodule



module B_out
(
	input clk,
	input reset,
	output o_valid,
	input i_ready,
	output [15:0] o_data
);
	period_gen #(5, 16, 80'hBAD4BAD3BAD2BAD1BAD0) p (clk, reset, !(o_valid && !i_ready), o_data);
	assign o_valid = 1'b1;
endmodule


module B_in
(
	input clk,
	input reset,
	input i_valid,
	output o_ready,
	input [15:0] i_data
);
	period_gen #(2, 1, 2'b1) p (clk, reset, 1'b1, o_ready);
endmodule


module C_out
(
	input clk,
	input reset,
	output o_valid,
	input i_ready,
	output [3:0] o_x,
	output [4:0] o_y,
	output [5:0] o_z,
	output o_sop,
	output o_eop
);
	wire enable = !(o_valid && !i_ready);
	wire pkt_enable = o_valid && enable;

	period_gen #(5, 4, {4'd1, 4'd2, 4'd3, 4'd4, 4'd5}) px(clk, reset, pkt_enable, o_x);
	period_gen #(5, 5, {5'd1, 5'd2, 5'd3, 5'd4, 5'd5}) py(clk, reset, pkt_enable, o_y);
	period_gen #(5, 6, {6'd1, 6'd2, 6'd3, 6'd4, 6'd5}) pz(clk, reset, pkt_enable, o_z);
	period_gen #(5, 1, 5'b00001) psop(clk, reset, pkt_enable, o_sop);
	period_gen #(5, 1, 5'b10000) peop(clk, reset, pkt_enable, o_eop);
	
	period_gen #(11, 1, 11'b00000011111) pv(clk, reset, enable, o_valid);
endmodule

module C_in
(
	input clk,
	input reset,
	input i_valid,
	output o_ready,
	input [3:0] i_x,
	input [4:0] i_y,
	input [5:0] i_z,
	input i_sop,
	input i_eop
);
	assign o_ready = 1'b1;
endmodule



module D_out
(
	input clk,
	input reset,
	output o_valid,
	input i_ready,
	output [23:0] o_data,
	output [3:0] o_x
);
	assign o_data = 24'hDEADED;
	assign o_x = 4'hD;
	
	period_gen #(13, 1, 13'd1) p (clk, reset, 1'b1, o_valid);
endmodule

module D_in
(
	input clk,
	input reset,
	input i_valid,
	output o_ready,
	input [23:0] i_data,
	input [3:0] i_x
);
	period_gen #(2, 1, 2'd1) p (clk, reset, 1'b1, o_ready);
endmodule
