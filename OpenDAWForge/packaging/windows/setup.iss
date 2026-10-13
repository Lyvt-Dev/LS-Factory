#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef PayloadPath
  #error PayloadPath is required
#endif
#ifndef OutputPath
  #error OutputPath is required
#endif
[Setup]
AppId={{6E8926C5-4899-43B7-BC60-6B93DE4FA9DC}
AppName=LS Factory
AppVersion={#AppVersion}
AppPublisher=LS Factory
DefaultDirName={localappdata}\Programs\LSFactory
DefaultGroupName=LS Factory
PrivilegesRequired=lowest
OutputDir={#OutputPath}
OutputBaseFilename=LSFactory-{#AppVersion}-windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\LS Factory.exe
LicenseFile={#PayloadPath}\LICENSE
CloseApplications=yes
[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
[Tasks]
Name: "desktopicon"; Description: "Desktop-Verknuepfung erstellen"; Flags: unchecked
Name: "fileassociation"; Description: ".odaw-Projekte mit LS Factory oeffnen"; Flags: unchecked
[Files]
Source: "{#PayloadPath}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\LS Factory"; Filename: "{app}\LS Factory.exe"
Name: "{group}\LS Factory entfernen"; Filename: "{uninstallexe}"
Name: "{autodesktop}\LS Factory"; Filename: "{app}\LS Factory.exe"; Tasks: desktopicon
[Registry]
Root: HKCU; Subkey: "Software\Classes\.odaw\OpenWithProgids"; ValueType: string; ValueName: "LSFactory.Project"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\LSFactory.Project"; ValueType: string; ValueData: "LS Factory Project"; Flags: uninsdeletekey; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\LSFactory.Project\DefaultIcon"; ValueType: string; ValueData: "{app}\LS Factory.exe,0"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\LSFactory.Project\shell\open\command"; ValueType: string; ValueData: """{app}\LS Factory.exe"" ""%1"""; Tasks: fileassociation
[Run]
Filename: "{app}\LS Factory.exe"; Description: "LS Factory starten"; Flags: nowait postinstall skipifsilent

Filename: "{app}\LS Factory.exe"; Flags: nowait; Check: IsFactoryUpdate

[Code]
function OpenProcess(DesiredAccess: LongWord; InheritHandle: Boolean; ProcessId: LongWord): LongWord;
  external 'OpenProcess@kernel32.dll stdcall';
function WaitForSingleObject(Handle: LongWord; Milliseconds: LongWord): LongWord;
  external 'WaitForSingleObject@kernel32.dll stdcall';
function CloseHandle(Handle: LongWord): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';
function IsFactoryUpdate: Boolean;
begin
  Result := ExpandConstant('{param:LSFACTORYUPDATE|0}') = '1';
end;
function InitializeSetup: Boolean;
var
  Pid, Handle: LongWord;
begin
  Result := True;
  Pid := StrToIntDef(ExpandConstant('{param:WAITPID|0}'), 0);
  if Pid = 0 then Exit;
  Handle := OpenProcess($00100000, False, Pid);
  if Handle = 0 then Exit;
  Result := WaitForSingleObject(Handle, 60000) = 0;
  CloseHandle(Handle);
  if not Result then MsgBox('LS Factory konnte nicht geschlossen werden. Bitte das Update erneut starten.', mbError, MB_OK);
end;
