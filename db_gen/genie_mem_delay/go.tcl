source [file join .. make_model.tcl]

set param_col_names {WIDTH CYCLES}
set config_col_names {BP}
set datapoints {}

foreach WIDTH {0 1 2 4 8 16 21 22} {
	foreach CYCLES {2 4 8 16 32 33 34} {
		foreach BP {0 1} {
			if {$BP == 0 } {
				set config nobp
			} else {
				set config all
			}
			lappend datapoints [dict create \
				params [dict create WIDTH $WIDTH CYCLES $CYCLES BP $BP] \
				config $config]
		}
	}
}

make_model genie_mem_delay $param_col_names $config_col_names $datapoints
