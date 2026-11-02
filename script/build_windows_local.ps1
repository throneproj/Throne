#Requires -Version 5.1
<#
.SYNOPSIS
  Configure and build Throne on Windows using the local .tools toolchain.
#>
[CmdletBinding()]
param(
    [string] $BuildDir = 'build',
    [string] $Config = 'RelWithDebInfo',
    [string] $Version = '1.0.5',
    [string] $ToolsRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $repoRoot
if (-not $ToolsRoot) { $ToolsRoot = Join-Path $repoRoot '.tools' }

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    $hit = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter cmake.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($hit) { $env:Path = "$(Split-Path $hit.FullName);$env:Path" }
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'cmake not found. Run script/setup_windows_dev.ps1 first.'
}

$ninja = Join-Path $ToolsRoot 'ninja.exe'
if (Test-Path $ninja) {
    $env:Path = "$ToolsRoot;$env:Path"
} elseif (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    throw 'ninja not found. Run script/setup_windows_dev.ps1 first.'
}

$vsDevCmd = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -find 'Common7\Tools\VsDevCmd.bat' 2>$null |
    Select-Object -First 1
if (-not $vsDevCmd) {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    )
    $vsDevCmd = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $vsDevCmd) {
    throw @'
MSVC toolchain not found.
Install with an elevated PowerShell:
  powershell -File script/setup_windows_dev.ps1
or:
  winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
'@
}

$qtCmake = Join-Path $ToolsRoot 'Qt\lib\cmake'
$sslRoot = Join-Path $ToolsRoot 'openssl'
if (-not (Test-Path $qtCmake)) {
    throw "Qt CMake package missing at $qtCmake. Re-run setup_windows_dev.ps1 without -SkipQt."
}

$env:INPUT_VERSION = $Version
$env:CMAKE_PREFIX_PATH = $qtCmake
$env:OPENSSL_ROOT_DIR = $sslRoot

Write-Host "Entering VS developer environment: $vsDevCmd"
$inner = @"
call `"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
cmake -GNinja -DCMAKE_BUILD_TYPE=$Config -S `"$repoRoot`" -B `"$repoRoot\$BuildDir`"
if errorlevel 1 exit /b 1
curl.exe -fLso `"$repoRoot\$BuildDir\srslist.h`" https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/srslist.h
if errorlevel 1 exit /b 1
cmake --build `"$repoRoot\$BuildDir`"
"@
$cmdPath = Join-Path $env:TEMP 'throne-win-build.cmd'
Set-Content -LiteralPath $cmdPath -Value $inner -Encoding ASCII
cmd.exe /c $cmdPath
if ($LASTEXITCODE -ne 0) { throw "Windows build failed with exit code $LASTEXITCODE" }
Write-Host "Build OK: $repoRoot\$BuildDir\Throne.exe"
