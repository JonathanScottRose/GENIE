import lu_new::*;

module cpu_ram_addr_swiz #
(
	parameter TOTAL_BITS = 0,
	parameter SWIZ_BITS = 0,
	parameter LANE = 0
)
(
	input clk,
	input reset,
	
	input [TOTAL_BITS-1:0] i_addr,
	output logic [TOTAL_BITS-SWIZ_BITS-1:0] o_addr
);

localparam RESULT_BITS = TOTAL_BITS - SWIZ_BITS;

wire comp_result = i_addr[SWIZ_BITS-1:0] > ((SWIZ_BITS)'(LANE));

always_ff @ (posedge clk) begin
	o_addr <= i_addr[TOTAL_BITS-1 : SWIZ_BITS] + ((RESULT_BITS)'(comp_result));
end

endmodule
