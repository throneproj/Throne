#Requires -Version 7.2
<#
.SYNOPSIS
    W1: headless Throne as a Windows service, end to end, on a GitHub-hosted Windows runner (elevated PowerShell 7).
.DESCRIPTION
    Copies the windows-amd64 deployment to "C:\Program Files\Throne", installs it with dist\windows\install-service.bat
    and checks the service: RUNNING, the C:\ProgramData\Throne ACL, Throne.exe --health, --check and --print-connection,
    a SOCKS profile through thronectl and the mixed inbound, thronectl restart through the SCM recovery actions,
    sc.exe stop and uninstall-service.bat; then a console-less foreground run that taskkill without /F stops. The
    fixtures (smoke_fixtures.py) run on the runner.

    It creates the service Throne, C:\ProgramData\Throne and C:\Program Files\Throne and removes them again, so it
    refuses to run when any of them exists, and outside GitHub Actions it runs only with -AllowHostChanges.
.PARAMETER ThroneDir
    The unpacked windows-amd64 deployment (Throne.exe, ThroneCore.exe).
.PARAMETER Thronectl
    thronectl.exe built for the runner.
.PARAMETER Fixtures
    script\ci\smoke_fixtures.py.
.PARAMETER RepoRoot
    The Throne checkout, for dist\windows\install-service.bat and uninstall-service.bat.
.PARAMETER WorkDir
    Scratch and log directory; the script works in its windows-service subdirectory, which a failed job uploads.
    Default: $env:E2E_TMP, else $env:RUNNER_TEMP\throne-e2e.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ThroneDir,
    [Parameter(Mandatory)][string]$Thronectl,
    [Parameter(Mandatory)][string]$Fixtures,
    [Parameter(Mandatory)][string]$RepoRoot,
    [string]$WorkDir,
    [switch]$AllowHostChanges
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$PSNativeCommandUseErrorActionPreference = $false

if (-not $WorkDir) {
    $WorkDir = if ($env:E2E_TMP) { $env:E2E_TMP }
    elseif ($env:RUNNER_TEMP) { Join-Path $env:RUNNER_TEMP 'throne-e2e' }
    else { Join-Path ([IO.Path]::GetTempPath()) 'throne-e2e' }
}
$ThroneDir, $Thronectl, $Fixtures, $RepoRoot, $WorkDir = foreach ($path in @($ThroneDir, $Thronectl, $Fixtures, $RepoRoot, $WorkDir)) {
    [IO.Path]::GetFullPath($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($path))
}

$ServiceName = 'Throne'
$DataDir = Join-Path $env:ProgramData 'Throne'
$ServiceLog = Join-Path $DataDir 'config\logs\throne.log'
$InstallDir = Join-Path $env:ProgramFiles 'Throne'
$ThroneExe = Join-Path $InstallDir 'Throne.exe'
$Sc = Join-Path $env:SystemRoot 'System32\sc.exe'
$Curl = Join-Path $env:SystemRoot 'System32\curl.exe'
$TaskKill = Join-Path $env:SystemRoot 'System32\taskkill.exe'
$InstallBat = Join-Path $RepoRoot 'dist\windows\install-service.bat'
$UninstallBat = Join-Path $RepoRoot 'dist\windows\uninstall-service.bat'
$Work = Join-Path $WorkDir 'windows-service'
$FixtureLog = Join-Path $Work 'fixtures.log'
$ForegroundData = Join-Path $Work 'foreground-data'
$ForegroundLog = Join-Path $ForegroundData 'config\logs\throne.log'

$HttpPort = 18380
$SocksPort = 11380
$ForegroundApiPort = 19395
$FixtureUrl = "http://fixture.test:$HttpPort/"
$ConnectPattern = "^CONNECT fixture\.test:$HttpPort -> 127\.0\.0\.1:$HttpPort ok$"
$SidSystem = 'S-1-5-18'
$SidAdmins = 'S-1-5-32-544'
$CleanEnd = '===== Throne session end (clean) ====='

$Ctx = @{
    StartTime         = Get-Date
    CurrentStep       = $null
    GroupOpen         = $false
    OwnInstallDir     = $false
    OwnDataDir        = $false
    OwnService        = $false
    FixtureProcess    = $null
    ForegroundProcess = $null
}

# ------------------------------------------------------------------------------------------------------------ helpers

function Format-Annotation([string]$Text) {
    $Text.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
}

function Write-Summary([string]$Line) {
    if (-not $env:GITHUB_STEP_SUMMARY) { return }
    try { Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Value $Line -Encoding utf8 } catch { Write-Host "warning: cannot write the step summary: $_" }
}

function Close-Step([string]$Result) {
    if ($Ctx.GroupOpen) {
        Write-Host '::endgroup::'
        $Ctx.GroupOpen = $false
    }
    if ($Ctx.CurrentStep) {
        Write-Summary "- W1 $($Ctx.CurrentStep): $Result"
        $Ctx.CurrentStep = $null
    }
}

function Step([string]$Name) {
    Close-Step 'passed'
    $Ctx.CurrentStep = $Name
    $Ctx.GroupOpen = $true
    Write-Host "::group::W1 $Name"
}

function Fail([string]$Message) {
    throw $Message
}

function Assert-Equal([string]$What, $Actual, $Expected) {
    if ([string]$Actual -cne [string]$Expected) { Fail "$What is '$Actual', want '$Expected'" }
}

function Assert-Contains([string]$What, [string]$Text, [string]$Needle, [switch]$IgnoreCase) {
    $comparison = if ($IgnoreCase) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
    if ($Text.IndexOf($Needle, $comparison) -lt 0) { Fail "$What has no line with '$Needle'" }
}

