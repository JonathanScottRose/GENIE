source [file join .. make_model.tcl]

set param_col_names {N NO_MULTICAST}
set config_col_names {BP}
set datapoints {}

foreach N {2 4 8 12 16 24 32} {
	foreach NO_MULTICAST {0 1} {
		foreach BP {0 1} {
			if {$BP == 0} {
				set config nobp
			} else {
				set config all
			}
		
			lappend datapoints [dict create \
				params [dict create N $N NO_MULTICAST $NO_MULTICAST BP $BP] \
				config $config]
		}
	}
}

make_model genie_split $param_col_names $config_col_names $datapoints
