!ifndef APP_VERSION
  !define APP_VERSION "0.0.0"
!endif
!ifndef APP_VERSION_MAJOR
  !define APP_VERSION_MAJOR 0
!endif
!ifndef APP_VERSION_MINOR
  !define APP_VERSION_MINOR 0
!endif
!ifndef APP_VERSION_PATCH
  !define APP_VERSION_PATCH 0
!endif
!ifndef APP_VERSION_BUILD
  !define APP_VERSION_BUILD 0
!endif

Name "Throne ${APP_VERSION}"
OutFile "ThroneSetup.exe"

; 1. NEVER ask for UAC on launch
RequestExecutionLevel user 

SetCompressor /SOLID /FINAL lzma
SetCompressorDictSize 64

!include MUI2.nsh
!include nsDialogs.nsh
!include LogicLib.nsh
!include FileFunc.nsh
!include WinVer.nsh
!include x64.nsh

!define APP_DIR_NAME "Throne"

!define MUI_ICON "res\Throne.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to Throne Installer"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of Throne."
!define MUI_FINISHPAGE_RUN "$INSTDIR\Throne.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Throne"
!addplugindir .\script\

; This is the Windows constant used to draw the UAC Shield on a button
!ifndef BCM_SETSHIELD
  !define BCM_SETSHIELD 0x160C
!endif

Var RadioAllUsers
Var RadioJustMe
Var IsAllUsers
Var SkipPages
Var UninstPath
Var DeleteUserData
Var CheckDeleteData

; =====================================
; PAGE SEQUENCE
; =====================================
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipPageCheck
!insertmacro MUI_PAGE_WELCOME

; Custom pages do not use MUI_PAGE_CUSTOMFUNCTION_PRE.
; The skip logic is handled directly inside InstallModePageCreate.
Page custom InstallModePageCreate InstallModePageLeave

!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipPageCheck
!define MUI_PAGE_CUSTOMFUNCTION_SHOW DirectoryShow
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryLeave
!define MUI_DIRECTORYPAGE_TEXT_TOP "Setup will install Throne in the folder below. If the folder you choose is not named '${APP_DIR_NAME}', Setup creates a '${APP_DIR_NAME}' subfolder inside it, so uninstalling only ever removes Throne's own folder."
!insertmacro MUI_PAGE_DIRECTORY

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
UninstPage custom un.DataPageCreate un.DataPageLeave
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

VIProductVersion "${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH}.${APP_VERSION_BUILD}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "Throne"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "Throne Setup"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "Throne"

UninstallText "This will uninstall Throne. Do you wish to continue?"
UninstallIcon "res\ThroneDel.ico"

; =====================================
; INIT & SEAMLESS RESTART LOGIC
; =====================================
Function .onInit
  ; Check if this is the elevated restart
  ${GetParameters} $R0
  ${GetOptions} $R0 "/ELEVATED" $R1

  ${IfNot} ${Errors}
    ; We are the elevated process! Skip the UI pages and set context to All Users
    StrCpy $SkipPages 1
    StrCpy $IsAllUsers "1"

    ; Read the chosen installation path from the temporary registry key
    ReadRegStr $INSTDIR HKCU "Software\Throne" "TempSetupPath"
    DeleteRegValue HKCU "Software\Throne" "TempSetupPath" ; Clean it up immediately

    ${If} $INSTDIR == ""
      StrCpy $INSTDIR "$PROGRAMFILES64\${APP_DIR_NAME}"
    ${EndIf}

    SetShellVarContext all
  ${Else}
    ; Normal launch. Default to "Just Me" (Current User)
    StrCpy $IsAllUsers "0"
    StrCpy $INSTDIR "$LOCALAPPDATA\${APP_DIR_NAME}"
    SetShellVarContext current
  ${EndIf}
FunctionEnd

Function SkipPageCheck
  ${If} $SkipPages == 1
    Abort ; Skipping this page because we are already elevated and ready to install
  ${EndIf}
FunctionEnd

; =====================================
; CUSTOM JUST ME / ALL USERS PAGE
; =====================================
Function InstallModePageCreate
  ${If} $SkipPages == 1
    Abort ; Skip this page if elevated
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "Installation Options" "Choose who can use this application."
  nsDialogs::Create 1018
  Pop $0

  ${NSD_CreateLabel} 0 0 100% 24u "Please select whether you wish to make this software available to all users or just yourself."
  Pop $0

  ${NSD_CreateRadioButton} 10u 30u 100% 12u "Install for anyone using this computer"
  Pop $RadioAllUsers

  ${NSD_CreateRadioButton} 10u 50u 100% 12u "Install just for me"
  Pop $RadioJustMe

  ${If} $IsAllUsers == "1"
    SendMessage $RadioAllUsers ${BM_SETCHECK} ${BST_CHECKED} 0
  ${Else}
    SendMessage $RadioJustMe ${BM_SETCHECK} ${BST_CHECKED} 0
  ${EndIf}

  nsDialogs::Show
