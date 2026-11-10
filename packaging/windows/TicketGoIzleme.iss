; TicketGo Izleme — Windows x64 per-user installer (no admin required).
; Secrets and API keys are never embedded.

#define MyAppName "TicketGo Izleme ve Takip Sistemi"
#define MyAppVersion "0.1.3"
#define MyAppPublisher "TicketGoTeknoloji"
#define MyAppExeName "TicketGoIzleme.exe"
#define MyAppId "tr.ticketgoteknoloji.izleme"

#ifndef BuildDir
  #define BuildDir "..\..\build-windows\dist"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\build-windows"
#endif

[Setup]
AppId={{A7C2E8F1-4B19-4D6A-9E21-7B3A91C0D2E4}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\TicketGoTeknoloji\TicketGo Izleme
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=TicketGoIzleme-Windows-x64-Setup
SetupIconFile=..\..\resources\AppIcon.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes
RestartApplications=no
; User data under %AppData%\TicketGoTeknoloji\SistemIzleme is never deleted.

[Languages]
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Masaüstü kısayolu oluştur"; GroupDescription: "Ek simgeler:"; Flags: unchecked

[Files]
Source: "{#BuildDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Kaldır {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{#MyAppName} uygulamasını aç"; Flags: nowait postinstall skipifsilent

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
