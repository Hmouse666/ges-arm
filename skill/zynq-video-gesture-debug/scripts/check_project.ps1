param(
    [string]$Root
)

$ErrorActionPreference = "Stop"
$skillRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = [System.IO.Path]::GetFullPath((Join-Path $skillRoot "..\.."))
}
else {
    $Root = [System.IO.Path]::GetFullPath($Root)
}

if (-not (Test-Path -LiteralPath $Root)) {
    throw "Repository root does not exist: $Root"
}

$requiredPaths = @(
    "readme.md",
    "src/fpga/rtl/axi_vip_frame_difference.v",
    "src/fpga/rtl/rgb888_ycbcr444.v",
    "src/fpga/constraints/board.xdc",
    "src/fpga/bd/system.bd",
    "src/fpga/bd/recreate_system_bd.tcl",
    "src/ps/app/main.c",
    "src/ps/app/gesture_ui.c",
    "src/ps/app/lv_port_indev.c",
    "src/stm32/core/src/app.c",
    "src/stm32/tkkc_drivers/fpga_gesture.c",
    "build/vivado/create_project.tcl",
    "build/vitis/build_app.tcl",
    "build/reports/timing_summary.rpt",
    "build/reports/utilization.rpt"
)

$missing = @()
foreach ($relative in $requiredPaths) {
    $path = Join-Path $Root $relative
    if (-not (Test-Path -LiteralPath $path)) {
        $missing += $relative
    }
}

if ($missing.Count -gt 0) {
    $missing | ForEach-Object { Write-Output "MISSING: $_" }
    throw "Zynq project check failed: $($missing.Count) required path(s) missing."
}

$namePattern = "^[a-z0-9_.-]+$"
$badNames = @()

Get-ChildItem -LiteralPath $Root -Recurse -Directory -Force |
    Where-Object {
        $relative = $_.FullName.Substring($Root.Length).TrimStart("\", "/")
        $relative -notmatch "(^|[\\/])\.git([\\/]|$)"
    } |
    Where-Object { $_.Name -cnotmatch $namePattern } |
    ForEach-Object { $badNames += $_.FullName }

Get-ChildItem -LiteralPath $Root -Recurse -File -Force |
    Where-Object {
        $relative = $_.FullName.Substring($Root.Length).TrimStart("\", "/")
        $relative -notmatch "(^|[\\/])\.git([\\/]|$)"
    } |
    Where-Object { $_.Name -cnotmatch $namePattern } |
    ForEach-Object { $badNames += $_.FullName }

if ($badNames.Count -gt 0) {
    $badNames | Select-Object -First 50 | ForEach-Object { Write-Output "NONCOMPLIANT: $_" }
    throw "Zynq project check failed: $($badNames.Count) noncompliant path(s)."
}

$timingReport = Join-Path $Root "build/reports/timing_summary.rpt"
$timingText = Get-Content -LiteralPath $timingReport -Raw
if ($timingText -notmatch "All user specified timing constraints are met") {
    Write-Output "WARNING: saved timing report does not contain the expected success sentence."
}

Write-Output "PROJECT_CHECK_OK=True"
Write-Output "ROOT=$Root"
Write-Output "REQUIRED_PATHS=$($requiredPaths.Count)"
Write-Output "NAMING_OK=True"

