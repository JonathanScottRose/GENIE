# ** Multicycles
#    -----------
set_multicycle_path -setup -end -from {*:pipe|*:top_read_data_mux|*} -to {*:*fpdiv|*} 4
set_multicycle_path -hold -end -from {*:pipe|*:top_read_data_mux|*} -to {*:*fpdiv|*} 4
set_multicycle_path -setup -end -from {*:*fpdiv|*} -to {*:*fpdiv|*} 4
set_multicycle_path -hold -end -from {*:*fpdiv|*} -to {*:*fpdiv|*} 4
set_multicycle_path -setup -end -from {*:*fpdiv|*} -to {*:*recip_mem|*} 4
set_multicycle_path -hold -end -from {*:*fpdiv|*} -to {*:*recip_mem|*} 4
#set_multicycle_path -setup -end -from {*:pipe|*j_37*} -to {*:pipe|*recip_mem|*} 4
#set_multicycle_path -hold -end -from {*:pipe|*j_37*} -to {*:pipe|*recip_mem|*} 4
# ---------------------------------------------

