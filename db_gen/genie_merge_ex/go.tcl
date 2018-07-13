source [file join .. make_model.tcl]

set param_col_names {NI WIDTH}
set config_col_names {}
set datapoints {}

foreach WIDTH {0 1 2} {
	foreach NI {2 3 4} {
		lappend datapoints [dict create \
			params [dict create WIDTH $WIDTH NI $NI] \
			config all]
	}
}

make_model genie_merge_ex $param_col_names $config_col_names $datapoints
