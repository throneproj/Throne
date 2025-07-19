Name "Throne"
OutFile "ThroneSetup.exe"
InstallDir $APPDATA\Throne
RequestExecutionLevel admin

!include MUI2.nsh
!define MUI_ICON "res\Throne.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to Throne Installer"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of Throne."
!define MUI_FINISHPAGE_RUN "$INSTDIR\Throne.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Throne"
!define stopCommand 'powershell -ExecutionPolicy Bypass -WindowStyle Hidden -command "&{Stop-Process -Id (Get-CimInstance -ClassName Win32_Process -Filter $\'Name = $\'$\'Throne.exe$\'$\'$\' | Where-Object { $$_.ExecutablePath -eq $\'$INSTDIR\Throne.exe$\' }).ProcessId -Force}"'


!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

UninstallText "This will uninstall Throne. Do you wish to continue?"
UninstallIcon "res\ThroneDel.ico"

Section "Install"
  SetOutPath "$INSTDIR"

  ExecWait '${stopCommand}'

  File /r ".\deployment\windows64\*"

  CreateShortcut "$desktop\Throne.lnk" "$instdir\Throne.exe"

  CreateDirectory "$SMPROGRAMS\Throne"
  CreateShortcut "$SMPROGRAMS\Throne\Throne.lnk" "$INSTDIR\Throne.exe" "" "$INSTDIR\Throne.exe" 0
  CreateShortcut "$SMPROGRAMS\Throne\Uninstall Throne.lnk" "$INSTDIR\uninstall.exe"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "DisplayName" "Throne"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "Uninstall"

  ExecWait '${stopCommand}'

  Delete "$SMPROGRAMS\Throne\Throne.lnk"
  Delete "$SMPROGRAMS\Throne\Uninstall Throne.lnk"
  Delete "$desktop\Throne.lnk"
  RMDir "$SMPROGRAMS\Throne"

  RMDir /r "$INSTDIR"

  Delete "$INSTDIR\uninstall.exe"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne"
SectionEnd
