#define AppVersion "0.8.0"
[Setup]
AppId={{D6736F2C-56E0-4A8A-8C30-8872AA9A47E1}
AppName=NEXVARY Disk Care
AppVersion={#AppVersion}
AppPublisher=NEXVARY
AppPublisherURL=https://nexvary.com
AppSupportURL=https://nexvary.com
DefaultDirName={autopf}\NEXVARY\Disk Care
DefaultGroupName=NEXVARY Disk Care
OutputDir=..\out
OutputBaseFilename=NEXVARY-DC-0.8.0-Setup
SetupIconFile=..\assets\nexvary-dc.ico
UninstallDisplayIcon={app}\nexvary_dc.exe
WizardImageFile=..\assets\setup-banner.bmp
WizardSmallImageFile=..\assets\setup-small.bmp
WizardStyle=modern
WizardSizePercent=110
DisableProgramGroupPage=yes
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
VersionInfoDescription=NEXVARY Disk Care Installer
CloseApplications=yes
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"; InfoBeforeFile: "features-en.txt"
Name: "arabic"; MessagesFile: "Arabic.isl"; InfoBeforeFile: "features-ar.txt"
[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
[Files]
Source: "..\out\portable\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\NEXVARY Disk Care"; Filename: "{app}\nexvary_dc.exe"
Name: "{autodesktop}\NEXVARY Disk Care"; Filename: "{app}\nexvary_dc.exe"; Tasks: desktopicon
[Run]
Filename: "{app}\nexvary_dc.exe"; Description: "{cm:LaunchProgram,NEXVARY Disk Care}"; Flags: nowait postinstall skipifsilent
