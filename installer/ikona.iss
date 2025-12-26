[Setup]
AppName=Ikona
AppVersion=1.3
DefaultDirName={pf}\Ikona
DefaultGroupName=Ikona
OutputDir=..\dist
OutputBaseFilename=ikona-1.2-setup
Compression=none
SolidCompression=no
UninstallDisplayIcon={app}\ikona.exe
ArchitecturesInstallIn64BitMode=x64
SetupIconFile=..\\ikona_icon.ico

InfoAfterFile=..\\README.md

[Files]
Source: "..\dist\ikona-1.2\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Icons]
Name: "{group}\Ikona"; Filename: "{app}\ikona.exe"; WorkingDir: "{app}"
Name: "{commondesktop}\Ikona"; Filename: "{app}\ikona.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ikona.exe"; Description: "Launch Ikona"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
