;
; The Windows installer: the VST3 and the standalone in one Setup.exe.
;
;     .\build.ps1 -Release         builds the plugin, then this, into dist\
;
; build.ps1 passes the version read from CMakeLists.txt, so it is written down once. Compiled
; by hand without it, the installer says 0.0.0 rather than a version it was not built from.
;
; Setup asks whether to install for everyone or just this user. Everyone (the default) puts
; the VST3 in C:\Program Files\Common Files\VST3, which every DAW scans; just me needs no
; administrator rights and uses %LOCALAPPDATA%\Programs\Common\VST3, the folder build.ps1
; installs to, which some hosts need adding to their plugin paths.
;
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define Artefacts "..\build\Mz303_artefacts\Release"

[Setup]
; The installer's identity, as the plugin codes are the plugin's: never change it, or a new
; version installs beside the old one rather than upgrading it.
AppId={{FBA46E18-0804-4905-A2FB-E86810795D1B}
AppName=Mz303
AppVersion={#AppVersion}
AppVerName=Mz303 {#AppVersion}
AppPublisher=simozzer
AppPublisherURL=https://github.com/simozzer/Mz303
AppSupportURL=https://github.com/simozzer/Mz303/issues
AppUpdatesURL=https://github.com/simozzer/Mz303/releases
AppCopyright=Created by Simon Moscrop
VersionInfoVersion={#AppVersion}

DefaultDirName={autopf}\Mz303
DefaultGroupName=Mz303
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE

PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0

OutputDir=..\dist
OutputBaseFilename=Mz303-{#AppVersion}-win64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\Mz303.exe
UninstallDisplayName=Mz303 {#AppVersion}

; A DAW with the plugin loaded holds Mz303.vst3 open; let Setup offer to close it.
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "VST3 plugin and standalone app"
Name: "vst3"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full vst3 custom
Name: "standalone"; Description: "Standalone app (plays without a DAW)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: standalone; Flags: unchecked

[Files]
; {autocf} is Common Files for everyone and %LOCALAPPDATA%\Programs\Common for just me.
Source: "{#Artefacts}\VST3\Mz303.vst3\*"; DestDir: "{autocf}\VST3\Mz303.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\Mz303.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Mz303"; Filename: "{app}\Mz303.exe"; Components: standalone
Name: "{autodesktop}\Mz303"; Filename: "{app}\Mz303.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Mz303.exe"; Description: "{cm:LaunchProgram,Mz303}"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked
