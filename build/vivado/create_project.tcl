# Recreate the Vivado project from source-controlled files.
set script_dir [file normalize [file dirname [info script]]]
set repo_root  [file normalize [file join $script_dir .. ..]]
set work_root  [file join $repo_root build vivado work]
set project_dir [file join $work_root project]

file mkdir $work_root
create_project zynq7020_gesture $project_dir -part xc7z020clg400-2 -force
set_property target_language Verilog [current_project]
set_property ip_repo_paths [list [file join $repo_root src fpga ip_repo]] [current_project]
update_ip_catalog -rebuild

set rtl_files [list \
    [file join $repo_root src fpga rtl axi_vip_frame_difference.v] \
    [file join $repo_root src fpga rtl rgb888_ycbcr444.v] \
    [file join $repo_root src fpga rtl key.v] \
    [file join $repo_root src fpga rtl line_buffer.v] \
]
add_files -norecurse -fileset sources_1 $rtl_files

set constraint_files [list \
    [file join $repo_root src fpga constraints board.xdc] \
    [file join $repo_root src fpga constraints debug_hub_clk.xdc] \
]
add_files -norecurse -fileset constrs_1 $constraint_files
set_property target_constrs_file [lindex $constraint_files 0] [current_fileset -constrset]

update_compile_order -fileset sources_1
source [file join $repo_root src fpga bd recreate_system_bd.tcl]

set bd_file [get_files system.bd]
if {$bd_file eq ""} {
    error "Block Design system.bd was not created."
}
make_wrapper -files $bd_file -top
set wrapper_file [file join $project_dir zynq7020_gesture.gen sources_1 bd system hdl system_wrapper.v]
if {![file exists $wrapper_file]} {
    error "Generated wrapper not found: $wrapper_file"
}
add_files -norecurse -fileset sources_1 $wrapper_file
set_property top system_wrapper [current_fileset]
update_compile_order -fileset sources_1
puts "INFO: Vivado project recreated at $project_dir"

