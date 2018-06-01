module genie_pipe_stage #
(
	parameter WIDTH = 1
)
(
	input i_clk,
	input i_reset,
	
	input [WIDTH-1:0] i_data,
	input i_valid,
	output logic o_ready,
	
	output logic [WIDTH-1:0] o_data,
	output logic o_valid,
	input i_ready
);
	logic [WIDTH-1:0] data0, data1;
	logic valid0, valid1;
	logic ready0;
	logic nready0;
	
	wire xfer = !valid0 || i_ready;
	
	always_ff @ (posedge i_clk or posedge i_reset) begin
		if (i_reset) begin
			ready0 <= '0;
			valid1 <= '0;
			data1 <= 'x;
		end
		else begin
			ready0 <= xfer;
			
			if (ready0) begin
				valid1 <= i_valid;
				data1 <= i_data;
			end
		end
	end
	
	always_ff @ (posedge i_clk or posedge i_reset) begin
		if (i_reset) begin
			nready0 <= '0;
			valid0 <= '0;
			data0 <= 'x;
		end
		else begin
			nready0 <= xfer ? 1'b0 : 1'b1;
			
			if (xfer) begin
				data0 <= nready0? data1 : i_data;
				valid0 <= nready0? valid1 : i_valid;
			end
		end
	end
	
	assign o_data = data0;
	assign o_valid = valid0;
	assign o_ready = ready0;
	
endmodule