FunctionEnd

Function InstallModePageLeave
  ${NSD_GetState} $RadioAllUsers $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $IsAllUsers "1"
    StrCpy $INSTDIR "$PROGRAMFILES64\${APP_DIR_NAME}"
  ${Else}
    StrCpy $IsAllUsers "0"
    StrCpy $INSTDIR "$LOCALAPPDATA\${APP_DIR_NAME}"
  ${EndIf}
FunctionEnd

; =====================================
; DIRECTORY PAGE & UAC SHIELD
; =====================================
Function DirectoryShow
  ; Target the "Next" (Install) button
  GetDlgItem $0 $HWNDPARENT 1

  ${If} $IsAllUsers == "1"
    SendMessage $0 ${BCM_SETSHIELD} 0 1 ; Draw Shield
  ${Else}
    SendMessage $0 ${BCM_SETSHIELD} 0 0 ; Remove Shield
  ${EndIf}
FunctionEnd

Function EnsureAppSubfolder
  ; A hand-typed "D:\Apps\" would otherwise append into "D:\Apps\\Throne".
  StrCpy $0 $INSTDIR "" -1
  ${If} $0 == '\'
    StrCpy $INSTDIR $INSTDIR -1
  ${EndIf}

  ${GetFileName} "$INSTDIR" $0
  ${If} $0 != "${APP_DIR_NAME}"
    StrCpy $INSTDIR "$INSTDIR\${APP_DIR_NAME}"
  ${EndIf}
FunctionEnd

Function DirectoryLeave
  ; Must run before the All Users branch hands the path to the elevated process.
  Call EnsureAppSubfolder

  ${If} $IsAllUsers == "1"
    ; --- ALL USERS LOGIC ---
    UserInfo::GetAccountType
    Pop $0
    ${If} $0 != "Admin"
      ; Write the chosen path safely to the registry for the elevated process to grab
      WriteRegStr HKCU "Software\Throne" "TempSetupPath" "$INSTDIR"

      ; Trigger UAC and silently launch the elevated installer
      ExecShell "runas" "$EXEPATH" "/ELEVATED"
      Quit ; Close the unelevated window
    ${EndIf}
    SetShellVarContext all

  ${Else}
    ; --- JUST ME LOGIC ---
    ; Verify they aren't trying to install to an Admin folder without Admin rights
    ClearErrors
    CreateDirectory "$INSTDIR"
    IfErrors AccessDenied

    GetTempFileName $0 "$INSTDIR"
    FileOpen $1 $0 w
    FileWrite $1 "test"
    FileClose $1
    IfErrors AccessDenied

    Delete $0
    SetShellVarContext current
    Goto DoneCheck

    AccessDenied:
      RMDir "$INSTDIR" ; Drop the folder the write test just created (no-op unless empty)
      MessageBox MB_OK|MB_ICONSTOP "You do not have permission to install to '$INSTDIR'.$\n$\nPlease choose a different folder, or select 'Install for anyone using this computer'."
      Abort ; Forces them to stay on the Directory page

    DoneCheck:
  ${EndIf}
FunctionEnd

!macro AbortOnRunningApp EXEName
  killModule:
  FindProcDLL::FindProc ${EXEName}
  Pop $R0
  IntCmp $R0 1 0 notRunning
    FindProcDLL::KillProc ${EXEName}
    Sleep 1000
    Goto killModule
  notRunning:
!macroend

; =====================================
; INSTALLATION
; =====================================
Section "Install"
  SetOutPath "$INSTDIR"
  SetOverwrite on

  !insertmacro AbortOnRunningApp "$INSTDIR\Throne.exe"

  ${If} ${IsNativeAMD64}
    ${If} ${AtLeastWaaS} 1809
      File /oname=libcronet.dll "deployment\windows-amd64\libcronet.dll"
      File /oname=ThroneCore.exe "deployment\windows-amd64\ThroneCore.exe"
      File /oname=Throne.exe "deployment\windows-amd64\Throne.exe"
      File /oname=updater.exe "deployment\windows-amd64\updater.exe"
    ${Else}
      File /oname=ThroneCore.exe "deployment\windowslegacy-amd64\ThroneCore.exe"
      File /oname=Throne.exe "deployment\windowslegacy-amd64\Throne.exe"
      File /oname=updater.exe "deployment\windowslegacy-amd64\updater.exe"
    ${EndIf}
  ${ElseIf} ${IsNativeARM64}
    File /oname=libcronet.dll "deployment\windows-arm64\libcronet.dll"
    File /oname=ThroneCore.exe "deployment\windows-arm64\ThroneCore.exe"
    File /oname=Throne.exe "deployment\windows-arm64\Throne.exe"
    File /oname=updater.exe "deployment\windows-arm64\updater.exe"
  ${ElseIf} ${IsNativeIA32}
    File /oname=ThroneCore.exe "deployment\windowslegacy-386\ThroneCore.exe"
    File /oname=Throne.exe "deployment\windowslegacy-386\Throne.exe"
    File /oname=updater.exe "deployment\windowslegacy-386\updater.exe"
  ${Else}
    Abort "Unsupported CPU architecture!"
  ${EndIf}

  CreateShortcut "$DESKTOP\Throne.lnk" "$INSTDIR\Throne.exe"
  CreateShortcut "$SMPROGRAMS\Throne.lnk" "$INSTDIR\Throne.exe" "" "$INSTDIR\Throne.exe" 0

  WriteRegStr SHCTX "Software\Throne" "InstallPath" "$INSTDIR"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "DisplayName" "Throne"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "Publisher" "Throne"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "DisplayIcon" "$INSTDIR\Throne.exe"
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "VersionMajor" ${APP_VERSION_MAJOR}
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "VersionMinor" ${APP_VERSION_MINOR}
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "InstallLocation" "$INSTDIR"
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "NoModify" 1
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

