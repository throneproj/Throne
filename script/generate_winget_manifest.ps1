#Requires -Version 5.1
<#
.SYNOPSIS
  Generate Windows Package Manager (winget) manifests for a Throne GitHub release.

.DESCRIPTION
  Manifest schema follows:
  https://learn.microsoft.com/en-us/windows/package-manager/package/manifest

  Defaults to InstallerType inno for the universal Setup EXE.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Tag,

    [string] $InstallerPath = '',

    [string] $Repo = 'AL4AL/Throne',

    [string] $PackageIdentifier = 'AL4AL.Throne',

    [string] $Publisher = 'AL4AL',

    [string] $OutDir = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot ("packaging\winget\generated\{0}" -f $PackageIdentifier)
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$version = $Tag -replace '^[vV]', ''
$installerName = "Throne-$Tag-windows-universal-installer.exe"
if (-not $InstallerPath) {
    $InstallerPath = Join-Path $repoRoot "deployment\$installerName"
}
if (-not (Test-Path -LiteralPath $InstallerPath)) {
    throw "Installer not found: $InstallerPath"
}

$sha = (Get-FileHash -Algorithm SHA256 -LiteralPath $InstallerPath).Hash.ToLowerInvariant()
$size = (Get-Item -LiteralPath $InstallerPath).Length
$url = "https://github.com/$Repo/releases/download/$Tag/$installerName"

$localePath = Join-Path $OutDir "$PackageIdentifier.locale.en-US.yaml"
$installerPathOut = Join-Path $OutDir "$PackageIdentifier.installer.yaml"
$versionPath = Join-Path $OutDir "$PackageIdentifier.yaml"

@"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.version.1.10.0.schema.json
PackageIdentifier: $PackageIdentifier
PackageVersion: $version
DefaultLocale: en-US
ManifestType: version
ManifestVersion: 1.10.0
"@ | Set-Content -LiteralPath $versionPath -Encoding utf8

@"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.defaultLocale.1.10.0.schema.json
PackageIdentifier: $PackageIdentifier
PackageVersion: $version
PackageLocale: en-US
Publisher: $Publisher
PublisherUrl: https://github.com/$Repo
PublisherSupportUrl: https://github.com/$Repo/issues
Author: $Publisher
PackageName: Throne
PackageUrl: https://github.com/$Repo
License: GPL-3.0
LicenseUrl: https://github.com/$Repo/blob/dev/LICENSE
Copyright: Copyright (c) $Publisher
ShortDescription: Qt desktop proxy manager (sing-box / Xray)
Description: Throne is a Qt desktop proxy manager built on sing-box / Xray. This AL4AL fork ships a universal Windows Setup installer that selects the correct build for your CPU and Windows version.
Moniker: throne
Tags:
  - proxy
  - sing-box
  - xray
  - vpn
  - networking
ReleaseNotesUrl: https://github.com/$Repo/releases/tag/$Tag
ManifestType: defaultLocale
ManifestVersion: 1.10.0
"@ | Set-Content -LiteralPath $localePath -Encoding utf8

@"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.installer.1.10.0.schema.json
PackageIdentifier: $PackageIdentifier
PackageVersion: $version
Platform:
  - Windows.Desktop
MinimumOSVersion: 10.0.17763.0
InstallerType: inno
Scope: user
InstallModes:
  - interactive
  - silent
  - silentWithProgress
InstallerSwitches:
  Silent: /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
  SilentWithProgress: /SILENT /SUPPRESSMSGBOXES /NORESTART
  Custom: /CURRENTUSER
UpgradeBehavior: install
Commands:
  - Throne
Installers:
  - Architecture: neutral
    InstallerUrl: $url
    InstallerSha256: $sha
    InstallerLocale: en-US
    InstallerSize: $size
ManifestType: installer
ManifestVersion: 1.10.0
"@ | Set-Content -LiteralPath $installerPathOut -Encoding utf8

Write-Host "Wrote winget manifests to $OutDir"
Get-ChildItem $OutDir | ForEach-Object { Write-Host ("  {0}" -f $_.Name) }
