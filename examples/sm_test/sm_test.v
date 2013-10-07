module sm_test
(
    output xorro_out_valid,
    input xorro_out_ready,
    input clk,
    output [15:0] xorro_out_data,
    input reset
);
    wire the_dispatch_out_lp_id;
    wire the_dispatch_out_ready;
    wire split0_out1_valid;
    wire [3:0] xorro_in_conv_out_lp_id;
    wire [15:0] the_dispatch_out_conv_out_data;
    wire the_dispatch_out_valid;
    wire [15:0] the_dispatch_out_data;
    wire merge0_out_ready;
    wire xorro_in_conv_out_valid;
    wire xorro_in_conv_out_ready;
    wire the_dispatch_out_conv_out_ready;
    wire [15:0] xorro_in_conv_out_data;
    wire [1:0] the_dispatch_out_conv_out_flow_id;
    wire [1:0] merge0_out_flow_id;
    wire the_dispatch_out_conv_out_valid;
    wire [1:0] split0_out0_flow_id;
    wire split0_out0_valid;
    wire [15:0] the_inverter_out_data;
    wire split0_out0_ready;
    wire [15:0] split0_out0_data;
    wire [1:0] split0_out1_flow_id;
    wire split0_out1_ready;
    wire [15:0] split0_out1_data;
    wire merge0_out_eop;
    wire [15:0] the_reverser_out_data;
    wire merge0_out_valid;
    wire [15:0] merge0_out_data;
    wire the_reverser_out_valid;
    wire the_reverser_out_ready;
    wire the_inverter_out_valid;
    wire the_inverter_out_ready;
    
    dispatch #
    (
        .WIDTH(16)
    )
    the_dispatch
    (
        .clk(clk),
        .o_data(the_dispatch_out_data),
        .reset(reset),
        .i_ready(the_dispatch_out_ready),
        .o_lp(the_dispatch_out_lp_id),
        .o_valid(the_dispatch_out_valid)
    );
    inverter #
    (
        .WIDTH(16)
    )
    the_inverter
    (
        .clk(clk),
        .o_data(the_inverter_out_data),
        .reset(reset),
        .i_ready(the_inverter_out_ready),
        .i_data(split0_out0_data),
        .o_ready(split0_out0_ready),
        .i_valid(split0_out0_valid),
        .o_valid(the_inverter_out_valid)
    );
    xorer #
    (
        .WIDTH(16)
    )
    xorro
    (
        .clk(clk),
        .o_data(xorro_out_data),
        .reset(reset),
        .i_ready(xorro_out_ready),
        .i_lp(xorro_in_conv_out_lp_id),
        .i_valid(xorro_in_conv_out_valid),
        .i_data(xorro_in_conv_out_data),
        .o_ready(xorro_in_conv_out_ready),
        .o_valid(xorro_out_valid)
    );
    reverser #
    (
        .WIDTH(16)
    )
    the_reverser
    (
        .clk(clk),
        .o_data(the_reverser_out_data),
        .reset(reset),
        .i_ready(the_reverser_out_ready),
        .i_data(split0_out1_data),
        .o_ready(split0_out1_ready),
        .i_valid(split0_out1_valid),
        .o_valid(the_reverser_out_valid)
    );
    ct_split #
    (
        .NO(2),
        .FLOWS(7),
        .WO(18),
        .NF(2),
        .WF(2),
        .FLOW_LOC(0),
        .ENABLES(6)
    )
    split0
    (
        .clk(clk),
        .o_data({split0_out1_data,split0_out1_flow_id,split0_out0_data,split0_out0_flow_id}),
        .reset(reset),
        .i_ready({split0_out1_ready,split0_out0_ready}),
        .i_data({the_dispatch_out_conv_out_data,the_dispatch_out_conv_out_flow_id}),
        .i_valid(the_dispatch_out_conv_out_valid),
        .o_ready(the_dispatch_out_conv_out_ready),
        .o_valid({split0_out1_valid,split0_out0_valid})
    );
    ct_field_conv #
    (
        .WD(16),
        .IF(1),
        .WIF(1),
        .WOF(2),
        .N_ENTRIES(2),
        .OF(7)
    )
    the_dispatch_out_conv
    (
        .clk(clk),
        .i_field(the_dispatch_out_lp_id),
        .o_data(the_dispatch_out_conv_out_data),
        .reset(reset),
        .i_data(the_dispatch_out_data),
        .i_valid(the_dispatch_out_valid),
        .o_field(the_dispatch_out_conv_out_flow_id),
        .o_ready(the_dispatch_out_ready),
        .o_valid(the_dispatch_out_conv_out_valid),
        .i_ready(the_dispatch_out_conv_out_ready)
    );
    ct_field_conv #
    (
        .WD(16),
        .IF(2),
        .WIF(2),
        .WOF(4),
        .N_ENTRIES(2),
        .OF(105)
    )
    xorro_in_conv
    (
        .clk(clk),
        .i_field(merge0_out_flow_id),
        .o_data(xorro_in_conv_out_data),
        .reset(reset),
        .i_data(merge0_out_data),
        .i_valid(merge0_out_valid),
        .o_field(xorro_in_conv_out_lp_id),
        .o_ready(merge0_out_ready),
        .o_valid(xorro_in_conv_out_valid),
        .i_ready(xorro_in_conv_out_ready)
    );
    ct_merge #
    (
        .RADIX(2),
        .WIDTH(19),
        .EOP_LOC(0)
    )
    merge0
    (
        .clk(clk),
        .o_data({merge0_out_data,merge0_out_flow_id,merge0_out_eop}),
        .reset(reset),
        .i_ready(merge0_out_ready),
        .i_data({the_inverter_out_data,2'd2,1'd1,the_reverser_out_data,2'd0,1'd1}),
        .i_valid({the_inverter_out_valid,the_reverser_out_valid}),
        .o_ready({the_inverter_out_ready,the_reverser_out_ready}),
        .o_valid(merge0_out_valid)
    );
endmodule
