@echo off
rem Stops and removes the Throne service. From an elevated prompt. The data in ProgramData\Throne stays.
setlocal

fltmc >nul 2>&1 || (
    echo Run this from an elevated Command Prompt.
    exit /b 1
)
sc.exe query Throne >nul 2>&1 || (
    echo The Throne service is not installed.
    exit /b 0
)

sc.exe stop Throne >nul 2>&1
rem A clean stop (core Stop, kill-switch teardown) can take up to 45 s.
set /a WAITED=0
:wait
sc.exe query Throne | findstr /c:"STOPPED" >nul && goto delete
if %WAITED% geq 60 (
    echo The service did not stop within 60 s; it is removed once it stops.
    goto delete
)
set /a WAITED+=1
ping -n 2 127.0.0.1 >nul
goto wait

:delete
sc.exe delete Throne
