# Recreate the Vitis platform and application from the exported XSA.
set script_dir [file normalize [file dirname [info script]]]
set repo_root  [file normalize [file join $script_dir .. ..]]
set workspace  [file join $repo_root build vitis workspace]
set xsa_file   [file join $repo_root build hardware system_wrapper.xsa]

if {![file exists $xsa_file]} {
    error "Missing XSA: $xsa_file"
}

file mkdir $workspace
setws $workspace

platform create -name zynq7020_platform \
    -hw $xsa_file \
    -proc ps7_cortexa9_0 \
    -os standalone
platform generate

app create -name gesture_app \
    -platform zynq7020_platform \
    -domain standalone_domain \
    -template {Empty Application}

importsources -name gesture_app \
    -path [file join $repo_root src ps app] \
    -soft-link

app build -name gesture_app
puts "INFO: Vitis workspace created at $workspace"

