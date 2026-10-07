#define AppVersion "0.1.0"
[Setup]
AppId={{D6736F2C-56E0-4A8A-8C30-8872AA9A47E1}
AppName=NEXVARY Disk Care
AppVersion={#AppVersion}
AppPublisher=NEXVARY
DefaultDirName={autopf}\NEXVARY\Disk Care
DefaultGroupName=NEXVARY Disk Care
OutputDir=..\out
OutputBaseFilename=NEXVARY-DC-0.1.0-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
[Files]
Source: "..\out\portable\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\NEXVARY Disk Care"; Filename: "{app}\nexvary_dc.exe"
[Run]
Filename: "{app}\nexvary_dc.exe"; Description: "Open NEXVARY Disk Care"; Flags: nowait postinstall skipifsilent
