@echo off
rem Installs Throne as a Windows service that runs headless as LocalSystem. From an elevated prompt:
rem   install-service.bat ["C:\Program Files\Throne\Throne.exe"]
rem The service runs as SYSTEM, so Throne.exe must sit in a folder only administrators can write:
rem install Throne for all users (Program Files), not for one user under AppData.
setlocal

set "EXE=%~1"
if not defined EXE set "EXE=%ProgramFiles%\Throne\Throne.exe"
if not exist "%EXE%" (
    echo Throne.exe not found: "%EXE%"
    exit /b 1
)
fltmc >nul 2>&1 || (
    echo Run this from an elevated Command Prompt.
    exit /b 1
)
sc.exe query Throne >nul 2>&1 && (
    echo The Throne service already exists; run uninstall-service.bat first.
    exit /b 1
)

rem No --data-dir: in service mode Throne uses ProgramData\Throne and creates it for SYSTEM and Administrators only.
sc.exe create Throne binPath= "\"%EXE%\" --headless --service" start= delayed-auto obj= LocalSystem DisplayName= "Throne (headless)" || exit /b 1
sc.exe description Throne "Throne proxy manager without a window; manage it with thronectl."
rem A crash or any non-zero exit (75: restart requested) restarts the service; the failure count resets after a day.
sc.exe failure Throne reset= 86400 actions= restart/3000/restart/10000/restart/60000
sc.exe failureflag Throne 1
sc.exe start Throne
