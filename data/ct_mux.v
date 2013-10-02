// Vendor-neutral implementation of Altera's LPM_MUX megafunction

module ct_mux #
(
	parameter lpm_widths,
	parameter lpm_size,
	parameter lpm_pipeline,
	parameter lpm_width
)
(
	input [(lpm_width*lpm_size)-1:0] data,
	input [lpm_widths-1:0] sel,
	output [lpm_width-1:0] result
);

reg [lpm_width-1:0] tmp;
reg match;

always @* begin
	integer i;
	tmp = 0;
	
	for (i = 0; i < lpm_size; i = i + 1) begin
		match = i[lpm_widths-1:0] == sel;
		tmp = tmp | (data[i*lpm_width +: lpm_width] & {(lpm_width){match}});
	end
end

assign result = tmp;

endmodule
