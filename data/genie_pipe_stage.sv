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
	
	wire xfer = !valid0 || i_ready;
	
	always_ff @ (posedge i_clk or posedge i_reset) begin
		if (i_reset) begin
			ready0 <= '1;
			valid0 <= '0;
		end
		else begin
			ready0 <= xfer;
		
			if (xfer) begin
				valid0 <= ready0? i_valid : valid1;
			end
			
			if (ready0) begin
				valid1 <= i_valid;
				data1 <= i_data;
			end
		end
	end
	
	// synthesis translate_off
	always_ff @ (posedge i_clk) begin
		if (xfer) begin
			data0 <= ready0? i_data : data1;
		end
	end
	// synthesis translate_on
	
	// synthesis read_comments_as_HDL on
	// genvar i;
	// generate
	// for (i = 0; i < WIDTH; i++) begin : data_regs
	//     dffeas data0_reg
	//     (
	//         .d(i_data[i]),
	//         .clk(i_clk),
	//         .clrn(~i_reset),
	//         .ena(xfer),
	//         .asdata(data1[i]),
	//         .sload(ready0),
	//         .q(data0[i]),
	//         .prn(1'b1),
	//         .aload(1'b0),
	//         .sclr(1'b0)
	//     );
	// end
	// endgenerate
	// synthesis read_comments_as_HDL off
	
	assign o_data = data0;
	assign o_valid = valid0;
	assign o_ready = ready0;
	
endmodule
