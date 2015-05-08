module altera_std_synchronizer (
                                clk, 
                                reset_n, 
                                din, 
                                dout
                                );

    // GLOBAL PARAMETER DECLARATION
    parameter depth = 3; // This value must be >= 2 !
     
  
    // INPUT PORT DECLARATION 
    input   clk;
    input   reset_n;    
    input   din;

    // OUTPUT PORT DECLARATION 
    output  dout;

    // QuartusII synthesis directives:
    //     1. Preserve all registers ie. do not touch them.
    //     2. Do not merge other flip-flops with synchronizer flip-flops.
    // QuartusII TimeQuest directives:
    //     1. Identify all flip-flops in this module as members of the synchronizer 
    //        to enable automatic metastability MTBF analysis.
    //     2. Cut all timing paths terminating on data input pin of the first flop din_s1.

    (* altera_attribute = {"-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS; -name DONT_MERGE_REGISTER ON; -name PRESERVE_REGISTER ON; -name SDC_STATEMENT \"set_false_path -to [get_keepers {*altera_std_synchronizer:*|din_s1}]\" "} *) reg din_s1;

    (* altera_attribute = {"-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS; -name DONT_MERGE_REGISTER ON; -name PRESERVE_REGISTER ON"} *) reg [depth-2:0] dreg;    



    always @(posedge clk or negedge reset_n) begin
        if (reset_n == 0) 
            din_s1 <= 1'b0;
        else
            din_s1 <= din;
    end

    // the remaining synchronizer registers form a simple shift register
    // of length depth-1

    generate
        if (depth < 3) begin
            always @(posedge clk or negedge reset_n) begin
                if (reset_n == 0) 
                    dreg <= {depth-1{1'b0}};      
                else
                    dreg <= din_s1;
            end     
        end else begin
            always @(posedge clk or negedge reset_n) begin
                if (reset_n == 0) 
                    dreg <= {depth-1{1'b0}};
                else
                    dreg <= {dreg[depth-3:0], din_s1};
            end
        end
    endgenerate

    assign dout = dreg[depth-2];
   
endmodule  

module altera_dcfifo_synchronizer_bundle(
				     clk,
				     reset_n,
				     din,
				     dout
				     );
   parameter WIDTH = 1;
   parameter DEPTH = 3;   
   
   input clk;
   input reset_n;
   input [WIDTH-1:0] din;
   output [WIDTH-1:0] dout;
   
   genvar i;
   
   generate
      for (i=0; i<WIDTH; i=i+1)
	begin : sync
	   altera_std_synchronizer #(.depth(DEPTH))
                                   u (
				      .clk(clk), 
				      .reset_n(reset_n), 
				      .din(din[i]), 
				      .dout(dout[i])
				      );
	end
   endgenerate
   
endmodule 



module genie_clockx #
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

/* synthesis ALTERA_ATTRIBUTE = "-name SDC_STATEMENT \"set_false_path -from [get_registers {*|in_wr_ptr_gray[*]}] -to [get_registers {*|altera_dcfifo_synchronizer_bundle:write_crosser|altera_std_synchronizer:sync[*].u|din_s1}]\" " */

/* synthesis ALTERA_ATTRIBUTE = "-name SDC_STATEMENT \"set_false_path -from [get_registers {*|out_rd_ptr_gray[*]}] -to [get_registers {*|altera_dcfifo_synchronizer_bundle:read_crosser|altera_std_synchronizer:sync[*].u|din_s1}]\" " */

/* synthesis ALTERA_ATTRIBUTE = "-name SDC_STATEMENT \"set_false_path -from [get_registers -nowarn altera_avalon_dc_fifo:*|mem*] -to [get_registers -nowarn altera_avalon_dc_fifo:*|internal_out_payload*]\" " */

    // optimizations
    localparam LOOKAHEAD_POINTERS  = 0;
    localparam PIPELINE_POINTERS   = 1;

    localparam WR_SYNC_DEPTH = 2;
    localparam RD_SYNC_DEPTH = 2;
    
    localparam ADDR_WIDTH   = 4;
    localparam DEPTH        = 2 ** ADDR_WIDTH;

    // ---------------------------------------------------------------------
    // Memory Pointers
    // ---------------------------------------------------------------------
    (* ramstyle="no_rw_check,MLAB" *) reg [WIDTH - 1 : 0] mem [DEPTH - 1 : 0];
    
    wire [ADDR_WIDTH - 1 : 0] mem_wr_ptr;
    wire [ADDR_WIDTH - 1 : 0] mem_rd_ptr;

    reg [ADDR_WIDTH : 0] in_wr_ptr;
    reg [ADDR_WIDTH : 0] in_wr_ptr_lookahead;
    reg [ADDR_WIDTH : 0] out_rd_ptr;
    reg [ADDR_WIDTH : 0] out_rd_ptr_lookahead;
    
    // ---------------------------------------------------------------------
    // Internal Signals
    // ---------------------------------------------------------------------
    wire [ADDR_WIDTH : 0] next_out_wr_ptr;
    wire [ADDR_WIDTH : 0] next_in_wr_ptr;
    wire [ADDR_WIDTH : 0] next_out_rd_ptr;
    wire [ADDR_WIDTH : 0] next_in_rd_ptr;

    reg  [ADDR_WIDTH : 0] in_wr_ptr_gray     /*synthesis ALTERA_ATTRIBUTE = "SUPPRESS_DA_RULE_INTERNAL=D102" */;
    wire [ADDR_WIDTH : 0] out_wr_ptr_gray;    

    reg  [ADDR_WIDTH : 0] out_rd_ptr_gray    /*synthesis ALTERA_ATTRIBUTE = "SUPPRESS_DA_RULE_INTERNAL=D102" */;
    wire [ADDR_WIDTH : 0] in_rd_ptr_gray;

    reg  [ADDR_WIDTH : 0] out_wr_ptr_gray_reg;
    reg  [ADDR_WIDTH : 0] in_rd_ptr_gray_reg;

    reg full;
    reg empty;

    reg  [WIDTH - 1 : 0] internal_out_payload;

    wire internal_i_ready;
    wire internal_o_valid;   

    // ---------------------------------------------------------------------
    // Memory
    //
    // Infers a simple dual clock memory with unregistered outputs
    // ---------------------------------------------------------------------
    always @(posedge wrclk) begin
        if (i_valid && o_ready)
            mem[mem_wr_ptr] <= i_data;
    end

    always @(posedge rdclk) begin
        internal_out_payload <= mem[mem_rd_ptr];
    end

    assign mem_rd_ptr = next_out_rd_ptr;
    assign mem_wr_ptr = in_wr_ptr;


    // ---------------------------------------------------------------------
    // Pointer Management
    //
    // Increment our good old read and write pointers on their native
    // clock domains.
    // ---------------------------------------------------------------------
    always @(posedge wrclk or posedge arst) begin
        if (arst) begin
            in_wr_ptr           <= 0;
            in_wr_ptr_lookahead <= 1;
        end
        else begin
            in_wr_ptr           <= next_in_wr_ptr;
            in_wr_ptr_lookahead <= (i_valid && o_ready) ? in_wr_ptr_lookahead + 1'b1 : 
                in_wr_ptr_lookahead;
        end
    end

    always @(posedge rdclk or posedge arst) begin
        if (arst) begin
            out_rd_ptr           <= 0;
            out_rd_ptr_lookahead <= 1;
        end
        else begin
            out_rd_ptr           <= next_out_rd_ptr;
            out_rd_ptr_lookahead <= (internal_o_valid && internal_i_ready) ? out_rd_ptr_lookahead + 1'b1 :
                out_rd_ptr_lookahead;
        end
    end

    generate if (LOOKAHEAD_POINTERS) begin : lookahead_pointers

        assign next_in_wr_ptr = (o_ready && i_valid) ? in_wr_ptr_lookahead : in_wr_ptr;
        assign next_out_rd_ptr = (internal_i_ready && internal_o_valid) ? out_rd_ptr_lookahead : out_rd_ptr;

    end
    else begin : non_lookahead_pointers

        assign next_in_wr_ptr = (o_ready && i_valid) ? in_wr_ptr + 1'b1 : in_wr_ptr;
        assign next_out_rd_ptr = (internal_i_ready && internal_o_valid) ? out_rd_ptr + 1'b1 : out_rd_ptr;

    end
    endgenerate

    // ---------------------------------------------------------------------
    // Empty/Full Signal Generation
    //
    // We keep read and write pointers that are one bit wider than
    // required, and use that additional bit to figure out if we're
    // full or empty.
    // ---------------------------------------------------------------------
    always @(posedge rdclk or posedge arst) begin
        if(arst)
            empty <= 1;
        else
            empty <= (next_out_rd_ptr == next_out_wr_ptr);
    end

    always @(posedge wrclk or posedge arst) begin
        if (arst) begin
            full <= 0;
        end
        else begin
            full <= (next_in_rd_ptr[ADDR_WIDTH - 1 : 0] == next_in_wr_ptr[ADDR_WIDTH - 1 : 0]) && 
                (next_in_rd_ptr[ADDR_WIDTH] != next_in_wr_ptr[ADDR_WIDTH]);
        end
    end


    // ---------------------------------------------------------------------
    // Write Pointer Clock Crossing
    //
    // Clock crossing is done with gray encoding of the pointers. What? You
    // want to know more? We ensure a one bit change at sampling time,
    // and then metastable harden the sampled gray pointer.
    // ---------------------------------------------------------------------
    always @(posedge wrclk or posedge arst) begin
        if (arst)
            in_wr_ptr_gray <= 0;
        else
            in_wr_ptr_gray <= bin2gray(in_wr_ptr);
    end

    altera_dcfifo_synchronizer_bundle #(.WIDTH(ADDR_WIDTH+1), .DEPTH(WR_SYNC_DEPTH)) 
      write_crosser (
        .clk(rdclk),
        .reset_n(~arst),
        .din(in_wr_ptr_gray),
        .dout(out_wr_ptr_gray)
    );

    // ---------------------------------------------------------------------
    // Optionally pipeline the gray to binary conversion for the write pointer. 
    // Doing this will increase the latency of the FIFO, but increase fmax.
    // ---------------------------------------------------------------------
    generate if (PIPELINE_POINTERS) begin : wr_ptr_pipeline

        always @(posedge rdclk or posedge arst) begin
            if (arst)
                out_wr_ptr_gray_reg <= 0;
            else
                out_wr_ptr_gray_reg <= gray2bin(out_wr_ptr_gray);
        end

        assign next_out_wr_ptr = out_wr_ptr_gray_reg;

    end
    else begin : no_wr_ptr_pipeline

        assign next_out_wr_ptr = gray2bin(out_wr_ptr_gray);

    end
    endgenerate

    // ---------------------------------------------------------------------
    // Read Pointer Clock Crossing
    //
    // Go the other way, go the other way...
    // ---------------------------------------------------------------------
    always @(posedge rdclk or posedge arst) begin
        if (arst)
            out_rd_ptr_gray <= 0;
        else
            out_rd_ptr_gray <= bin2gray(out_rd_ptr);
    end

    altera_dcfifo_synchronizer_bundle #(.WIDTH(ADDR_WIDTH+1), .DEPTH(RD_SYNC_DEPTH)) 
      read_crosser (
        .clk(wrclk),
        .reset_n(~arst),
        .din(out_rd_ptr_gray),
        .dout(in_rd_ptr_gray)
    );

    // ---------------------------------------------------------------------
    // Optionally pipeline the gray to binary conversion of the read pointer. 
    // Doing this will increase the pessimism of the FIFO, but increase fmax.
    // ---------------------------------------------------------------------
    generate if (PIPELINE_POINTERS) begin : rd_ptr_pipeline

        always @(posedge wrclk or posedge arst) begin
            if (arst)
                in_rd_ptr_gray_reg <= 0;
            else
                in_rd_ptr_gray_reg <= gray2bin(in_rd_ptr_gray);
        end
        
        assign next_in_rd_ptr = in_rd_ptr_gray_reg;

    end
    else begin : no_rd_ptr_pipeline

        assign next_in_rd_ptr = gray2bin(in_rd_ptr_gray);

    end
    endgenerate

    // ---------------------------------------------------------------------
    // Avalon ST Signals
    // ---------------------------------------------------------------------
    assign internal_o_valid = !empty;
    assign o_ready = !full;

    // --------------------------------------------------
    // Output Pipeline Stage
    //
    // We do this on the single clock FIFO to keep fmax
    // up because the memory outputs are kind of slow.
    // Therefore, this stage is even more critical on a dual clock
    // FIFO, wouldn't you say? No one wants a slow dcfifo.
    // --------------------------------------------------
    assign internal_i_ready = i_ready || !o_valid;

    always @(posedge rdclk or posedge arst) begin
        if (arst) begin
            o_valid <= 0;
            o_data <= 0;
        end
        else begin
            if (internal_i_ready) begin
                o_valid <= internal_o_valid;
                o_data <= internal_out_payload;
            end
        end
    end

    // ---------------------------------------------------------------------
    // Gray Functions
    // 
    // These are real beasts when you look at them. But they'll be
    // tested thoroughly.
    // ---------------------------------------------------------------------
    function [ADDR_WIDTH : 0] bin2gray;
        input [ADDR_WIDTH : 0]  bin_val;
        integer i; 
                
        for (i = 0; i <= ADDR_WIDTH; i = i + 1)
        begin
            if (i == ADDR_WIDTH)
                bin2gray[i] = bin_val[i];
            else
                bin2gray[i] = bin_val[i+1] ^ bin_val[i];
        end
    endfunction

    function [ADDR_WIDTH : 0] gray2bin;
        input [ADDR_WIDTH : 0]  gray_val;
        integer i;
        integer j;
                
        for (i = 0; i <= ADDR_WIDTH; i = i + 1) begin
            
            gray2bin[i] = gray_val[i];

            for (j = ADDR_WIDTH; j > i; j = j - 1) begin
                gray2bin[i] = gray2bin[i] ^ gray_val[j];	
            end

        end
    endfunction

endmodule

