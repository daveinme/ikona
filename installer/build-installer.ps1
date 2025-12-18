param(
    [string]$ISCCPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    [string]$Version = "1.0"
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path $ISCCPath)) {
    Write-Host "ISCC not found at $ISCCPath. Install Inno Setup 6 and retry, or pass the correct path as -ISCCPath." -ForegroundColor Yellow
    exit 1
}

Write-Host "Preparing dist..."
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
& "$scriptDir\prepare_dist.ps1" -Version $Version

$issPath = Join-Path $scriptDir "ikona.iss"
if (-not (Test-Path $issPath)) {
    Write-Error "Cannot find installer script: $issPath"
    exit 1
}

Write-Host "Compiling installer with ISCC..."
& "$ISCCPath" "$issPath"

Write-Host "Installer build finished."
