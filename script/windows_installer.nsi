Name "Nekoray"
OutFile "NekoraySetup.exe"
SetShellVarContext all
InstallDir "$APPDATA\Nekoray"
RequestExecutionLevel admin

!include MUI2.nsh
!define MUI_ICON "res\nekoray.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to Nekoray Installer"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of Nekoray."
!define MUI_FINISHPAGE_RUN "$INSTDIR\nekoray.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Nekoray"
!define stopCommand 'powershell -ExecutionPolicy Bypass -WindowStyle Hidden -command "&{Stop-Process -Id (Get-CimInstance -ClassName Win32_Process -Filter $\'Name = $\'$\'nekoray.exe$\'$\'$\' | Where-Object { $$_.ExecutablePath -eq $\'$INSTDIR\nekoray.exe$\' }).ProcessId -Force}"'


!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

UninstallText "This will uninstall Nekoray. Do you wish to continue?"
UninstallIcon "res\nekorayDel.ico"

Section "Install"
  SetOutPath "$INSTDIR"

  ExecWait '${stopCommand}'

  File /r ".\deployment\windows64\*"

  CreateShortcut "$desktop\Nekoray.lnk" "$instdir\nekoray.exe"

  CreateDirectory "$SMPROGRAMS\Nekoray"
  CreateShortcut "$SMPROGRAMS\Nekoray\Nekoray.lnk" "$INSTDIR\nekoray.exe" "" "$INSTDIR\nekoray.exe" 0
  CreateShortcut "$SMPROGRAMS\Nekoray\Uninstall Nekoray.lnk" "$INSTDIR\uninstall.exe"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Nekoray" "DisplayName" "Nekoray"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Nekoray" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Nekoray" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Nekoray" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Nekoray" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "Uninstall"

  ExecWait '${stopCommand}'

  Delete "$SMPROGRAMS\Nekoray\Nekoray.lnk"
  Delete "$SMPROGRAMS\Nekoray\Uninstall Nekoray.lnk"
  Delete "$desktop\Nekoray.lnk"
  RMDir "$SMPROGRAMS\Nekoray"

  RMDir /r "$INSTDIR"

  Delete "$INSTDIR\uninstall.exe"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Nekoray"
SectionEnd