source [file join .. make_model.tcl]

set param_col_names {NI WIDTH}
set config_col_names {BP EOP}
set datapoints {}

foreach WIDTH {0 1 2} {
	foreach NI {2 3 4} {
		foreach BP {0 1} {
			foreach EOP {0 1} {
				switch "$BP $EOP" {
					"1 1" { set config all }
					"0 1" { set config nobp }
					"1 0" { set config noeop }
					"0 0" { set config nobp_noeop }
				}
			
				lappend datapoints [dict create \
					params [dict create WIDTH $WIDTH NI $NI BP $BP EOP $EOP] \
					config $config]
			}
		}
	}
}

make_model genie_merge $param_col_names $config_col_names $datapoints
