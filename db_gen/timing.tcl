set PROJ harness

package require cmdline
global quartus

# Get cmdline options
set q_args $quartus(args)

set args [cmdline::getKnownOptions q_args { \
    {config_file.arg "" "Configuration file"} \
	{out_file.arg "" "Output file"} \
}]
dict for {k v} $args { set $k $v }

foreach arg {config_file out_file} {
	if { ![info exists $arg] } {
		post_message -type error "Need $arg"
		qexit -error
	}
}

# Open config file
if [catch {set fh_config [open $config_file r]} errmsg] {
	post_message -type error $errmsg
	qexit -error
}

# Process config file and parse tnode specs
set nodespecs {}
while 1 {
	set line [gets $fh_config]
	if [eof $fh_config] break
	lappend nodespecs $line
}

# Close config file
close $fh_config

# Open project
project_open $PROJ 
create_timing_netlist
read_sdc
update_timing_netlist

# Open output file
set fp [open $out_file w]

puts $fp "NODES [llength $nodespecs]"

foreach spec $nodespecs {
	set in_name [lindex $spec 0]
	set in_str [lindex $spec 1]
	set out_name [lindex $spec 2]
	set out_str [lindex $spec 3]
	
	# change npaths to 0 to get highest number of logic levels
	# leave npaths at 1 to get # of logic levels for longest (ns) path
	set coll [get_path -from $in_str -to $out_str -npaths 0]
	set coll_size [get_collection_size $coll]
	
	if { $coll_size == 0 } {
		# default to 0 if no paths found
		set result 0
	} else {
		# find path with most logic levels
		set result 0
		foreach_in_collection path $coll {
			set lev [get_path_info -num_logic_levels $path]
			if {$lev > $result} {
				set result $lev
			}
		}
	}
	
	puts $fp "$in_name $out_name $result"
}

# Close output file
close $fp

# Close project
project_close