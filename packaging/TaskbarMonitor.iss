#ifndef BinaryDir
  #error Define BinaryDir as the absolute path to the installed-mode Release executable directory
#endif
#define Bin BinaryDir + "\TaskbarMonitor.exe"
#define Version GetStringFileInfo(Bin, "ProductVersion")
#define Publisher GetStringFileInfo(Bin, "CompanyName")
#define Name "Taskbar Monitor"

[Setup]
AppId=TaskbarMonitor
AppName={#Name}
AppVersion={#Version}
AppPublisher={#Publisher}
VersionInfoVersion={#Version}.0
VersionInfoProductName={#Name}
VersionInfoCompany={#Publisher}
VersionInfoDescription=Taskbar Monitor Setup
DefaultDirName={autopf}\{#Name}
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
WizardStyle=modern
SetupIconFile=TaskbarMonitor.ico
UninstallDisplayIcon={app}\TaskbarMonitor.exe
OutputDir=..\release
OutputBaseFilename=TaskbarMonitor-Setup-x64
Compression=lzma2
SolidCompression=yes
AppMutex=Local\TaskbarMonitor.Foundation
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked
Name: startup; Description: "Start Taskbar Monitor when I sign in"; Flags: unchecked; Check: FreshInstall

[Files]
Source: "{#Bin}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\third_party\nlohmann\LICENSE.MIT"; DestDir: "{app}"; DestName: "nlohmann-JSON-LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\Taskbar Monitor"; Filename: "{app}\TaskbarMonitor.exe"
Name: "{commondesktop}\Taskbar Monitor"; Filename: "{app}\TaskbarMonitor.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\TaskbarMonitor.exe"; Parameters: "--register-startup"; Flags: runasoriginaluser waituntilterminated; Tasks: startup
Filename: "{app}\TaskbarMonitor.exe"; Description: "Launch Taskbar Monitor"; Flags: postinstall nowait skipifsilent runasoriginaluser

[Code]
const
  RunKey = 'Software\Microsoft\Windows\CurrentVersion\Run';
var
  FirstInstall: Boolean;

function InitializeSetup(): Boolean;
begin
  FirstInstall := not RegKeyExists(HKLM64, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\TaskbarMonitor_is1');
  Result := True;
end;

function FreshInstall(): Boolean;
begin
  Result := FirstInstall;
end;

// Resolve the opened file before marking that same handle for deletion.
// A redirected parent must never turn an elevated cleanup into deletion elsewhere.
function OpenConfig(Name: String; Access, Share, Security, Mode, Flags, Template: LongWord): Integer;
  external 'CreateFileW@kernel32.dll stdcall';
function FinalName(Handle: Integer; Buffer: String; Count, Flags: LongWord): LongWord;
  external 'GetFinalPathNameByHandleW@kernel32.dll stdcall';
function MarkDelete(Handle: Integer; InfoClass: Integer; var Delete: Byte; Size: LongWord): Boolean;
  external 'SetFileInformationByHandle@kernel32.dll stdcall';
function CloseFileHandle(Handle: Integer): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

function DeleteConfigFile(Path: String): Boolean;
var
  FileHandle: Integer;
  Buffer: String;
  LengthRead: LongWord;
  Disposition: Byte;
begin
  Result := False;
  FileHandle := OpenConfig(Path, $10000, 0, 0, 3, $00200000, 0);
  if FileHandle = -1 then Exit;
  try
    SetLength(Buffer, 32768);
    LengthRead := FinalName(FileHandle, Buffer, Length(Buffer), 0);
    if (LengthRead = 0) or (LengthRead >= 32768) then Exit;
    SetLength(Buffer, LengthRead);
    if CompareText(Buffer, '\\?\' + Path) <> 0 then Exit;
    Disposition := 1;
    Result := MarkDelete(FileHandle, 4, Disposition, 1);
  finally
    CloseFileHandle(FileHandle);
  end;
end;

procedure CurUninstallStepChanged(Step: TUninstallStep);
var
  Value, Config: String;
begin
  if Step <> usPostUninstall then Exit;
  if RegQueryStringValue(HKCU, RunKey, 'TaskbarMonitor', Value) and
     (CompareText(Value, '"' + ExpandConstant('{app}\TaskbarMonitor.exe') + '"') = 0) then
    RegDeleteValue(HKCU, RunKey, 'TaskbarMonitor');
  Config := ExpandConstant('{localappdata}\TaskbarMonitor\config.json');
  if not UninstallSilent and FileExists(Config) then
    if MsgBox('Delete the settings file for this Windows account?' + #13#10 + Config,
              mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
      if not DeleteConfigFile(Config) then
        MsgBox('Windows could not safely remove the settings file. It has been kept.', mbInformation, MB_OK);
end;
