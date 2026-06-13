; ============================================================================
;  AVRDUDE GUI – Inno Setup 6 installer script
;  package/windows.iss
;
;  Prerequisites
;  -------------
;  1. Inno Setup 6.x  (https://jrsoftware.org/isinfo.php)
;  2. Build + windeployqt the project first:
;       cmake --build build --config Release
;     This produces build\deploy\ with adgui.exe + all Qt DLLs.
;  3. Optional: Inno Setup Preprocessor (ISPP) is used for version macros
;     (ships with the standard Inno Setup installer).
;
;  To compile from the command line:
;       iscc package\windows.iss
;  Or open in Inno Setup IDE and press F9.
; ============================================================================

#define AppName      "AVRDUDE GUI"
#define AppVersion   "1.0.0"
#define AppPublisher "avrdudes"
#define AppURL       "https://github.com/avrdudes/avrdude"
#define AppExeName   "adgui.exe"
#define DeployDir    "..\build\deploy"    ; relative to this .iss file

; ── Output ──────────────────────────────────────────────────────────────────
[Setup]
AppId                    ={{A3F2B8C1-7D4E-4F29-9A0B-1E6C5D8F3A72}
AppName                  ={#AppName}
AppVersion               ={#AppVersion}
AppPublisher             ={#AppPublisher}
AppPublisherURL          ={#AppURL}
AppSupportURL            ={#AppURL}/issues
AppUpdatesURL            ={#AppURL}/releases
DefaultDirName           ={autopf}\{#AppName}
DefaultGroupName         ={#AppName}
AllowNoIcons             =yes
; License shown on the first installer page
LicenseFile              =..\LICENSE

; Output installer filename
OutputDir                =..\dist
OutputBaseFilename       =AVRDUDE-GUI-{#AppVersion}-win64-setup

; Installer icon (optional – provide a 256×256 .ico or comment these out)
;SetupIconFile           ={#DeployDir}\..\package\adgui.ico
;UninstallDisplayIcon    ={app}\{#AppExeName}

; Compression
Compression              =lzma2/ultra64
SolidCompression         =yes
LZMAUseSeparateProcess   =yes

; Architecture
ArchitecturesAllowed             =x64compatible
ArchitecturesInstallIn64BitMode  =x64compatible

; Require Windows 10 1809+ (same baseline as Qt 6.5)
MinVersion               =10.0.17763

; Add/Remove Programs appearance
UninstallDisplayName     ={#AppName} {#AppVersion}
VersionInfoVersion       ={#AppVersion}
VersionInfoCompany       ={#AppPublisher}
VersionInfoDescription   ={#AppName} Installer

; Privileges: "lowest" = no UAC prompt when installing to %LocalAppData%
; Change to "admin" if you need to write to %ProgramFiles% by default.
PrivilegesRequired       =lowest
PrivilegesRequiredOverridesAllowed=dialog

; ── Languages ────────────────────────────────────────────────────────────────
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

; ── Tasks (optional checkboxes on the installer pages) ──────────────────────
[Tasks]
Name: "desktopicon";   Description: "{cm:CreateDesktopIcon}";   \
      GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startmenuicon"; Description: "Create a Start Menu shortcut"; \
      GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce

; ── Files ────────────────────────────────────────────────────────────────────
; Everything produced by windeployqt lands in DeployDir.
; The recursive wildcard picks up:
;   adgui.exe
;   Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll, …
;   libavrdude.dll  (if present)
;   platforms\qwindows.dll
;   styles\qwindowsvistastyle.dll
;   imageformats\*.dll
;   translations\qt_*.qm
;   Any VC++ runtime DLLs staged by windeployqt
[Files]
; Main application tree
Source: "{#DeployDir}\*";  DestDir: "{app}"; \
        Flags: ignoreversion recursesubdirs createallsubdirs

; avrdude.conf – place the system-wide config next to the exe
; (comment out if you prefer to rely on the one in %PROGRAMDATA%)
;Source: "..\avrdude.conf";  DestDir: "{app}";  Flags: ignoreversion

; ── Shortcuts ────────────────────────────────────────────────────────────────
[Icons]
; Start Menu
Name: "{group}\{#AppName}";      Filename: "{app}\{#AppExeName}"; \
      Tasks: startmenuicon
Name: "{group}\Uninstall {#AppName}"; \
      Filename: "{uninstallexe}"

; Desktop
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; \
      Tasks: desktopicon

; ── Registry ─────────────────────────────────────────────────────────────────
; Store the install path so other tools (e.g. avrdude CLI) can find the GUI.
[Registry]
Root: HKCU; Subkey: "Software\{#AppPublisher}\{#AppName}"; \
      ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; \
      Flags: uninsdeletekey

; ── Run after install ────────────────────────────────────────────────────────
[Run]
Filename: "{app}\{#AppExeName}"; \
          Description: "{cm:LaunchProgram,{#StringChange(AppName,'&','&&')}}"; \
          Flags: nowait postinstall skipifsilent

; ── Uninstall: remove user settings ─────────────────────────────────────────
; Qt stores settings in %APPDATA%\avrdudes\adgui.ini (or registry).
; Offer to clean them up on uninstall.
[UninstallRun]
; Nothing needed here – QSettings cleanup is handled via [UninstallDelete].

[UninstallDelete]
; Remove Qt INI settings written by QSettings("avrdudes","adgui")
Type: files;     Name: "{userappdata}\avrdudes\adgui.ini"
Type: dirifempty; Name: "{userappdata}\avrdudes"

; ── Custom messages ──────────────────────────────────────────────────────────
[Messages]
BeveledLabel=AVRDUDE GUI {#AppVersion}

; ── Pascal script: pre-install checks ────────────────────────────────────────
[Code]
// Check that the VC++ runtime is present.
// windeployqt normally stages it, but this is a belt-and-suspenders check.
function VCRedistInstalled: Boolean;
var
  SubKey: string;
  Installed: Cardinal;
begin
  // VC++ 2019/2022 x64 redistributable
  SubKey := 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64';
  Result := RegQueryDWordValue(HKLM, SubKey, 'Installed', Installed) and
            (Installed = 1);
end;

function InitializeSetup: Boolean;
begin
  Result := True;
  // Uncomment if you ship MSVC build and want to warn the user:
  // if not VCRedistInstalled then
  //   MsgBox('The Visual C++ 2019/2022 x64 redistributable is not installed.'#13#10
  //          + 'Please install it from Microsoft before continuing.',
  //          mbError, MB_OK);
end;
