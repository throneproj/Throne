#Requires -Version 5.1
<#
.SYNOPSIS
  Create a local code-signing certificate for MSIX/EXE sideload testing.

.DESCRIPTION
  Uses New-SelfSignedCertificate for development only. Store/production releases
  require a certificate that chains to a Microsoft Trusted Root CA.

  Sets process env for subsequent windows_sign.ps1 / pack_msix.ps1 calls:
    WINDOWS_CERT_PFX_PATH, WINDOWS_CERT_PASSWORD, WINDOWS_CERT_SUBJECT
#>
[CmdletBinding()]
param(
    [string] $Subject = 'CN=AL4AL',

    [string] $OutDir = '',

    [securestring] $Password
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
if (-not $OutDir) { $OutDir = Join-Path $repoRoot '.tools\certs' }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not $Password) {
    $plain = 'ThroneDevCert!'
    $Password = ConvertTo-SecureString -String $plain -Force -AsPlainText
    Write-Warning "Using default password 'ThroneDevCert!' for the local PFX (dev only)."
}

$cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject $Subject `
    -FriendlyName 'Throne local development' `
    -CertStoreLocation 'Cert:\CurrentUser\My' `
    -KeyExportPolicy Exportable `
    -KeySpec Signature `
    -HashAlgorithm SHA256 `
    -NotAfter (Get-Date).AddYears(2)

$pfx = Join-Path $OutDir 'throne-dev.pfx'
$cer = Join-Path $OutDir 'throne-dev.cer'
Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $Password | Out-Null
Export-Certificate -Cert $cert -FilePath $cer | Out-Null

# Trust for current user so Add-AppxPackage / SmartScreen sideload testing is less painful.
Import-Certificate -FilePath $cer -CertStoreLocation 'Cert:\CurrentUser\TrustedPeople' | Out-Null

$env:WINDOWS_CERT_PFX_PATH = $pfx
$env:WINDOWS_CERT_PASSWORD = [Runtime.InteropServices.Marshal]::PtrToStringAuto(
    [Runtime.InteropServices.Marshal]::SecureStringToBSTR($Password))
$env:WINDOWS_CERT_SUBJECT = $Subject
$env:WINDOWS_CERT_THUMBPRINT = $cert.Thumbprint

Write-Host "Created $pfx"
Write-Host "Subject: $Subject"
Write-Host "Thumbprint: $($cert.Thumbprint)"
Write-Host 'Process env WINDOWS_CERT_* is set for this shell only.'
