onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate -radix hexadecimal /tb/dut/reset
add wave -noupdate -radix hexadecimal /tb/dut/clk
add wave -noupdate -radix hexadecimal /tb/dut/D_out_inst_o_x
add wave -noupdate -radix hexadecimal -subitemconfig {{/tb/dut/D_out_inst_o_data[23]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[22]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[21]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[20]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[19]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[18]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[17]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[16]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[15]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[14]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[13]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[12]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[11]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[10]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[9]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[8]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[7]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[6]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[5]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[4]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[3]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[2]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[1]} {-radix hexadecimal} {/tb/dut/D_out_inst_o_data[0]} {-radix hexadecimal}} /tb/dut/D_out_inst_o_data
add wave -noupdate -radix hexadecimal /tb/dut/D_out_inst_o_valid
add wave -noupdate -radix hexadecimal /tb/dut/D_in_inst_o_ready
add wave -noupdate -radix hexadecimal /tb/dut/merge_o_valid
add wave -noupdate -radix hexadecimal /tb/dut/merge_o_ready
add wave -noupdate -radix hexadecimal -subitemconfig {{/tb/dut/merge_o_data[34]} {-radix hexadecimal} {/tb/dut/merge_o_data[33]} {-radix hexadecimal} {/tb/dut/merge_o_data[32]} {-radix hexadecimal} {/tb/dut/merge_o_data[31]} {-radix hexadecimal} {/tb/dut/merge_o_data[30]} {-radix hexadecimal} {/tb/dut/merge_o_data[29]} {-radix hexadecimal} {/tb/dut/merge_o_data[28]} {-radix hexadecimal} {/tb/dut/merge_o_data[27]} {-radix hexadecimal} {/tb/dut/merge_o_data[26]} {-radix hexadecimal} {/tb/dut/merge_o_data[25]} {-radix hexadecimal} {/tb/dut/merge_o_data[24]} {-radix hexadecimal} {/tb/dut/merge_o_data[23]} {-radix hexadecimal} {/tb/dut/merge_o_data[22]} {-radix hexadecimal} {/tb/dut/merge_o_data[21]} {-radix hexadecimal} {/tb/dut/merge_o_data[20]} {-radix hexadecimal} {/tb/dut/merge_o_data[19]} {-radix hexadecimal} {/tb/dut/merge_o_data[18]} {-radix hexadecimal} {/tb/dut/merge_o_data[17]} {-radix hexadecimal} {/tb/dut/merge_o_data[16]} {-radix hexadecimal} {/tb/dut/merge_o_data[15]} {-radix hexadecimal} {/tb/dut/merge_o_data[14]} {-radix hexadecimal} {/tb/dut/merge_o_data[13]} {-radix hexadecimal} {/tb/dut/merge_o_data[12]} {-radix hexadecimal} {/tb/dut/merge_o_data[11]} {-radix hexadecimal} {/tb/dut/merge_o_data[10]} {-radix hexadecimal} {/tb/dut/merge_o_data[9]} {-radix hexadecimal} {/tb/dut/merge_o_data[8]} {-radix hexadecimal} {/tb/dut/merge_o_data[7]} {-radix hexadecimal} {/tb/dut/merge_o_data[6]} {-radix hexadecimal} {/tb/dut/merge_o_data[5]} {-radix hexadecimal} {/tb/dut/merge_o_data[4]} {-radix hexadecimal} {/tb/dut/merge_o_data[3]} {-radix hexadecimal} {/tb/dut/merge_o_data[2]} {-radix hexadecimal} {/tb/dut/merge_o_data[1]} {-radix hexadecimal} {/tb/dut/merge_o_data[0]} {-radix hexadecimal}} /tb/dut/merge_o_data
add wave -noupdate -radix hexadecimal /tb/dut/merge_o_eop
add wave -noupdate -radix hexadecimal /tb/dut/split_o_data
add wave -noupdate -radix hexadecimal /tb/dut/split_o_valid
add wave -noupdate -radix hexadecimal /tb/dut/split_o_ready
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst_o_sop
add wave -noupdate -radix hexadecimal /tb/dut/B_in_inst_o_ready
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst_o_valid
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst_o_z
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst_o_eop
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst_o_y
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst_o_x
add wave -noupdate -radix hexadecimal /tb/dut/B_out_inst_o_valid
add wave -noupdate -radix hexadecimal /tb/dut/B_out_inst_o_data
add wave -noupdate -radix hexadecimal /tb/dut/C_in_inst_o_ready
add wave -noupdate -radix hexadecimal /tb/dut/A_out_inst_o_valid
add wave -noupdate -radix hexadecimal /tb/dut/A_in_inst_o_ready
add wave -noupdate -divider aout
add wave -noupdate -radix hexadecimal /tb/dut/A_out_inst/clk
add wave -noupdate -radix hexadecimal /tb/dut/A_out_inst/reset
add wave -noupdate -radix hexadecimal /tb/dut/A_out_inst/o_valid
add wave -noupdate -radix hexadecimal /tb/dut/A_out_inst/i_ready
add wave -noupdate -radix hexadecimal /tb/dut/A_out_inst/strobe
add wave -noupdate -divider ain
add wave -noupdate /tb/dut/A_in_inst/clk
add wave -noupdate /tb/dut/A_in_inst/reset
add wave -noupdate /tb/dut/A_in_inst/i_valid
add wave -noupdate /tb/dut/A_in_inst/o_ready
add wave -noupdate -divider bout
add wave -noupdate -radix hexadecimal /tb/dut/B_out_inst/clk
add wave -noupdate -radix hexadecimal /tb/dut/B_out_inst/reset
add wave -noupdate -radix hexadecimal /tb/dut/B_out_inst/o_valid
add wave -noupdate -radix hexadecimal /tb/dut/B_out_inst/i_ready
add wave -noupdate -radix hexadecimal /tb/dut/B_out_inst/o_data
add wave -noupdate -divider bin
add wave -noupdate -radix hexadecimal /tb/dut/B_in_inst/clk
add wave -noupdate -radix hexadecimal /tb/dut/B_in_inst/reset
add wave -noupdate -radix hexadecimal /tb/dut/B_in_inst/i_valid
add wave -noupdate -radix hexadecimal /tb/dut/B_in_inst/o_ready
add wave -noupdate -radix hexadecimal /tb/dut/B_in_inst/i_data
add wave -noupdate -divider cout
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst/clk
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst/reset
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst/o_valid
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst/i_ready
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst/o_x
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst/o_y
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst/o_z
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst/o_sop
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst/o_eop
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst/enable
add wave -noupdate -radix hexadecimal /tb/dut/C_out_inst/pkt_enable
add wave -noupdate -divider cin
add wave -noupdate -radix hexadecimal /tb/dut/C_in_inst/clk
add wave -noupdate -radix hexadecimal /tb/dut/C_in_inst/reset
add wave -noupdate -radix hexadecimal /tb/dut/C_in_inst/i_valid
add wave -noupdate -radix hexadecimal /tb/dut/C_in_inst/o_ready
add wave -noupdate -radix hexadecimal /tb/dut/C_in_inst/i_x
add wave -noupdate -radix hexadecimal /tb/dut/C_in_inst/i_y
add wave -noupdate -radix hexadecimal /tb/dut/C_in_inst/i_z
add wave -noupdate -radix hexadecimal /tb/dut/C_in_inst/i_sop
add wave -noupdate -radix hexadecimal /tb/dut/C_in_inst/i_eop
add wave -noupdate -divider dout
add wave -noupdate -radix hexadecimal /tb/dut/D_out_inst/clk
add wave -noupdate -radix hexadecimal /tb/dut/D_out_inst/reset
add wave -noupdate -radix hexadecimal /tb/dut/D_out_inst/o_valid
add wave -noupdate -radix hexadecimal /tb/dut/D_out_inst/i_ready
add wave -noupdate -radix hexadecimal /tb/dut/D_out_inst/o_data
add wave -noupdate -radix hexadecimal /tb/dut/D_out_inst/o_x
add wave -noupdate -divider din
add wave -noupdate -radix hexadecimal /tb/dut/D_in_inst/clk
add wave -noupdate -radix hexadecimal /tb/dut/D_in_inst/reset
add wave -noupdate -radix hexadecimal /tb/dut/D_in_inst/i_valid
add wave -noupdate -radix hexadecimal /tb/dut/D_in_inst/o_ready
add wave -noupdate -radix hexadecimal -subitemconfig {{/tb/dut/D_in_inst/i_data[23]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[22]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[21]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[20]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[19]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[18]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[17]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[16]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[15]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[14]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[13]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[12]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[11]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[10]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[9]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[8]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[7]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[6]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[5]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[4]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[3]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[2]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[1]} {-radix hexadecimal} {/tb/dut/D_in_inst/i_data[0]} {-radix hexadecimal}} /tb/dut/D_in_inst/i_data
add wave -noupdate -radix hexadecimal /tb/dut/D_in_inst/i_x
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 1} {17 ns} 0}
configure wave -namecolwidth 150
configure wave -valuecolwidth 100
configure wave -justifyvalue left
configure wave -signalnamewidth 1
configure wave -snapdistance 10
configure wave -datasetprefix 0
configure wave -rowmargin 4
configure wave -childrowmargin 2
configure wave -gridoffset 0
configure wave -gridperiod 1
configure wave -griddelta 40
configure wave -timeline 0
configure wave -timelineunits ns
update
WaveRestoreZoom {0 ns} {658 ns}
