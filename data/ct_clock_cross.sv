module ct_clock_cross #
(
	integer WIDTH = 256
)
(
	input arst,
	input wrclk,
	input rdclk,
	
	input [WIDTH-1:0] i_data,
	input i_valid,
	output logic o_ready,
	
	output logic [WIDTH-1:0] o_data,
	output logic o_valid,
	input i_ready
);

localparam RAM_ADDR_WIDTH = 5;
localparam SYNC_STAGES = 3;

wire rdarst, wrarst;
reg rdempty;
wire rdpipe_en = !(o_valid && !i_ready);
assign o_valid = ~rdempty; 

//////////////////////////////////////////
// reset distribution
//////////////////////////////////////////
reg [1:0] wrfilter = 0 /* synthesis preserve */;
always @(posedge wrclk or posedge arst) begin
	if (arst) wrfilter <= 2'b00;
	else wrfilter <= {wrfilter[0],1'b1};
end
assign wrarst = ~wrfilter[1];

reg [1:0] rdfilter = 0 /* synthesis preserve */;
always @(posedge rdclk or posedge arst) begin
	if (arst) rdfilter <= 2'b00;
	else rdfilter <= {rdfilter[0],1'b1};
end
assign rdarst = ~rdfilter[1];

//////////////////////////////////////////
// read pointers
//////////////////////////////////////////

reg [RAM_ADDR_WIDTH:0] rdgray = 0 /* synthesis preserve */
/* synthesis ALTERA_ATTRIBUTE = "-name SDC_STATEMENT \"set_false_path -from [get_keepers *ct_clock_cross*rdgray\[*\]]\" " */;

reg [RAM_ADDR_WIDTH:0] rdbin = 0 /* synthesis preserve */;
initial rdempty = 1'b1;
wire [RAM_ADDR_WIDTH:0] rdbin_next = rdbin + (rdpipe_en & ~rdempty);
wire [RAM_ADDR_WIDTH:0] rdgray_next = (rdbin_next >> 1'b1) ^ rdbin_next;
wire [RAM_ADDR_WIDTH:0] sync_wrptr;

always @(posedge rdclk or posedge rdarst) begin
	if (rdarst) begin
		rdbin <= 0;
		rdgray <= 0;
	end
	else begin
		rdbin <= rdbin_next;
		rdgray <= rdgray_next;		
	end
end

always @(posedge rdclk) begin
	rdempty <= (rdgray_next == sync_wrptr);	
end

//////////////////////////////////////////
// write pointers
//////////////////////////////////////////

reg wrfull;
wire wrreq = i_valid;
assign o_ready = !wrfull;

reg [RAM_ADDR_WIDTH:0] wrgray = 0, wrbin = 0 /* synthesis preserve */;
initial wrfull = 1'b1;

//timing modification 
//wire [RAM_ADDR_WIDTH:0] wrbin_next = wrbin + (wrreq & ~wrfull);
wire [RAM_ADDR_WIDTH:0] wrbin_plus = wrbin + 1'b1 /* synthesis keep */; 
wire [RAM_ADDR_WIDTH:0] wrbin_next = (~wrfull & wrreq) ? wrbin_plus : wrbin;

wire [RAM_ADDR_WIDTH:0] wrgray_next = (wrbin_next >> 1'b1) ^ wrbin_next /* synthesis keep */;
wire [RAM_ADDR_WIDTH:0] sync_rdptr;

always @(posedge wrclk or posedge wrarst) begin
	if (wrarst) begin
		wrbin <= 0;
		wrgray <= 0;
		wrfull <= 1'b1;
	end
	else begin
		wrbin <= wrbin_next;
		wrgray <= wrgray_next;
		wrfull <= (wrgray_next == 
				{sync_rdptr[RAM_ADDR_WIDTH] ^ 1'b1,
				sync_rdptr[RAM_ADDR_WIDTH-1] ^ 1'b1,
				sync_rdptr[RAM_ADDR_WIDTH-2:0]});
	end
end

//////////////////////////////////////////
// domain synchronizers
//////////////////////////////////////////

// stall the write a little more to give it time to settle in the RAM
// before reporting over to the read side
reg [RAM_ADDR_WIDTH:0] wrgray_rr;
reg [RAM_ADDR_WIDTH:0] wrgray_r /* synthesis preserve */
/* synthesis ALTERA_ATTRIBUTE = "-name SDC_STATEMENT \"set_false_path -from [get_keepers *ct_clock_cross*wrgray_r\[*\]]\" " */;

always @(posedge wrclk or posedge wrarst) begin
	if (wrarst) begin
		wrgray_r <= 0;
		wrgray_rr <= 0;
	end
	else begin
		wrgray_rr <= wrgray;		
		wrgray_r <= wrgray_rr;
	end
end

reg [SYNC_STAGES * (RAM_ADDR_WIDTH+1)-1:0] syn0 = 0 /* synthesis preserve */;
always @(posedge wrclk or posedge wrarst) begin
	if (wrarst) syn0 <= 0;
	else syn0 <= {syn0[(SYNC_STAGES-1) * (RAM_ADDR_WIDTH+1)-1:0], rdgray};		
end

reg [SYNC_STAGES * (RAM_ADDR_WIDTH+1)-1:0] syn1 = 0 /* synthesis preserve */;
always @(posedge rdclk or posedge rdarst) begin
	if (rdarst) syn1 <= 0;
	else syn1 <= {syn1[(SYNC_STAGES-1) * (RAM_ADDR_WIDTH+1)-1:0], wrgray_r};		
end

assign sync_rdptr = syn0[SYNC_STAGES * (RAM_ADDR_WIDTH+1)-1 :
						(SYNC_STAGES-1) * (RAM_ADDR_WIDTH+1)];
assign sync_wrptr = syn1[SYNC_STAGES * (RAM_ADDR_WIDTH+1)-1 :
						(SYNC_STAGES-1) * (RAM_ADDR_WIDTH+1)];


//////////////////////////////////////////
// storage array
//////////////////////////////////////////

wire we = wrreq & !wrfull;

(* ramstyle = "no_rw_check,MLAB" *)
reg [WIDTH-1:0] mem [0 : (1<<RAM_ADDR_WIDTH) - 1];

always @ (posedge wrclk) begin
	if (we) mem[wrbin[RAM_ADDR_WIDTH-1:0]] <= i_data;
end


assign o_data = mem[rdbin[RAM_ADDR_WIDTH-1:0]];

endmodule

