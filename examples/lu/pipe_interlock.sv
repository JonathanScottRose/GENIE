module pipe_interlock #
(
	parameter WIDTH = 1,
	parameter REGISTERED = 1,
	parameter MLAB = 0
)
(
	input i_clk,
	input i_reset,
	
	input [WIDTH-1:0] i_in,
	input i_have,
	output logic o_want,
	
	output logic [WIDTH-1:0] o_out,
	output logic o_have,
	input i_want
);

generate
if (MLAB) begin : mlab
	logic full;
	logic empty;
	
	scfifo #
	(
		.lpm_width(WIDTH),
		.lpm_numwords(32),
		.lpm_widthu(5),
		.lpm_showahead("ON")
		// synthesis read_comments_as_HDL on
		//, .ram_block_type("MLAB")
		// synthesis read_comments_as_HDL off
	) fifo
	(
		.clock(i_clk),
		.aclr(i_reset),
		.data(i_in),
		.wrreq(i_have),
		.full(full),
		.rdreq(i_want),
		.q(o_out),
		.empty(empty)
	);
	
	assign o_want = !full;
	assign o_have = !empty;
end
else begin : regs
	logic [WIDTH-1:0] i2o_saved_data;
	logic [WIDTH-1:0] i2o_data;
	logic i2o_have;

	if (REGISTERED) begin : registered
		always_ff @ (posedge i_clk or posedge i_reset) begin
			if (i_reset) begin
				i2o_have <= 1'b0;
			end
			else if (o_want) begin
				i2o_have <= i_have;
				i2o_data <= i_in;
			end
		end
	end
	else begin
		assign i2o_have = i_have;
		assign i2o_data = i_in;
	end
	
	always_ff @ (posedge i_clk or posedge i_reset) begin
		if (i_reset) begin
			i2o_saved_data <= 'bx;
		end
		else if (o_want) begin
			i2o_saved_data <= i2o_data;
		end
	end

	logic blocked;
	always_ff @ (posedge i_clk or posedge i_reset) begin
		if (i_reset) begin
			blocked <= 1'b0;
		end
		else begin
			case (blocked)
				1'b0: begin
					if (i2o_have && !i_want) blocked <= 1'b1;
				end
				
				1'b1: begin
					if (i_want) blocked <= 1'b0;
				end
			endcase
		end
	end

	always_comb begin
		case (blocked)
			1'b0: begin
				o_have = i2o_have;
				o_out = i2o_data;
				o_want = 1'b1;
			end
			
			1'b1: begin
				o_have = 1'b1;
				o_out = i2o_saved_data;
				o_want = 1'b0;
			end
		endcase
	end
end
endgenerate


endmodule
