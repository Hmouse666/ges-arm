# Build bitstream and export an XSA from the reconstructed project.
set script_dir [file normalize [file dirname [info script]]]
set repo_root  [file normalize [file join $script_dir .. ..]]
set work_root  [file join $repo_root build vivado work]
set project_file [file join $work_root project zynq7020_gesture.xpr]
set xsa_file [file join $repo_root build hardware zynq7020_gesture.xsa]

if {![file exists $project_file]} {
    source [file join $script_dir create_project.tcl]
}

open_project $project_file
reset_run synth_1
launch_runs synth_1 -jobs 8
wait_on_run synth_1
if {[get_property STATUS [get_runs synth_1]] ne "synth_design Complete!"} {
    error "Synthesis failed: [get_property STATUS [get_runs synth_1]]"
}

launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_run impl_1
if {[get_property STATUS [get_runs impl_1]] ne "write_bitstream Complete!"} {
    error "Implementation failed: [get_property STATUS [get_runs impl_1]]"
}

open_run impl_1
report_timing_summary -file [file join $repo_root build reports timing_summary_new.rpt]
report_utilization -file [file join $repo_root build reports utilization_new.rpt]
report_drc -file [file join $repo_root build reports drc_new.rpt]

write_hw_platform -fixed -include_bit -force $xsa_file
puts "INFO: XSA exported to $xsa_file"

