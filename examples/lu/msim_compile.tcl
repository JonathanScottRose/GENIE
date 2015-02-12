if {$argc < 1} {
    error "Must specify compilation path"
}

set path $1
set files { lu_new_pkg.sv cpu_cache.sv cpu_cache_to_net.sv cpu_main_ctrl.sv \
    cpu_net_to_cache.sv cpu_pipe_ctrl.sv cpu_pipeline.sv cpu_ram_addr_swiz.sv \
    cpu_ram_we_swiz.sv divsp.v multsplat11.v pipe_delay.v \
    pipe_interlock.sv subsplat11.v }
    
foreach file $files {
    set fullpath [file join $path $file]
    vlog $fullpath
}

    