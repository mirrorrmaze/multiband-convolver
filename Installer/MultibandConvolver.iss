; Inno Setup script for Multiband Convolver.
; Builds "Install Multiband Convolver.exe" -- lets the user pick Standalone app / VST3 plugin
; (or both) via checkboxes, and choose the VST3 install folder (defaulting to the standard
; system VST3 folder), matching how most commercial plugin installers work.
;
; Build with: ISCC.exe Installer\MultibandConvolver.iss
; (run from the repo root, or adjust the relative Source paths below if not)

#define MyAppName "Multiband Convolver"
#define MyAppVersion "0.5.0"
#define MyAppPublisher "Mirror Maze"
#define MyBuildDir "..\build\MultibandConvolver_artefacts\Release"

[Setup]
AppId={{6C8B8C6E-6E6E-4C7A-9C2E-3D6B7B7B0A11}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\Installer\Output
OutputBaseFilename=Install Multiband Convolver
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
DisableWelcomePage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Standalone application and VST3 plugin"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Standalone application"; Types: full custom
Name: "vst3"; Description: "VST3 plugin"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut for the standalone app"; GroupDescription: "Additional icons:"; Components: standalone; Flags: unchecked

[Files]
Source: "{#MyBuildDir}\Standalone\Multiband Convolver.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "{#MyBuildDir}\VST3\Multiband Convolver.vst3\*"; DestDir: "{code:GetVST3Dir}\Multiband Convolver.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
; Factory IR library -- installed once to a shared per-user location (not beside either binary)
; since IRLibrary::resolveIRRoot() already checks {userdocs}\MultibandConvolver\IRs as its "user
; content-pack" fallback, and both Standalone and VST3 need to find the exact same files.
; Unconditional (not tied to a Component) since either app needs it to do anything useful.
Source: "..\Resources\IRs\*"; DestDir: "{userdocs}\MultibandConvolver\IRs"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\Multiband Convolver.exe"; Components: standalone
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\Multiband Convolver.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Multiband Convolver.exe"; Description: "Launch {#MyAppName} now"; Flags: nowait postinstall skipifsilent; Check: WizardIsComponentSelected('standalone')

[Code]
var
  VST3DirPage: TInputDirWizardPage;

procedure InitializeWizard;
begin
  VST3DirPage := CreateInputDirPage(wpSelectComponents,
    'Select VST3 Plugin Folder', 'Where should the VST3 plugin be installed?',
    'Setup will install the VST3 plugin in the following folder -- this is the standard system ' +
    'VST3 folder that DAWs scan by default. Only change this if you use a custom plugin folder.',
    False, '');
  VST3DirPage.Add('');
  VST3DirPage.Values[0] := ExpandConstant('{commoncf64}\VST3');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = VST3DirPage.ID then
    Result := not WizardIsComponentSelected('vst3');
end;

function GetVST3Dir(Param: String): String;
begin
  Result := VST3DirPage.Values[0];
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectComponents then
  begin
    if (not WizardIsComponentSelected('standalone')) and (not WizardIsComponentSelected('vst3')) then
    begin
      MsgBox('Please select at least one component to install.', mbError, MB_OK);
      Result := False;
    end;
  end;
end;
