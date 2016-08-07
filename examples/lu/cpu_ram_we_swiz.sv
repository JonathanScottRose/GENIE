import lu_new::*;

module cpu_ram_we_swiz #
(
	parameter COORD_BITS = 0,
	parameter SWIZ_BITS = 0,
	parameter LANE = 0
)
(
	input clk,
	input reset,
	
	input [2*COORD_BITS-1:0] i_addr,
	input i_we,
	output logic o_we
);

localparam MAX_COORD_BASE = (2**COORD_BITS) - (2**SWIZ_BITS);

wire comp_result = i_addr[COORD_BITS-1:0] <= ((COORD_BITS)'(MAX_COORD_BASE + LANE));

always_ff @ (posedge clk or posedge reset) begin
	if (reset) o_we <= '0;
	else o_we <= i_we & comp_result;
end

endmodule
