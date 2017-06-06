package require ::quartus::flow
package require ::quartus::report
package require ::quartus::misc

if { ![info exists env(GENIE_ROOTDIR)] } {
	error "Environment GENIE_ROOTDIR not set"
}
set GENIE_ROOTDIR $env(GENIE_ROOTDIR)

set RES_TYPES { \
    { "ALM" {-resource {ALMs needed*}} } \
    { "MemALM" {-resource "*ALM**memory*"} } \
    { "CombALUT" {-alut} } \
    { "Reg" {-reg} } \
}

proc write_col_info {fp param_col_names config_col_names} {
	set all_col_names [concat $param_col_names $config_col_names]
	puts $fp "COLS [llength $all_col_names]"
	foreach col $all_col_names {
		puts -nonewline $fp "$col "
	}
	puts $fp ""
}

proc write_row_params {fp dp_params param_col_names config_col_names} {
	set all_col_names [concat $param_col_names $config_col_names]
	
	foreach parm_name $all_col_names {
		set value [dict get $dp_params $parm_name]
		puts -nonewline $fp "$value "
	}
	puts $fp ""
}

proc do_datapoint {fp dp param_col_names config_col_names} {
	global GENIE_ROOTDIR
	
	set dp_params [dict get $dp params]
	set dp_config [dict get $dp config]
	
	write_row_params $fp $dp_params $param_col_names $config_col_names
	
	# Create the project
	project_new -overwrite harness
	project_open harness

	# Device settings
	source [file join .. device.tcl]

	# Define source files
	set_global_assignment -name TOP_LEVEL_ENTITY harness
	set_global_assignment -name SYSTEMVERILOG_FILE [file join src ${dp_config}.sv]
	set_global_assignment -name SDC_FILE [file join src harness.sdc]
	set_global_assignment -name SEARCH_PATH [file join $GENIE_ROOTDIR data]

	# Parameterize
	foreach parm_name $param_col_names {
		set parm_val [dict get $dp_params $parm_name]
		set_parameter -name $parm_name $parm_val
	}

	# Compile settings
	set_global_assignment -name PROJECT_OUTPUT_DIRECTORY out
	set_global_assignment -name PLACEMENT_EFFORT_MULTIPLIER 4

	# Make IOs be virtual pins, and silence message spam
	set_global_assignment -name MESSAGE_DISABLE 15720
	set_instance_assignment -name VIRTUAL_PIN ON -to *

	# Map and fit
	execute_module -tool syn
	execute_module -tool fit
		
	# Extract area
	load_report
	
	global RES_TYPES
	foreach res_entry $RES_TYPES {
		set name [lindex $res_entry 0]
		set arg [lindex $res_entry 1]
	
		set result [eval get_fitter_resource_usage $arg]
		set used [lindex $result 0]
		
		puts -nonewline $fp "$name $used "
	}
	puts $fp ""
	
	# Extract timing
	set timing_script [file join .. timing.tcl]
	set config_file [file join src ${dp_config}.cfg]
	set out_file timing.txt
	exec -ignorestderr quartus_sta -t $timing_script -config_file $config_file -out_file $out_file
	
	set timing_fp [open $out_file r]
	set timing_data [read $timing_fp]
	puts -nonewline $fp $timing_data
	close $timing_fp
	
	project_close
}

proc make_model {model_name param_col_names config_col_names datapoints} {
	set fp [open ${model_name}.txt w]
	
	write_col_info $fp $param_col_names $config_col_names
	
	puts $fp "ROWS [llength $datapoints]"
	
	foreach dp $datapoints {
		do_datapoint $fp $dp $param_col_names $config_col_names
		flush $fp
	}
	
	close $fp
}
