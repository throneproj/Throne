#Requires -Version 5.1
<#
.SYNOPSIS
  Authenticode-sign Windows binaries and packages with signtool.

.DESCRIPTION
  Follows Microsoft code-signing guidance for Win32/MSIX distribution:
  https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool

  Certificate sources (first match wins):
    1. WINDOWS_CERT_PFX_PATH + WINDOWS_CERT_PASSWORD
    2. WINDOWS_CERT_PFX_BASE64 + WINDOWS_CERT_PASSWORD  (CI secret)
    3. WINDOWS_CERT_THUMBPRINT  (certificate already in the CurrentUser/LocalMachine store)

  Optional:
    WINDOWS_CERT_TIMESTAMP_URL  (default DigiCert RFC3161)
    WINDOWS_CERT_SUBJECT        (used only for validation / MSIX publisher alignment)

  When no certificate is configured, exits 0 with a skip message so unsigned
  local/CI builds keep working.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string[]] $Path,

    [string] $Description = 'Throne',

    [switch] $Strict
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Find-SignTool {
    $cmd = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "${env:ProgramFiles}\Windows Kits\10\bin"
    )
    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }
        $hit = Get-ChildItem -Path $root -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    return $null
}

function Resolve-CertificateArgs {
    $timestamp = if ($env:WINDOWS_CERT_TIMESTAMP_URL) {
        $env:WINDOWS_CERT_TIMESTAMP_URL
    } else {
        'http://timestamp.digicert.com'
    }

    if ($env:WINDOWS_CERT_PFX_PATH -and (Test-Path -LiteralPath $env:WINDOWS_CERT_PFX_PATH)) {
        if (-not $env:WINDOWS_CERT_PASSWORD) {
            throw 'WINDOWS_CERT_PASSWORD is required when WINDOWS_CERT_PFX_PATH is set.'
        }
        return @{
            Args      = @('/f', $env:WINDOWS_CERT_PFX_PATH, '/p', $env:WINDOWS_CERT_PASSWORD)
            Timestamp = $timestamp
            Cleanup   = $null
        }
    }

    if ($env:WINDOWS_CERT_PFX_BASE64) {
        if (-not $env:WINDOWS_CERT_PASSWORD) {
            throw 'WINDOWS_CERT_PASSWORD is required when WINDOWS_CERT_PFX_BASE64 is set.'
        }
        $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("throne-sign-{0}.pfx" -f [guid]::NewGuid().ToString('n'))
        [IO.File]::WriteAllBytes($tmp, [Convert]::FromBase64String($env:WINDOWS_CERT_PFX_BASE64))
        return @{
            Args      = @('/f', $tmp, '/p', $env:WINDOWS_CERT_PASSWORD)
            Timestamp = $timestamp
            Cleanup   = $tmp
        }
    }

    if ($env:WINDOWS_CERT_THUMBPRINT) {
        return @{
            Args      = @('/sha1', ($env:WINDOWS_CERT_THUMBPRINT -replace '\s', ''))
            Timestamp = $timestamp
            Cleanup   = $null
        }
    }

    return $null
}

$files = foreach ($item in $Path) {
    Get-Item -LiteralPath $item -ErrorAction Stop
}
if (-not $files) {
    Write-Host 'windows_sign: no files to sign.'
    exit 0
}

$cert = Resolve-CertificateArgs
if (-not $cert) {
    $msg = 'windows_sign: no Authenticode certificate configured; skipping signing.'
    if ($Strict) { throw $msg }
    Write-Host $msg
    exit 0
}

$signtool = Find-SignTool
if (-not $signtool) {
    throw 'signtool.exe not found. Install the Windows SDK Signing Tools component.'
}

try {
    foreach ($file in $files) {
        Write-Host ("Signing {0}" -f $file.FullName)
        & $signtool sign `
            /fd SHA256 `
            /td SHA256 `
            /tr $cert.Timestamp `
            /d $Description `
            @($cert.Args) `
            $file.FullName
        if ($LASTEXITCODE -ne 0) {
            throw ("signtool failed for {0} with exit code {1}" -f $file.FullName, $LASTEXITCODE)
        }
    }
}
finally {
    if ($cert.Cleanup -and (Test-Path -LiteralPath $cert.Cleanup)) {
        Remove-Item -LiteralPath $cert.Cleanup -Force -ErrorAction SilentlyContinue
    }
}
