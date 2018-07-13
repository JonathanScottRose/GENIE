source [file join .. make_model.tcl]

set param_col_names {N NO_MULTICAST}
set config_col_names {BP}
set datapoints {}

foreach N {2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33} {
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
