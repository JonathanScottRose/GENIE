if {$argc < 1} {
    error "Must specify compilation path"
}

set path $1

do [file join $path msim_compile.tcl] $1

set files { cpu.v cpu_standalone.sv ct_clock_cross.sv ct_field_conv.sv ct_merge.sv \
    ct_mux.v ct_split.sv ct_pipe_stage.sv ct_merge_ex.sv }
    
foreach file $files {
    set fullpath [file join $path $file]
    vlog $fullpath
}

   