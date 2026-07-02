; Inno Setup 6 script for 直拼 ZhiPin.
; Build after CMake: iscc installer.iss
; Pass /DAppVersion=x.y.z to override the version.

#ifndef AppVersion
#define AppVersion "0.1.3"
#endif

[Setup]
AppId={{B5D7E1F3-4A2C-4C6E-9B80-2D3E4F5A6B7C}
AppName=直拼 ZhiPin
AppVersion={#AppVersion}
AppPublisher=ZhiPin Project
DefaultDirName={autopf}\ZhiPin
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir=dist
OutputBaseFilename=ZhiPin-Setup-{#AppVersion}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=直拼 ZhiPin
CloseApplications=no

[Files]
Source: "build\Release\ZhiPin.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace uninsrestartdelete
Source: "..\data\dict.tsv"; DestDir: "{app}"; Flags: ignoreversion
Source: "INSTALL-NOTE.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Run]
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\ZhiPin.dll"""; StatusMsg: "正在注册输入法 Registering input method..."; Flags: runhidden

[UninstallRun]
; Unregister only. The user dictionary in %APPDATA%\ZhiPin is intentionally
; left untouched so learned phrases survive reinstalls.
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\ZhiPin.dll"""; RunOnceId: "UnregZhiPin"; Flags: runhidden
