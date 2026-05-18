; EmberInterDebugTool 尘智串口调试工具 - Inno Setup 安装脚本
; 用法: ISCC emberInter.iss (在 deploy 目录下执行)

#define MyAppName "EmberInterDebugTool"
#define MyAppCnName "尘智串口调试工具"
#define MyAppVersion "1.2.0"
#define MyAppPublisher "EmberInter"
#define MyAppExeName "serial-monitor.exe"

[Setup]
AppId={{B8F7A3D2-9C4E-4A1B-8F6D-3E2C5A7B9D1F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commonpf32}\EmberInterDebugTool
DefaultGroupName=EmberInterDebugTool
AllowNoIcons=yes
OutputDir=..\dist
OutputBaseFilename=emberInter-Setup-{#MyAppVersion}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
DisableProgramGroupPage=yes
SetupIconFile=emberInter\icons\app.ico
UninstallDisplayIcon={app}\icons\app.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式:"; Flags: checkedonce

[Files]
Source: "emberInter\serial-monitor.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "emberInter\serial-monitor-cli.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "emberInter\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "emberInter\platforms\*.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "emberInter\imageformats\*.dll"; DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "emberInter\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion
Source: "emberInter\icons\*"; DestDir: "{app}\icons"; Flags: ignoreversion
Source: "emberInter\*.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "emberInter\skill\*"; DestDir: "{app}\skill"; Flags: ignoreversion

[Icons]
Name: "{group}\尘智串口调试工具 (GUI)"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\icons\app.ico"
Name: "{group}\尘智串口调试工具 (CLI)"; Filename: "{app}\serial-monitor-cli.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\尘智串口调试工具"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon; IconFilename: "{app}\icons\app.ico"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动 尘智串口调试工具"; Flags: nowait postinstall skipifsilent shellexec