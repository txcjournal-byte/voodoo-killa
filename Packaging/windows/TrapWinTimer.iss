; TrapWin Timer - Windows installer (Inno Setup 6)
; Build: iscc /DAppVersion=0.9.0 /DSourceDir=<folder with "TrapWin Timer.vst3" and "TrapWin Timer.exe"> Packaging\windows\TrapWinTimer.iss

#ifndef AppVersion
  #define AppVersion "0.9.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\installers"
#endif

[Setup]
AppId={{6E0B7C1A-3F55-4C8E-9D3B-7A1D2B9F4E21}
AppName=TrapWin Timer
AppVersion={#AppVersion}
AppVerName=TrapWin Timer {#AppVersion}
AppPublisher=TrapWin
AppPublisherURL=https://trapwin.com
AppSupportURL=https://trapwin.com
DefaultDirName={autopf}\TrapWin\TrapWin Timer
DefaultGroupName=TrapWin
DisableProgramGroupPage=yes
LicenseFile=..\EULA.txt
OutputDir={#OutputDir}
OutputBaseFilename=TrapWin-Timer-{#AppVersion}-Windows-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName=TrapWin Timer
MinVersion=10.0

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (FL Studio, Ableton Live, ...)"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone application"; Types: full

[Files]
Source: "{#SourceDir}\TrapWin Timer.vst3\*"; DestDir: "{commoncf64}\VST3\TrapWin Timer.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\TrapWin Timer.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\README-USER.txt"; DestDir: "{app}"; DestName: "README.txt"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\TrapWin Timer"; Filename: "{app}\TrapWin Timer.exe"; Components: standalone
Name: "{group}\Uninstall TrapWin Timer"; Filename: "{uninstallexe}"

[InstallDelete]
; remove an older copy of the bundle so no stale files stay inside it
Type: filesandordirs; Name: "{commoncf64}\VST3\TrapWin Timer.vst3"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TrapWin Timer.vst3"