function Assert-NotContains([string]$What, [string]$Text, [string]$Needle) {
    if ($Text.Contains($Needle)) { Fail "$What has '$Needle'" }
}

function Assert-LogClean([string]$What, [string]$Text) {
    $hits = @($Text -split "`r?`n" | Where-Object { $_ -match 'HEADLESS-(DIALOG|WIDGET)|POLICY-UNBOUND' })
    if ($hits.Count -gt 0) { Fail ("{0} shows headless Throne reaching a GUI-only path:`n{1}" -f $What, ($hits -join "`n")) }
}

function Wait-Until {
    param(
        [Parameter(Mandatory, Position = 0)][double]$WaitSeconds,
        [Parameter(Mandatory, Position = 1)][string]$WaitWhat,
        [Parameter(Mandatory, Position = 2)][scriptblock]$WaitCondition,
        [int]$WaitIntervalMs = 500
    )
    Write-Host "waiting up to $WaitSeconds s for $WaitWhat"
    $waitClock = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        if ([bool](& $WaitCondition | Select-Object -Last 1)) {
            Write-Host ('{0}: after {1:N1} s' -f $WaitWhat, $waitClock.Elapsed.TotalSeconds)
            return
        }
        if ($waitClock.Elapsed.TotalSeconds -ge $WaitSeconds) { Fail "${WaitWhat}: not within $WaitSeconds s" }
        Start-Sleep -Milliseconds $WaitIntervalMs
    }
}

