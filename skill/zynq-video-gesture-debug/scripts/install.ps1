param(
    [string]$CodexHome,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$skillRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$sourceEntry = Join-Path $skillRoot "skill.md"

if (-not (Test-Path -LiteralPath $sourceEntry)) {
    throw "Project skill entry is missing: $sourceEntry"
}

if ([string]::IsNullOrWhiteSpace($CodexHome)) {
    if (-not [string]::IsNullOrWhiteSpace($env:CODEX_HOME)) {
        $CodexHome = $env:CODEX_HOME
    }
    else {
        $CodexHome = Join-Path $HOME ".codex"
    }
}

$skillsRoot = Join-Path $CodexHome "skills"
$destination = Join-Path $skillsRoot "zynq-video-gesture-debug"

if (Test-Path -LiteralPath $destination) {
    if (-not $Force) {
        throw "Destination exists: $destination. Re-run with -Force to replace it."
    }
    Remove-Item -LiteralPath $destination -Recurse -Force
}

New-Item -ItemType Directory -Path $destination -Force | Out-Null
Get-ChildItem -LiteralPath $skillRoot -Force |
    ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force
    }

$installedEntry = Join-Path $destination "skill.md"
$standardEntry = Join-Path $destination "SKILL.md"
Rename-Item -LiteralPath $installedEntry -NewName "SKILL.md"

if (-not (Test-Path -LiteralPath $standardEntry)) {
    throw "Installed skill entry was not created: $standardEntry"
}

Write-Output "SKILL_INSTALLED=True"
Write-Output "DESTINATION=$destination"
Write-Output "ENTRY=$standardEntry"

