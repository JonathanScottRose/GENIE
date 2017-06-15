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
	
	wire xfer0 = valid0 && i_ready;
	wire xfer1 = valid1 && ready0;
	
	always_ff @ (posedge i_clk or posedge i_reset) begin
		if (i_reset) begin
			valid0 <= '0;
			valid1 <= '0;
			ready0 <= '1;
			data0 <= 'x;
			data1 <= 'x;
		end
		else begin
			ready0 <= !valid0 || i_ready;
			
			if (!valid0 || i_ready) begin
				valid0 <= ready0? i_valid : valid1;
				data0 <= ready0? i_data : data1;
			end
			
			valid1 <= i_valid;
			data1 <= i_data;
		end
	end
	
	assign o_data = data0;
	assign o_valid = valid0;
	assign o_ready = ready0;
	
endmodule