# Opened with FILE_SHARE_WRITE: Throne and the fixtures keep their logs open for writing.
function Read-Shared([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return '' }
    $stream = [IO.FileStream]::new($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    try {
        $reader = [IO.StreamReader]::new($stream, [Text.UTF8Encoding]::new($false), $true)
        return $reader.ReadToEnd()
    } finally {
        $stream.Dispose()
    }
}

# CommandLineToArgvW quoting, which Qt parses on Windows; Start-Process joins an argument array without quoting.
function Join-CommandLine([string[]]$Arguments) {
    $quoted = foreach ($argument in $Arguments) {
        if ($argument.Length -gt 0 -and $argument -notmatch '[\s"]') {
            $argument
        } else {
            $escaped = $argument -replace '(\\*)"', { ($_.Groups[1].Value * 2) + '\"' }
            $escaped = $escaped -replace '(\\+)$', { $_.Groups[1].Value * 2 }
            '"' + $escaped + '"'
        }
    }
    $quoted -join ' '
}

function Invoke-Native {
    param(
        [Parameter(Mandatory, Position = 0)][string]$FilePath,
        [Parameter(Position = 1)][string[]]$ArgumentList = @(),
        [switch]$Quiet
    )
    if (-not $Quiet) { Write-Host "> $(Split-Path -Leaf $FilePath) $($ArgumentList -join ' ')" }
    $errFile = Join-Path $Work 'native.stderr'
    Remove-Item -LiteralPath $errFile -Force -ErrorAction SilentlyContinue
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = & $FilePath @ArgumentList 2>$errFile
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    [pscustomobject]@{
        Code = $code
        Out  = (@($lines) | ForEach-Object { [string]$_ }) -join "`n"
        Err  = (Read-Shared $errFile).Trim()
    }
}

function Invoke-Ctl([string[]]$Arguments, [switch]$Quiet) {
    Invoke-Native $Thronectl $Arguments -Quiet:$Quiet
}

function ConvertFrom-JsonText([string]$Text, [string]$What) {
    try {
        return ($Text | ConvertFrom-Json)
    } catch {
        Fail "$What did not print JSON: $Text"
    }
}

function Invoke-CtlJson([string[]]$Arguments) {
    $r = Invoke-Ctl (@('--json') + $Arguments)
    if ($r.Code -ne 0) { Fail "thronectl $($Arguments -join ' ') exited with $($r.Code): $($r.Err)" }
    Write-Host $r.Out
    ConvertFrom-JsonText $r.Out "thronectl $($Arguments -join ' ')"
}

function Get-StatusOrNull {
    $r = Invoke-Ctl @('--json', 'status') -Quiet
    if ($r.Code -ne 0) { return $null }
    try { return ($r.Out | ConvertFrom-Json) } catch { return $null }
}

# A dotted path into ConvertFrom-Json output, list indexes as numbers; $null when any part is missing.
function Get-JsonField($Object, [string]$Path) {
    $value = $Object
    foreach ($part in $Path.Split('.')) {
        if ($null -eq $value) { return $null }
        if ($value -is [Collections.IList] -and $part -match '^\d+$') {
            if ([int]$part -ge $value.Count) { return $null }
            $value = $value[[int]$part]
            continue
        }
        if ($value -isnot [Management.Automation.PSCustomObject]) { return $null }
        $property = $value.PSObject.Properties[$part]
        if ($null -eq $property) { return $null }
        $value = $property.Value
    }
    return , $value
}

# Throne.exe is a GUI-subsystem program: only Start-Process -Wait gets its exit code, and its output goes to files.
function Invoke-ThroneClient {
    param(
        [Parameter(Mandatory, Position = 0)][string]$Name,
        [Parameter(Mandatory, Position = 1)][string[]]$Arguments,
        [switch]$Quiet
    )
    $line = Join-CommandLine $Arguments
    if (-not $Quiet) { Write-Host "> Throne.exe $line" }
    $outFile = Join-Path $Work "client-$Name.out"
    $errFile = Join-Path $Work "client-$Name.err"
    $process = Start-Process -FilePath $ThroneExe -ArgumentList $line -Wait -PassThru -NoNewWindow `
        -RedirectStandardOutput $outFile -RedirectStandardError $errFile
    if ($null -eq $process.ExitCode) { Fail "Throne.exe $line left no exit code" }
    $result = [pscustomobject]@{
        Code = [int]$process.ExitCode
        Out  = (Read-Shared $outFile).Trim()
        Err  = (Read-Shared $errFile).Trim()
    }
    if (-not $Quiet) {
        Write-Host "exit status $($result.Code)"
        if ($result.Out) { Write-Host $result.Out }
        if ($result.Err) { Write-Host "stderr: $($result.Err)" }
    }
    $result
}

function Get-Health([string]$Name, [string]$Dir, [switch]$Quiet) {
    $r = Invoke-ThroneClient $Name @('--health', '--data-dir', $Dir) -Quiet:$Quiet
    $json = $null
    if ($r.Out) {
        try { $json = $r.Out | ConvertFrom-Json } catch { $json = $null }
    }
    [pscustomobject]@{ Code = $r.Code; Json = $json; Out = $r.Out }
}

function Assert-Healthy([string]$What, $Health) {
    Assert-Equal "$What --health exit status" $Health.Code 0
    if ($null -eq $Health.Json) { Fail "$What --health printed no JSON: $($Health.Out)" }
    Assert-Equal "$What ok" (Get-JsonField $Health.Json 'ok') $true
    Assert-Equal "$What ready" (Get-JsonField $Health.Json 'ready') $true
    Assert-Equal "$What headless_dialogs" (Get-JsonField $Health.Json 'headless_dialogs') 0
}

# --print-connection's POSIX export lines; single-quoted values use the '\'' escape.
function ConvertFrom-ExportLines([string]$Text) {
    $vars = @{}
    foreach ($line in ($Text -split "`r?`n")) {
        if ($line -notmatch '^export ([A-Za-z_][A-Za-z0-9_]*)=(.*)$') { continue }
        $name = $Matches[1]
        $value = $Matches[2]
        if ($value.StartsWith("'")) {
            if ($value.Length -lt 2 -or -not $value.EndsWith("'")) { Fail "cannot read the quoted value of: $line" }
            $value = $value.Substring(1, $value.Length - 2).Replace("'\''", "'")
        }
        $vars[$name] = $value
    }
    $vars
}

function Get-ThroneService {
    Get-CimInstance -ClassName Win32_Service -Filter "Name='$ServiceName'"
}

function Get-RequiredService([string]$When) {
    $service = Get-ThroneService
    if ($null -eq $service) { Fail "the $ServiceName service is gone $When" }
    $service
}

function Get-OwnProcesses {
    @(Get-Process -Name Throne, ThroneCore -ErrorAction SilentlyContinue | Where-Object {
            $path = $_.Path
            $path -and $path.StartsWith($InstallDir + '\', [StringComparison]::OrdinalIgnoreCase)
        })
}

function Get-ConnectCount {
    @((Read-Shared $FixtureLog) -split "`r?`n" | Where-Object { $_ -match $ConnectPattern }).Count
}

function Get-InboundProxy($Status) {
    $address = [string](Get-JsonField $Status 'inbound.address')
    $port = Get-JsonField $Status 'inbound.port'
    if (-not $port) { Fail 'the status has no inbound.port' }
    if ($address -eq '' -or $address -eq '0.0.0.0') { $address = '127.0.0.1' }
    elseif ($address -eq '::') { $address = '[::1]' }
    elseif ($address.Contains(':')) { $address = "[$address]" }
    "${address}:$port"
}

function Invoke-ProxiedRequest([string]$Proxy) {
    $r = Invoke-Native $Curl @('-sS', '--max-time', '20', '-x', "socks5h://$Proxy", '-o', 'NUL', '-w', '%{http_code}', $FixtureUrl)
    if ($r.Code -ne 0) { Fail "curl through socks5h://$Proxy failed with $($r.Code): $($r.Err)" }
    $r.Out.Trim()
}

function Assert-ProxiedThroughThrone([string]$Proxy) {
    $before = Get-ConnectCount
    Assert-Equal "HTTP status through Throne's mixed inbound $Proxy" (Invoke-ProxiedRequest $Proxy) '204'
    if ((Get-ConnectCount) -le $before) { Fail 'the request never reached the SOCKS5 fixture, so Throne did not proxy it' }
}

function Get-AclSnapshot([string]$Path) {
    $acl = Get-Acl -LiteralPath $Path
    $sidType = [Security.Principal.SecurityIdentifier]
    [pscustomobject]@{
        Owner     = $acl.GetOwner($sidType).Value
        Protected = $acl.AreAccessRulesProtected
        Rules     = @($acl.GetAccessRules($true, $true, $sidType))
        Sddl      = $acl.Sddl
    }
}

function Assert-AdminOnlyEntry([string]$Path) {
    $snapshot = Get-AclSnapshot $Path
    Write-Host "${Path}: $($snapshot.Sddl)"
    if ($snapshot.Owner -notin @($SidSystem, $SidAdmins)) { Fail "$Path is owned by $($snapshot.Owner), not SYSTEM or Administrators" }
    foreach ($rule in $snapshot.Rules) {
        $sid = $rule.IdentityReference.Value
        if ($rule.AccessControlType -eq 'Allow' -and $sid -notin @($SidSystem, $SidAdmins)) {
            Fail "$Path grants $sid access ($($rule.FileSystemRights)); only SYSTEM and Administrators may have any"
        }
    }
    $snapshot
}

function Remove-Tree([string]$Path) {
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while (Test-Path -LiteralPath $Path) {
        try {
            Remove-Item -LiteralPath $Path -Recurse -Force
        } catch {
            if ($clock.Elapsed.TotalSeconds -ge 15) { throw }
            Start-Sleep -Milliseconds 500
        }
    }
}

# ------------------------------------------------------------------------------------------------------------ diagnostics

function Show-File([string]$Path, [int]$Tail = 0) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return }
    Write-Host "::group::$Path"
    try {
        $lines = @((Read-Shared $Path) -split "`r?`n")
        if ($Tail -gt 0 -and $lines.Count -gt $Tail) {
            Write-Host "(the last $Tail of $($lines.Count) lines)"
            $lines = $lines[($lines.Count - $Tail)..($lines.Count - 1)]
        }
        foreach ($line in $lines) { Write-Host $line }
    } catch {
        Write-Host "cannot read it: $($_.Exception.Message)"
    }
    Write-Host '::endgroup::'
}

function Show-Group([string]$Title, [scriptblock]$Action) {
    Write-Host "::group::$Title"
    try {
        & $Action | Out-String -Width 400 | Write-Host
    } catch {
        Write-Host "failed: $($_.Exception.Message)"
    }
    Write-Host '::endgroup::'
}

function Show-State {
    Show-Group "sc.exe queryex / qc $ServiceName" {
        (Invoke-Native $Sc @('queryex', $ServiceName) -Quiet).Out
        (Invoke-Native $Sc @('qc', $ServiceName) -Quiet).Out
    }
    Show-Group 'Throne processes' {
        Get-Process -Name Throne, ThroneCore -ErrorAction SilentlyContinue | Format-Table Id, ProcessName, StartTime, Path -AutoSize
    }
    Show-Group 'Service Control Manager and application error events' {
        foreach ($filter in @(
                @{ LogName = 'System'; ProviderName = 'Service Control Manager'; StartTime = $Ctx.StartTime },
                @{ LogName = 'Application'; ProviderName = 'Application Error'; StartTime = $Ctx.StartTime }
            )) {
            $events = @()
            try { $events = @(Get-WinEvent -FilterHashtable $filter -ErrorAction Stop) } catch { $events = @() }
            $events | Where-Object { $_.Message -match 'Throne' } | Sort-Object TimeCreated | ForEach-Object {
                '{0:HH:mm:ss} {1} {2}' -f $_.TimeCreated, $_.Id, ($_.Message -replace '\s+', ' ')
            }
        }
    }
}

function Save-ServiceData {
    $target = Join-Path $Work 'service-data'
    foreach ($part in @('config\logs', 'config\crashes', 'config\headless-state.json')) {
        $source = Join-Path $DataDir $part
        if (-not (Test-Path -LiteralPath $source)) { continue }
        $destination = Join-Path $target $part
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force
    }
}

function Show-Logs {
    $saved = Join-Path $Work 'service-data\config'
    Show-File (Join-Path $saved 'logs\throne.log') 300
    foreach ($file in @(Get-ChildItem -Path (Join-Path $saved 'logs') -Filter 'crashed-*.log' -ErrorAction SilentlyContinue)) {
        Show-File $file.FullName 200
    }
    foreach ($file in @(Get-ChildItem -Path (Join-Path $saved 'crashes') -Filter '*.txt' -ErrorAction SilentlyContinue)) {
        Show-File $file.FullName
    }
    Show-File (Join-Path $saved 'headless-state.json')
    Show-File $FixtureLog
    Show-File (Join-Path $Work 'fixtures.stderr')
    foreach ($file in @(Get-ChildItem -Path $Work -Filter 'client-*' -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime)) {
        if ($file.Length -gt 0) { Show-File $file.FullName 100 }
    }
    Show-File (Join-Path $Work 'foreground.stdout') 100
    Show-File (Join-Path $Work 'foreground.stderr') 200
    Show-File $ForegroundLog 300
}

function Invoke-Safely([string]$What, [scriptblock]$Action) {
    try {
        & $Action | Out-Host
    } catch {
        Write-Host "warning: ${What}: $($_.Exception.Message)"
    }
}

function Invoke-Cleanup([bool]$Failed) {
    if ($Failed) { Invoke-Safely 'showing the service state' { Show-State } }
    Invoke-Safely 'stopping the foreground Throne' {
        $process = $Ctx.ForegroundProcess
        if ($null -ne $process -and -not $process.HasExited) { $process.Kill($true) }
    }
    Invoke-Safely 'uninstalling the service' {
        if ($Ctx.OwnService -and $null -ne (Get-ThroneService)) {
            Write-Host 'cleanup: uninstall-service.bat'
            $r = Invoke-Native $UninstallBat
            Write-Host $r.Out
            if ($null -ne (Get-ThroneService)) { $null = Invoke-Native $Sc @('delete', $ServiceName) }
        }
    }
    Invoke-Safely 'stopping leftover Throne processes' {
        $left = Get-OwnProcesses
        if ($left.Count -gt 0) {
            Write-Host "cleanup: killing $(($left | ForEach-Object { "$($_.ProcessName) $($_.Id)" }) -join ', ')"
            $left | Stop-Process -Force -ErrorAction SilentlyContinue
            $left | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
        }
    }
    Invoke-Safely 'stopping the fixtures' {
        $process = $Ctx.FixtureProcess
        if ($null -ne $process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    }
    Invoke-Safely 'saving the service logs' { if ($Ctx.OwnDataDir) { Save-ServiceData } }
    if ($Failed) { Invoke-Safely 'showing the logs' { Show-Logs } }
    Invoke-Safely "removing $InstallDir" { if ($Ctx.OwnInstallDir) { Remove-Tree $InstallDir } }
    Invoke-Safely "removing $DataDir" { if ($Ctx.OwnDataDir) { Remove-Tree $DataDir } }
}

# ------------------------------------------------------------------------------------------------------------ scenario

function Invoke-W1 {
    Step 'preconditions'
    if (Test-Path -LiteralPath $Work) { Remove-Item -LiteralPath $Work -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $Work | Out-Null
    $principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { Fail 'run this from an elevated PowerShell 7' }
    if ($env:GITHUB_ACTIONS -ne 'true' -and -not $AllowHostChanges) {
        Fail "this installs the service $ServiceName and writes $DataDir and $InstallDir; outside GitHub Actions pass -AllowHostChanges"
    }
    foreach ($file in @((Join-Path $ThroneDir 'Throne.exe'), (Join-Path $ThroneDir 'ThroneCore.exe'), $Thronectl, $Fixtures,
            $InstallBat, $UninstallBat, $Sc, $Curl, $TaskKill)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { Fail "missing $file" }
    }
    if ($null -ne (Get-ThroneService)) { Fail "a service named $ServiceName already exists; this test installs its own" }
    if (Test-Path -LiteralPath $DataDir) { Fail "$DataDir already exists; this test checks the one the service creates" }
    if (Test-Path -LiteralPath $InstallDir) { Fail "$InstallDir already exists; this test installs there" }
    if (Get-Process -Name Throne, ThroneCore -ErrorAction SilentlyContinue) { Fail 'Throne or ThroneCore is already running' }
    # thronectl and the client modes must find everything through --data-dir and --print-connection.
    Get-ChildItem Env: | Where-Object { $_.Name -like 'THRONE_*' } | ForEach-Object { Remove-Item -LiteralPath "Env:$($_.Name)" }
    $python = Get-Command python -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $python) { Fail 'python is not on PATH' }
    $version = Invoke-Ctl @('version')
    Assert-Equal 'thronectl version exit status' $version.Code 0
    Write-Host $version.Out

    Step 'fixtures (HTTP 204 and SOCKS5 on the runner)'
    $ready = Join-Path $Work 'fixtures.ready'
    $fixtureArgs = Join-CommandLine @('-I', $Fixtures, '--listen', '127.0.0.1', '--http-port', "$HttpPort", '--socks-port', "$SocksPort",
        '--log', $FixtureLog, '--ready-file', $ready)
    Write-Host "> python $fixtureArgs"
    $fixtureProcess = Start-Process -FilePath $python.Source -ArgumentList $fixtureArgs -NoNewWindow -PassThru `
        -RedirectStandardOutput (Join-Path $Work 'fixtures.stdout') -RedirectStandardError (Join-Path $Work 'fixtures.stderr')
    $null = $fixtureProcess.Handle
    $Ctx.FixtureProcess = $fixtureProcess
    Wait-Until 15 'the fixtures listening' {
        if ($Ctx.FixtureProcess.HasExited) {
            Fail "the fixtures exited with $($Ctx.FixtureProcess.ExitCode): $(Read-Shared (Join-Path $Work 'fixtures.stderr'))"
        }
        (Read-Shared $ready).Trim().Length -gt 0
    }
    $before = Get-ConnectCount
    Assert-Equal 'fixture self-test HTTP status' (Invoke-ProxiedRequest "127.0.0.1:$SocksPort") '204'
    if ((Get-ConnectCount) -le $before) { Fail 'the SOCKS5 fixture did not log its own CONNECT' }

    Step 'install-service.bat'
    New-Item -ItemType Directory -Path $InstallDir | Out-Null
    $Ctx.OwnInstallDir = $true
    Get-ChildItem -LiteralPath $ThroneDir -Force | Copy-Item -Destination $InstallDir -Recurse -Force
    $r = Invoke-ThroneClient 'version' @('--headless', '--version')
    Assert-Equal 'Throne.exe --headless --version exit status' $r.Code 0
    if (-not $r.Out.StartsWith('Throne ')) { Fail "Throne.exe --headless --version printed '$($r.Out)'" }
    $r = Invoke-ThroneClient 'service-outside-scm' @('--headless', '--service')
    Assert-Equal 'exit status of --service outside the SCM' $r.Code 64
    Assert-Contains '--service outside the SCM stderr' $r.Err 'was not started by the Windows Service Control Manager'
    $h = Get-Health 'health-before-install' $DataDir
    Assert-Equal '--health exit status with no service' $h.Code 2
    if (Test-Path -LiteralPath $DataDir) { Fail "--health created $DataDir; the client modes must create nothing" }

    $Ctx.OwnDataDir = $true
    $Ctx.OwnService = $true
    $r = Invoke-Native $InstallBat @($ThroneExe)
    Write-Host $r.Out
    if ($r.Err) { Write-Host "stderr: $($r.Err)" }
    Assert-Equal 'install-service.bat exit status' $r.Code 0
    $service = Get-RequiredService 'after install-service.bat'
    if ($service.PathName -ine ('"{0}" --headless --service' -f $ThroneExe)) { Fail "the service runs '$($service.PathName)'" }
    Assert-Equal 'service account' $service.StartName 'LocalSystem'
    Assert-Equal 'service start mode' $service.StartMode 'Auto'
    $key = "HKLM:\SYSTEM\CurrentControlSet\Services\$ServiceName"
    Assert-Equal 'DelayedAutostart' (Get-ItemPropertyValue -LiteralPath $key -Name DelayedAutostart) 1
    Assert-Equal 'FailureActionsOnNonCrashFailures (sc failureflag)' (Get-ItemPropertyValue -LiteralPath $key -Name FailureActionsOnNonCrashFailures) 1
    $failure = Invoke-Native $Sc @('qfailure', $ServiceName)
    Write-Host $failure.Out
    if ($failure.Out -notmatch 'RESET_PERIOD[^\r\n]*:\s*86400') { Fail 'sc qfailure shows no 86400 s reset period' }
    if ($failure.Out -notmatch 'RESTART[^\r\n]*3000 milliseconds') { Fail 'sc qfailure shows no restart after 3000 ms as the first action' }

    Step 'service RUNNING'
    Wait-Until 90 'the Throne service RUNNING' {
        $s = Get-RequiredService 'while it starts'
        if ($s.State -eq 'Stopped') {
            Fail ('the service stopped while starting: exit code {0}, service-specific {1}' -f $s.ExitCode, $s.ServiceSpecificExitCode)
        }
        $s.State -eq 'Running'
    }
    $servicePid = [int](Get-RequiredService 'once running').ProcessId
    $serviceProcess = Get-Process -Id $servicePid
    if ($serviceProcess.Path -ine $ThroneExe) { Fail "the service PID $servicePid runs '$($serviceProcess.Path)', not $ThroneExe" }
    Write-Host "service PID $servicePid"

    Step "$DataDir access: SYSTEM and Administrators only"
    $snapshot = Assert-AdminOnlyEntry $DataDir
    if (-not $snapshot.Protected) { Fail "$DataDir inherits from its parent; its access list must be protected" }
    $sids = @($snapshot.Rules | ForEach-Object { $_.IdentityReference.Value } | Sort-Object -Unique)
    Assert-Equal "accounts in the $DataDir access list" ($sids -join ',') "$SidSystem,$SidAdmins"
    foreach ($rule in $snapshot.Rules) {
        $full = [Security.AccessControl.FileSystemRights]::FullControl
        if ($rule.AccessControlType -ne 'Allow' -or ($rule.FileSystemRights -band $full) -ne $full -or $rule.IsInherited) {
            Fail "$DataDir has the rule $($rule.AccessControlType) $($rule.FileSystemRights) for $($rule.IdentityReference) (inherited: $($rule.IsInherited))"
        }
    }
    foreach ($entry in @('config', 'config\api.key', 'config\throne.db')) {
        $path = Join-Path $DataDir $entry
        if (-not (Test-Path -LiteralPath $path)) { Fail "the running service has no $path" }
        $null = Assert-AdminOnlyEntry $path
    }

    Step 'Throne.exe --health and --check beside the service'
    Wait-Until 30 'Throne.exe --health exit status 0' { (Get-Health 'health-service' $DataDir -Quiet).Code -eq 0 }
    $h = Get-Health 'health-service' $DataDir
    Assert-Healthy 'the service' $h
    Assert-Equal 'previous_crash' (Get-JsonField $h.Json 'previous_crash') $false
    Assert-Equal 'api' (Get-JsonField $h.Json 'api') 'listening'
    # Assert-Equal compares as strings, where a missing field and JSON null are both ''.
    $degraded = $h.Json.PSObject.Properties['degraded']
    if ($null -eq $degraded) { Fail 'the service --health has no degraded field' }
    if ($null -ne $degraded.Value) { Fail "the service degraded is '$($degraded.Value)', want null" }
    Assert-Equal 'state' (Get-JsonField $h.Json 'state') 'idle'
    $r = Invoke-ThroneClient 'check-service' @('--check', '--data-dir', $DataDir)
    Assert-Equal 'Throne.exe --check exit status beside the service' $r.Code 0
    $outLines = @($r.Out -split "`r?`n")
    $failedItems = @($outLines | Where-Object { $_.StartsWith('[FAIL]') })
    if ($failedItems.Count -gt 0) { Fail ("--check failed items:`n{0}" -f ($failedItems -join "`n")) }
    $instance = @($outLines | Where-Object { $_.StartsWith('[INFO] instance: running: ') })
    if ($instance.Count -ne 1 -or -not $instance[0].Contains("PID $servicePid")) { Fail "--check does not see the service (PID $servicePid) as the running instance" }

    Step 'Throne.exe --print-connection and thronectl status'
    $r = Invoke-ThroneClient 'print-connection-service' @('--print-connection', '--data-dir', $DataDir)
    Assert-Equal 'Throne.exe --print-connection exit status' $r.Code 0
    $connection = ConvertFrom-ExportLines $r.Out
    Assert-Equal 'THRONE_ADDR' $connection['THRONE_ADDR'] '127.0.0.1:9095'
    $keyFile = $connection['THRONE_KEY_FILE']
    if (-not $keyFile) { Fail '--print-connection printed no THRONE_KEY_FILE' }
    $expectedKey = Join-Path $DataDir 'config\api.key'
    if ([IO.Path]::GetFullPath($keyFile) -ine $expectedKey) { Fail "THRONE_KEY_FILE is $keyFile, want $expectedKey" }
    $env:THRONE_ADDR = $connection['THRONE_ADDR']
    $env:THRONE_KEY_FILE = $keyFile
    $status = Invoke-CtlJson @('status')
    Assert-Equal 'capabilities.headless' (Get-JsonField $status 'capabilities.headless') $true
    Assert-Equal 'state' (Get-JsonField $status 'state') 'idle'
    Assert-Equal 'core' (Get-JsonField $status 'core') $true

    Step 'a SOCKS profile through the mixed inbound'
    $linkFile = Join-Path $Work 'profile.txt'
    [IO.File]::WriteAllText($linkFile, "socks5://127.0.0.1:$SocksPort#e2e-windows`n")
    $imported = Invoke-CtlJson @('import', $linkFile)
    Assert-Equal 'imported profiles' (Get-JsonField $imported 'imported') 1
    $profileId = Get-JsonField $imported 'profile_ids.0'
    if ($null -eq $profileId) { Fail 'the import returned no profile id' }
    $r = Invoke-Ctl @('--timeout', '90s', 'connect', "$profileId")
    Write-Host $r.Out
    if ($r.Code -ne 0) { Fail "thronectl connect $profileId exited with $($r.Code): $($r.Err)" }
    $status = Invoke-CtlJson @('status')
    Assert-Equal 'state' (Get-JsonField $status 'state') 'running'
    Assert-Equal 'profile.id' (Get-JsonField $status 'profile.id') $profileId
    Assert-Equal 'tun' (Get-JsonField $status 'tun') $false
    Assert-ProxiedThroughThrone (Get-InboundProxy $status)

    Step 'thronectl restart: exit 75, the SCM recovery starts it again'
    $restart = Invoke-CtlJson @('restart')
    Assert-Equal 'restarting' (Get-JsonField $restart 'restarting') $true
    Assert-Equal 'resume' (Get-JsonField $restart 'resume') $true
    $watch = @{ OldPid = $servicePid; Seen = ''; Stopped = $null; OldLog = $null }
    # The old log survives only between the old process's exit and the recovery start 3 s later, which truncates it.
    Wait-Until 120 'the SCM recovery to run Throne again (RUNNING with a new PID)' {
        $s = Get-RequiredService 'during the restart'
        $seen = '{0}, PID {1}, exit code {2}, service-specific {3}' -f $s.State, $s.ProcessId, $s.ExitCode, $s.ServiceSpecificExitCode
        if ($seen -ne $watch.Seen) {
            Write-Host "service: $seen"
            $watch.Seen = $seen
        }
        if ($s.State -eq 'Stopped' -and $null -eq $watch.Stopped) { $watch.Stopped = $s }
        if ($null -eq $watch.OldLog -and $null -eq (Get-Process -Id $watch.OldPid -ErrorAction SilentlyContinue)) {
            $watch.OldLog = Read-Shared $ServiceLog
        }
        $s.State -eq 'Running' -and $s.ProcessId -ne 0 -and $s.ProcessId -ne $watch.OldPid
    } -WaitIntervalMs 200
    if ($null -ne $watch.Stopped) {
        Assert-Equal 'WIN32_EXIT_CODE after thronectl restart' $watch.Stopped.ExitCode 1066
        Assert-Equal 'SERVICE_EXIT_CODE after thronectl restart' $watch.Stopped.ServiceSpecificExitCode 75
    } else {
        Write-Host 'note: the STOPPED state between the exit and the recovery start was not sampled'
    }
    if ($null -ne $watch.OldLog -and $watch.OldLog -match "(?m)^\s*pid\s*:\s*$servicePid\s*$") {
        Assert-Contains 'the log of the run before the restart' $watch.OldLog 'EXIT 75: restart requested'
        Assert-Contains 'the log of the run before the restart' $watch.OldLog $CleanEnd
        Assert-LogClean 'the log of the run before the restart' $watch.OldLog
    } else {
        Write-Host 'note: the log of the run before the restart was already replaced when it was read'
    }
    $restartPid = [int](Get-RequiredService 'after the restart').ProcessId
    Write-Host "service PID $restartPid after the restart"
    Wait-Until 90 'Throne.exe --health exit status 0 after the restart' { (Get-Health 'health-after-restart' $DataDir -Quiet).Code -eq 0 }
    $h = Get-Health 'health-after-restart' $DataDir
    Assert-Healthy 'the restarted service' $h
    Assert-Equal 'previous_crash after a restart' (Get-JsonField $h.Json 'previous_crash') $false
    Assert-Equal 'previous_exit.code after thronectl restart' (Get-JsonField $h.Json 'previous_exit.code') 75
    Assert-Equal 'previous_exit.reason after thronectl restart' (Get-JsonField $h.Json 'previous_exit.reason') 'restart requested'
    Wait-Until 90 'the profile resumed after the restart' {
        $s = Get-StatusOrNull
        $null -ne $s -and (Get-JsonField $s 'state') -eq 'running'
    }
    $status = Invoke-CtlJson @('status')
    Assert-Equal 'resumed profile.id' (Get-JsonField $status 'profile.id') $profileId
    Assert-ProxiedThroughThrone (Get-InboundProxy $status)
    $r = Invoke-Ctl @('--timeout', '60s', 'disconnect')
    Write-Host $r.Out
    if ($r.Code -ne 0) { Fail "thronectl disconnect exited with $($r.Code): $($r.Err)" }
    Assert-Equal 'state after disconnect' (Get-JsonField (Invoke-CtlJson @('status')) 'state') 'idle'

    Step 'sc.exe stop: STOPPED with exit code 0, no recovery'
    $r = Invoke-Native $Sc @('stop', $ServiceName)
    Write-Host $r.Out
    Assert-Equal 'sc.exe stop exit status' $r.Code 0
    Wait-Until 60 'the service STOPPED' { (Get-RequiredService 'while it stops').State -eq 'Stopped' }
    $service = Get-RequiredService 'after sc.exe stop'
    Assert-Equal 'WIN32_EXIT_CODE after sc.exe stop' $service.ExitCode 0
    Assert-Equal 'SERVICE_EXIT_CODE after sc.exe stop' $service.ServiceSpecificExitCode 0
    Write-Host (Invoke-Native $Sc @('query', $ServiceName)).Out
    # Longer than the first recovery delay (3 s): a requested stop must not count as a failure.
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while ($clock.Elapsed.TotalSeconds -lt 6) {
        $s = Get-RequiredService 'after sc.exe stop'
        if ($s.State -ne 'Stopped') { Fail "the service is $($s.State) again after sc.exe stop: the recovery actions took the stop for a failure" }
        Start-Sleep -Milliseconds 500
    }
    Wait-Until 30 "the service process (PID $restartPid) exiting" { $null -eq (Get-Process -Id $restartPid -ErrorAction SilentlyContinue) }
    Wait-Until 15 'no ThroneCore.exe left' { $null -eq (Get-Process -Name ThroneCore -ErrorAction SilentlyContinue) }
    $log = Read-Shared $ServiceLog
    Assert-LogClean $ServiceLog $log
    if ($log -notmatch "(?m)^\s*pid\s*:\s*$restartPid\s*$") { Fail "$ServiceLog is not the log of PID $restartPid" }
    Assert-Contains $ServiceLog $log 'started by --headless'
    Assert-Contains $ServiceLog $log "data dir $DataDir (ProgramData default)" -IgnoreCase
    Assert-Contains $ServiceLog $log 'restart mode exit (Windows service)'
    if ($log -notmatch '\[Service\] Running under the Service Control Manager as "Throne"; preshutdown timeout (raised to 45 s|\d+ s)\.') {
        Fail "$ServiceLog has no '[Service] Running under the Service Control Manager' line with a preshutdown timeout of at least 45 s"
    }
    Assert-Contains $ServiceLog $log "[Resume] Resuming profile $profileId"
    Assert-Contains $ServiceLog $log 'Stop request: the Service Control Manager (stop).'
    Assert-Contains $ServiceLog $log 'EXIT 0: service stop'
    Assert-Contains $ServiceLog $log $CleanEnd
    Assert-NotContains $ServiceLog $log 'previous session did not shut down cleanly'
    if (Test-Path -LiteralPath (Join-Path $DataDir 'config\logs\running.marker')) { Fail 'running.marker was left behind' }

    Step 'uninstall-service.bat'
    $r = Invoke-Native $UninstallBat
    Write-Host $r.Out
    Assert-Equal 'uninstall-service.bat exit status' $r.Code 0
    Wait-Until 30 'the Throne service deleted' { $null -eq (Get-ThroneService) }
    if (-not (Test-Path -LiteralPath (Join-Path $DataDir 'config\throne.db'))) { Fail "uninstall-service.bat removed the data in $DataDir; it must stay" }
    $h = Get-Health 'health-after-uninstall' $DataDir
    Assert-Equal '--health exit status after the uninstall' $h.Code 2

    Step 'console-less foreground run stopped by taskkill without /F'
    $foregroundArgs = Join-CommandLine @('--headless', '--data-dir', $ForegroundData, '--api-listen', "127.0.0.1:$ForegroundApiPort")
    Write-Host "> Throne.exe $foregroundArgs"
    $foreground = Start-Process -FilePath $ThroneExe -ArgumentList $foregroundArgs -NoNewWindow -PassThru `
        -RedirectStandardOutput (Join-Path $Work 'foreground.stdout') -RedirectStandardError (Join-Path $Work 'foreground.stderr')
    $null = $foreground.Handle
    $Ctx.ForegroundProcess = $foreground
    Wait-Until 90 'the foreground Throne healthy' {
        if ($Ctx.ForegroundProcess.HasExited) { Fail "the foreground Throne exited with $($Ctx.ForegroundProcess.ExitCode) before it was healthy" }
        (Get-Health 'health-foreground' $ForegroundData -Quiet).Code -eq 0
    }
    Assert-Healthy 'the foreground Throne' (Get-Health 'health-foreground' $ForegroundData)
    $r = Invoke-ThroneClient 'print-connection-foreground' @('--print-connection', '--data-dir', $ForegroundData)
    Assert-Equal 'foreground --print-connection exit status' $r.Code 0
    $connection = ConvertFrom-ExportLines $r.Out
    Assert-Equal 'foreground THRONE_ADDR' $connection['THRONE_ADDR'] "127.0.0.1:$ForegroundApiPort"
    $r = Invoke-Native $TaskKill @('/PID', "$($foreground.Id)")
    Write-Host $r.Out
    if ($r.Err) { Write-Host "stderr: $($r.Err)" }
    Assert-Equal 'taskkill /PID exit status' $r.Code 0
    if (-not $foreground.WaitForExit(60000)) { Fail "the foreground Throne (PID $($foreground.Id)) did not exit within 60 s of taskkill" }
    Assert-Equal 'foreground Throne exit status after taskkill without /F' $foreground.ExitCode 0
    Wait-Until 15 'no ThroneCore.exe left' { $null -eq (Get-Process -Name ThroneCore -ErrorAction SilentlyContinue) }
    $log = Read-Shared $ForegroundLog
    Assert-LogClean $ForegroundLog $log
    Assert-LogClean 'the foreground stderr' (Read-Shared (Join-Path $Work 'foreground.stderr'))
    Assert-Contains $ForegroundLog $log "data dir $ForegroundData (--data-dir)" -IgnoreCase
    Assert-Contains $ForegroundLog $log 'restart mode exec (not a Windows service; a new Throne process starts)'
    Assert-Contains $ForegroundLog $log 'Stop request: WM_CLOSE (taskkill without /F).'
    Assert-Contains $ForegroundLog $log 'EXIT 0: stop request (Ctrl+C, closing the console, signing out or taskkill)'
    Assert-Contains $ForegroundLog $log $CleanEnd
    if (Test-Path -LiteralPath (Join-Path $ForegroundData 'config\logs\running.marker')) { Fail 'the foreground run left running.marker behind' }
}

# ------------------------------------------------------------------------------------------------------------ main

$failed = $true
try {
    Invoke-W1
    Close-Step 'passed'
    $failed = $false
} catch {
    $stepName = $Ctx.CurrentStep
    Close-Step 'FAILED'
    $where = if ($stepName) { " ($stepName)" } else { '' }
    Write-Host ('::error::{0}' -f (Format-Annotation "W1 Windows service${where}: $($_.Exception.Message)"))
    Write-Host $_.ScriptStackTrace
} finally {
    Invoke-Cleanup $failed
}
if ($failed) {
    Write-Summary '- W1 Windows service: FAILED'
    Write-Host 'W1 failed; logs are above and in' $Work
    exit 1
}
Write-Summary '- W1 Windows service: passed'
Write-Host 'W1 passed'
exit 0
