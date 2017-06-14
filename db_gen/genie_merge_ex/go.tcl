source [file join .. make_model.tcl]

set param_col_names {NI WIDTH}
set config_col_names {}
set datapoints {}

foreach WIDTH {0 1 2} {
	foreach NI {2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31} {
		lappend datapoints [dict create \
			params [dict create WIDTH $WIDTH NI $NI] \
			config all]
	}
}

make_model genie_merge_ex $param_col_names $config_col_names $datapoints
