#Requires -Version 5.1
<#
.SYNOPSIS
  Install the Windows 11 developer toolchain needed to build and package Throne.

.DESCRIPTION
  Installs (via winget where possible):
    - Visual Studio 2022 Build Tools with MSVC + Windows SDK
    - CMake, Ninja, Go, Inno Setup 6
  Optionally downloads the project's prebuilt Qt/OpenSSL 7z toolchains used by CI.

  Run from an elevated PowerShell for the VS Build Tools workload.
#>
[CmdletBinding()]
param(
    [ValidateSet('x64', 'arm64')]
    [string] $QtTarget = 'x64',

    [string] $QtVersion = '6.12.0',

    [string] $ToolsRoot = '',

    [switch] $SkipQt,

    [switch] $SkipVs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Install-WingetPackage {
    param([string] $Id, [string] $Scope = 'machine')
    Write-Host "==> winget install $Id ($Scope)"
    $args = @(
        'install', '--id', $Id, '-e',
        '--accept-package-agreements', '--accept-source-agreements',
        '--disable-interactivity'
    )
    if ($Scope) { $args += @('--scope', $Scope) }
    & winget @args
    if ($LASTEXITCODE -notin 0, -1978335189) {
        # -1978335189 = already installed
        throw "winget install $Id failed with exit code $LASTEXITCODE"
    }
}

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw 'winget is required. Install App Installer from the Microsoft Store.'
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
if (-not $ToolsRoot) {
    $ToolsRoot = Join-Path $repoRoot '.tools'
}
New-Item -ItemType Directory -Force -Path $ToolsRoot | Out-Null

# User-scope tools first (no elevation).
Install-WingetPackage -Id 'Kitware.CMake' -Scope user
Install-WingetPackage -Id 'Ninja-build.Ninja' -Scope user
Install-WingetPackage -Id 'GoLang.Go' -Scope user
try {
    try {
    Install-WingetPackage -Id 'JRSoftware.InnoSetup' -Scope machine
} catch {
    Write-Warning "Machine-scope Inno Setup failed; trying user scope. $_"
    Install-WingetPackage -Id 'JRSoftware.InnoSetup' -Scope user
}
$isccCandidates = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
)
$iscc = $isccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($iscc) { Write-Host "ISCC: $iscc" } else { Write-Warning 'ISCC.exe not found after Inno Setup install.' }
} catch {
    Write-Warning "Machine-scope Inno Setup failed; trying user scope. $_"
    Install-WingetPackage -Id 'JRSoftware.InnoSetup' -Scope user
}
$isccCandidates = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
)
$iscc = $isccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($iscc) { Write-Host "ISCC: $iscc" } else { Write-Warning 'ISCC.exe not found after Inno Setup install.' }

if (-not $SkipVs) {
    if (-not (Test-IsAdmin)) {
        Write-Warning @'
Visual Studio Build Tools require elevation.
Re-run this script as Administrator, or manually install:
  winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --includeRecommended"
'@
    } else {
        Write-Host '==> Installing VS 2022 Build Tools (MSVC + Windows 11 SDK)'
        & winget install --id Microsoft.VisualStudio.2022.BuildTools -e `
            --accept-package-agreements --accept-source-agreements --disable-interactivity `
            --override '--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --includeRecommended'
        if ($LASTEXITCODE -notin 0, -1978335189) {
            throw "VS Build Tools install failed with exit code $LASTEXITCODE"
        }
    }
}

if (-not $SkipQt) {
    $qt7z = Join-Path $ToolsRoot "Qt_${QtVersion}_${QtTarget}.7z"
    $ssl7z = Join-Path $ToolsRoot "openssl_${QtTarget}.7z"
    $qtUrl = "https://github.com/throneproj/buildqt/releases/download/Qt_${QtVersion}/Qt_${QtVersion}_${QtTarget}.7z"
    $sslUrl = "https://github.com/throneproj/env_windows_legacy/releases/download/latest/openssl_${QtTarget}.7z"

    if (-not (Test-Path $qt7z)) {
        Write-Host "==> Downloading Qt $QtVersion ($QtTarget)"
        Invoke-WebRequest -Uri $qtUrl -OutFile $qt7z
    }
    if (-not (Test-Path $ssl7z)) {
        Write-Host "==> Downloading OpenSSL ($QtTarget)"
        Invoke-WebRequest -Uri $sslUrl -OutFile $ssl7z
    }

    $qtDir = Join-Path $ToolsRoot 'Qt'
    $sslDir = Join-Path $ToolsRoot 'openssl'
    if (-not (Test-Path (Join-Path $qtDir 'lib\cmake'))) {
        Write-Host '==> Extracting Qt'
        if (Test-Path $qtDir) { Remove-Item $qtDir -Recurse -Force }
        & 7z x $qt7z "-o$ToolsRoot" -y | Out-Null
        if (-not (Test-Path $qtDir)) {
            # archive may unpack to ./Qt already under ToolsRoot
            if (-not (Test-Path (Join-Path $ToolsRoot 'Qt\lib\cmake'))) {
                throw 'Qt extract did not produce Qt/lib/cmake'
            }
        }
    }
    if (-not (Test-Path $sslDir)) {
        Write-Host '==> Extracting OpenSSL'
        & 7z x $ssl7z "-o$ToolsRoot" -y | Out-Null
    }

    $envHint = @"

Add these for local builds (current shell):
  `$env:CMAKE_PREFIX_PATH = '$((Join-Path $ToolsRoot 'Qt\lib\cmake'))'
  `$env:OPENSSL_ROOT_DIR  = '$((Join-Path $ToolsRoot 'openssl'))'
  `$env:INPUT_VERSION     = '1.0.5'

Then from a VS Developer PowerShell:
  cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -S . -B build
  # fetch srslist.h into build/ as in docs/RELEASE.md
  ninja -C build
"@
    Write-Host $envHint
}

Write-Host 'setup_windows_dev.ps1 finished.'