; =====================================
; UNINSTALLER
; =====================================
Function un.onInit
  ClearErrors
  StrCpy $DeleteUserData 1

  ; Grab the folder passed via command-line (if we elevated), otherwise use standard folder
  ${GetParameters} $R0
  ${GetOptions} $R0 "/UINSTDIR=" $R1

  ${If} $R1 != ""
    StrCpy $UninstPath $R1
  ${Else}
    StrCpy $UninstPath $INSTDIR
  ${EndIf}

  ; Read the Admin registry to see if THIS specific folder belongs to an Admin installation
  ReadRegStr $0 HKLM "Software\Throne" "InstallPath"

  ${If} $0 == $UninstPath
    ; --- IT IS AN ALL USERS INSTALL ---
    SetShellVarContext all
    UserInfo::GetAccountType
    Pop $1
    ${If} $1 != "Admin"
       MessageBox MB_YESNO|MB_ICONEXCLAMATION "Uninstalling Throne for all users requires Administrator privileges.$\n$\nDo you want to elevate?" IDNO Stay
       ; Elevate via UAC and explicitly pass the real folder path in quotes!
       ExecShell "runas" "$EXEPATH" '/UINSTDIR="$UninstPath"'
       Quit
    Stay:
       Abort
    ${EndIf}
  ${Else}
    ; --- IT IS A JUST ME INSTALL (OR ORPHANED) ---
    SetShellVarContext current
  ${EndIf}

  ; Force $INSTDIR to the true path so %TEMP% executions don't delete themselves!
  StrCpy $INSTDIR $UninstPath
FunctionEnd

; =====================================
; USER DATA PAGE
; =====================================
Function un.DataPageCreate
  !insertmacro MUI_HEADER_TEXT "Remove Settings" "Choose what to do with your Throne data."

  nsDialogs::Create 1018
  Pop $0

  ${NSD_CreateLabel} 0 0 100% 24u "Throne keeps its profiles, settings and logs in the installation folder, or under your user profile when that folder is not writable."
  Pop $0

  ${NSD_CreateCheckbox} 10u 30u 100% 12u "Delete my profiles, settings and logs"
  Pop $CheckDeleteData
  ${If} $DeleteUserData == 1
    SendMessage $CheckDeleteData ${BM_SETCHECK} ${BST_CHECKED} 0
  ${EndIf}

  ${NSD_CreateLabel} 10u 48u 100% 20u "Clear this if you plan to reinstall Throne later and want to keep them."
  Pop $0

  nsDialogs::Show
FunctionEnd

Function un.DataPageLeave
  ${NSD_GetState} $CheckDeleteData $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $DeleteUserData 1
  ${Else}
    StrCpy $DeleteUserData 0
  ${EndIf}
FunctionEnd

Section "Uninstall"
  !insertmacro AbortOnRunningApp "$INSTDIR\Throne.exe"

  Delete "$SMPROGRAMS\Throne.lnk"
  Delete "$DESKTOP\Throne.lnk"
  RMDir "$SMPROGRAMS\Throne"

  Delete "$INSTDIR\libcronet.dll"
  Delete "$INSTDIR\ThroneCore.exe"
  Delete "$INSTDIR\Throne.exe"
  Delete "$INSTDIR\updater.exe"
  Delete "$INSTDIR\updater.old"
  Delete "$INSTDIR\uninstall.exe"

  ${If} $DeleteUserData == 1
  ${AndIf} ${FileExists} "$INSTDIR\config\throne.db"
    RMDir /r "$INSTDIR\config"
  ${EndIf}

  RMDir "$INSTDIR"

  ; Clean up registry!
  DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne"
  DeleteRegKey SHCTX "Software\Throne"

  ; Last, because SHCTX follows the shell var context and an all-users uninstall
  ; would otherwise resolve $APPDATA to ProgramData.
  ${If} $DeleteUserData == 1
    SetShellVarContext current
    ${If} ${FileExists} "$APPDATA\${APP_DIR_NAME}\*.*"
      RMDir /r "$APPDATA\${APP_DIR_NAME}"
    ${EndIf}
  ${EndIf}
SectionEnd
