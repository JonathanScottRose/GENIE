source [file join .. make_model.tcl]

set param_col_names {WIDTH}
set config_col_names {BP}
set datapoints {}

foreach WIDTH {0 1 2 7 8} {
	foreach BP {0 1} {
		if {$BP == 0 } {
			set config nobp
		} else {
			set config all
		}
		lappend datapoints [dict create \
			params [dict create WIDTH $WIDTH BP $BP] \
			config $config]
	}
}

make_model genie_pipe_stage $param_col_names $config_col_names $datapoints
