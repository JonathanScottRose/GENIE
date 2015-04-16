// Vendor-neutral implementation of Altera's LPM_MUX megafunction

module genie_mux #
(
	parameter lpm_widths = 0,
	parameter lpm_size = 0,
	parameter lpm_pipeline = 0,
	parameter lpm_width = 0
)
(
	input [(lpm_width*lpm_size)-1:0] data,
	input [lpm_widths-1:0] sel,
	output [lpm_width-1:0] result
);

reg [lpm_width-1:0] tmp;
reg match;

always @* begin : mux
	integer i;
	tmp = 0;
	
	for (i = 0; i < lpm_size; i = i + 1) begin
		match = i[lpm_widths-1:0] == sel;
		tmp = tmp | (data[i*lpm_width +: lpm_width] & {(lpm_width){match}});
	end
end

assign result = tmp;

endmodule
