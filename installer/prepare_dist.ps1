param(
    [string]$Version = "1.0"
)
$ErrorActionPreference = 'Stop'

Write-Host "Building release and preparing dist for Ikona $Version"

meson setup build --buildtype=release
meson compile -C build

$dist = Join-Path -Path "$(Get-Location)" -ChildPath "dist\ikona-$Version"
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Path $dist | Out-Null

Copy-Item -Path "build\ikona.exe" -Destination $dist -Force
if (Test-Path "ikona_icon.ico") { Copy-Item -Path "ikona_icon.ico" -Destination $dist -Force }
if (Test-Path "style.css") { Copy-Item -Path "style.css" -Destination $dist -Force }
if (Test-Path "README.md") { Copy-Item -Path "README.md" -Destination $dist -Force }
if (Test-Path "icons") { Copy-Item -Path "icons" -Destination $dist -Recurse -Force }

Write-Host "Dist prepared at: $dist"

# Generate installer script (ikona.iss) so it always matches the dist contents and version
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$issPath = Join-Path $scriptDir "ikona.iss"

$setupIconRelative = if (Test-Path (Join-Path (Get-Location) 'ikona_icon.ico')) { "..\\ikona_icon.ico" } else { "" }
$infoAfterRelative = if (Test-Path (Join-Path (Get-Location) 'README.md')) { "..\\README.md" } else { "" }

$outputBase = "ikona-$Version-setup"

$iss = @"
[Setup]
AppName=Ikona
AppVersion=$Version
DefaultDirName={pf}\Ikona
DefaultGroupName=Ikona
OutputDir=..\dist
OutputBaseFilename=$outputBase
Compression=none
SolidCompression=no
UninstallDisplayIcon={app}\ikona.exe
ArchitecturesInstallIn64BitMode=x64
"@

if ($setupIconRelative -ne "") {
    $iss += "`nSetupIconFile=$setupIconRelative`n"
}
if ($infoAfterRelative -ne "") {
    $iss += "`nInfoAfterFile=$infoAfterRelative`n"
}

$iss += @"

[Files]
Source: "..\dist\ikona-$Version\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Icons]
Name: "{group}\Ikona"; Filename: "{app}\ikona.exe"; WorkingDir: "{app}"
Name: "{commondesktop}\Ikona"; Filename: "{app}\ikona.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ikona.exe"; Description: "Launch Ikona"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
"@

Write-Host "Writing installer script to: $issPath"
$iss | Out-File -FilePath $issPath -Encoding UTF8 -Force
