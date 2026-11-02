#Requires -Version 5.1
<#
.SYNOPSIS
  Build a full-trust desktop MSIX for Throne from a staged deployment directory.

.DESCRIPTION
  Manual packaging per Microsoft Learn:
  https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion
  https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool

  Produces:
    deployment/Throne-<Tag>-windows-<arch>.msix
  and optionally signs it when an Authenticode certificate is configured.

  Notes:
    - Full-trust packagedClassicApp (runFullTrust). TUN/system-proxy/service
      features that need unpackaged layout may still prefer the Inno installer.
    - Publisher must match the signing certificate subject.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $SourceDir,

    [Parameter(Mandatory = $true)]
    [string] $Tag,

    [Parameter(Mandatory = $true)]
    [ValidateSet('x64', 'arm64', 'x86')]
    [string] $Architecture,

    [string] $OutDir = '',

    [string] $Publisher = '',

    [string] $PackageName = 'AL4AL.Throne',

    [string] $PublisherDisplayName = 'AL4AL'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Find-SdkTool([string] $Name) {
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($root in @("${env:ProgramFiles(x86)}\Windows Kits\10\bin", "${env:ProgramFiles}\Windows Kits\10\bin")) {
        if (-not (Test-Path $root)) { continue }
        $hit = Get-ChildItem -Path $root -Recurse -Filter $Name -ErrorAction SilentlyContinue |
            Where-Object { $_.Directory.Name -match '^(x64|x86)$' } |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    throw "$Name not found. Install the Windows 10/11 SDK App Packaging Tools."
}

function ConvertTo-MsixVersion([string] $RawTag) {
    $v = $RawTag -replace '^[vV]', '' -replace '^al4al-', ''
    $v = ($v -split '-', 2)[0]
    if ($v -notmatch '^\d+(\.\d+){0,3}$') {
        throw "Cannot derive MSIX version from tag '$RawTag' (need Major.Minor.Build.Revision)."
    }
    $parts = @($v -split '\.')
    while ($parts.Count -lt 4) { $parts += '0' }
    return ($parts[0..3] -join '.')
}

function Ensure-PngAsset {
    param(
        [string] $SourcePng,
        [string] $DestPng,
        [int] $Width,
        [int] $Height
    )
    Add-Type -AssemblyName System.Drawing
    $img = [System.Drawing.Image]::FromFile((Resolve-Path $SourcePng))
    try {
        $bmp = New-Object System.Drawing.Bitmap $Width, $Height
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        try {
            $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $g.Clear([System.Drawing.Color]::Transparent)
            $g.DrawImage($img, 0, 0, $Width, $Height)
            $dir = Split-Path -Parent $DestPng
            if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
            $bmp.Save($DestPng, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $g.Dispose()
            $bmp.Dispose()
        }
    }
    finally {
        $img.Dispose()
    }
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$SourceDir = (Resolve-Path $SourceDir).Path
if (-not $OutDir) { $OutDir = Join-Path $repoRoot 'deployment' }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not (Test-Path (Join-Path $SourceDir 'Throne.exe'))) {
    throw "Throne.exe not found in $SourceDir"
}

if (-not $Publisher) {
    $Publisher = if ($env:WINDOWS_CERT_SUBJECT) { $env:WINDOWS_CERT_SUBJECT } else { 'CN=AL4AL' }
}

$version = ConvertTo-MsixVersion $Tag
$archToken = switch ($Architecture) {
    'x64' { 'x64' }
    'arm64' { 'arm64' }
    'x86' { 'x86' }
}
$artifactStem = switch ($Architecture) {
    'x64' { 'windows-amd64' }
    'arm64' { 'windows-arm64' }
    'x86' { 'windows-x86' }
}

$work = Join-Path ([System.IO.Path]::GetTempPath()) ("throne-msix-{0}-{1}" -f $Architecture, [guid]::NewGuid().ToString('n'))
$payload = Join-Path $work 'payload'
$assets = Join-Path $payload 'Assets'
New-Item -ItemType Directory -Force -Path $assets | Out-Null

try {
    Write-Host "Staging MSIX payload from $SourceDir"
    Copy-Item -Path (Join-Path $SourceDir '*') -Destination $payload -Recurse -Force
    Get-ChildItem -Path $payload -Recurse -Include '*.pdb' -ErrorAction SilentlyContinue | Remove-Item -Force

    $logoSrc = Join-Path $repoRoot 'res\public\Throne.png'
    if (-not (Test-Path $logoSrc)) { throw "Missing $logoSrc" }
    Ensure-PngAsset -SourcePng $logoSrc -DestPng (Join-Path $assets 'StoreLogo.png') -Width 50 -Height 50
    Ensure-PngAsset -SourcePng $logoSrc -DestPng (Join-Path $assets 'Square150x150Logo.png') -Width 150 -Height 150
    Ensure-PngAsset -SourcePng $logoSrc -DestPng (Join-Path $assets 'Square44x44Logo.png') -Width 44 -Height 44
    Ensure-PngAsset -SourcePng $logoSrc -DestPng (Join-Path $assets 'Square44x44Logo.targetsize-44_altform-unplated.png') -Width 44 -Height 44

    $manifest = @"
<?xml version="1.0" encoding="utf-8"?>
<Package
  xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
  xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
  xmlns:uap10="http://schemas.microsoft.com/appx/manifest/uap/windows10/10"
  xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
  IgnorableNamespaces="uap uap10 rescap">
  <Identity
    Name="$PackageName"
    Publisher="$Publisher"
    Version="$version"
    ProcessorArchitecture="$archToken" />
  <Properties>
    <DisplayName>Throne</DisplayName>
    <PublisherDisplayName>$PublisherDisplayName</PublisherDisplayName>
    <Description>Qt desktop proxy manager (sing-box / Xray)</Description>
    <Logo>Assets\StoreLogo.png</Logo>
  </Properties>
  <Resources>
    <Resource Language="en-us" />
  </Resources>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.17763.0" MaxVersionTested="10.0.26100.0" />
  </Dependencies>
  <Capabilities>
    <Capability Name="internetClient" />
    <Capability Name="internetClientServer" />
    <Capability Name="privateNetworkClientServer" />
    <rescap:Capability Name="runFullTrust" />
    <rescap:Capability Name="allowElevation" />
  </Capabilities>
  <Applications>
    <Application
      Id="Throne"
      Executable="Throne.exe"
      EntryPoint="Windows.FullTrustApplication"
      uap10:RuntimeBehavior="packagedClassicApp"
      uap10:TrustLevel="mediumIL">
      <uap:VisualElements
        DisplayName="Throne"
        Description="Qt desktop proxy manager (sing-box / Xray)"
        BackgroundColor="transparent"
        Square150x150Logo="Assets\Square150x150Logo.png"
        Square44x44Logo="Assets\Square44x44Logo.png" />
      <Extensions>
        <uap:Extension Category="windows.protocol">
          <uap:Protocol Name="throne">
            <uap:DisplayName>Throne</uap:DisplayName>
          </uap:Protocol>
        </uap:Extension>
        <uap:Extension Category="windows.fileTypeAssociation">
          <uap:FileTypeAssociation Name="throneconfig">
            <uap:SupportedFileTypes>
              <uap:FileType>.json</uap:FileType>
              <uap:FileType>.yaml</uap:FileType>
              <uap:FileType>.yml</uap:FileType>
              <uap:FileType>.conf</uap:FileType>
            </uap:SupportedFileTypes>
            <uap:DisplayName>Throne config</uap:DisplayName>
          </uap:FileTypeAssociation>
        </uap:Extension>
      </Extensions>
    </Application>
  </Applications>
</Package>
"@
    $manifestPath = Join-Path $payload 'AppxManifest.xml'
    $utf8NoBom = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($manifestPath, $manifest, $utf8NoBom)

    $makeappx = Find-SdkTool 'makeappx.exe'
    $outMsix = Join-Path $OutDir ("Throne-{0}-{1}.msix" -f $Tag, $artifactStem)
    if (Test-Path $outMsix) { Remove-Item -LiteralPath $outMsix -Force }

    Write-Host "Packing $outMsix"
    & $makeappx pack /o /d $payload /p $outMsix
    if ($LASTEXITCODE -ne 0) { throw "makeappx failed with exit code $LASTEXITCODE" }

    $signScript = Join-Path $PSScriptRoot 'windows_sign.ps1'
    & $signScript -Path $outMsix -Description 'Throne MSIX'
    Write-Host "Created $outMsix"
    return $outMsix
}
finally {
    if (Test-Path $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
}
