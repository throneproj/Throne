#Requires -Version 5.1
# Embedded in Throne's resources and passed in-memory to Windows PowerShell.
[CmdletBinding()]
param(
    [ValidateSet('Probe', 'Run', 'Stop')][string]$Action = 'Probe',
    [int]$ParentProcessId = 0,
    [string]$StopFile = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)

function Send-Event($Value) {
    [Console]::WriteLine(('THRONE_HOTSPOT:' + ($Value | ConvertTo-Json -Compress)))
    [Console]::Out.Flush()
}

function Wait-Operation($Operation) {
    $bridge = [System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
        $_.Name -eq 'AsTask' -and $_.IsGenericMethod -and $_.GetGenericArguments().Count -eq 1 -and
        $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'
    } | Select-Object -First 1
    $task = $bridge.MakeGenericMethod([Windows.Networking.NetworkOperators.NetworkOperatorTetheringOperationResult]).Invoke($null, @($Operation))
    # Do not abandon an operation that can still turn the hotspot on. This runs
    # in a child process; the GUI remains responsive and can queue a stop.
    $result = $task.GetAwaiter().GetResult()
    if ($result.Status.ToString() -ne 'Success') {
        throw ('{0}: {1}' -f $result.Status, $result.AdditionalErrorMessage)
    }
}

function Test-Forwarding {
    $rows = @(Get-NetIPInterface -AddressFamily IPv4 -ErrorAction Stop)
    $tunRows = @($rows | Where-Object { $_.InterfaceIndex -eq $tunIndex })
    $egressRows = @($rows | Where-Object { $egressIndexes -contains $_.InterfaceIndex -and $_.ConnectionState.ToString() -eq 'Connected' })
    return $tunRows.Count -eq 1 -and [int]$tunRows[0].Forwarding -eq 1 -and
        $egressRows.Count -gt 0 -and @($egressRows | Where-Object { [int]$_.Forwarding -ne 0 }).Count -eq 0
}

function Assert-NoLegacySharing {
    $sharing = New-Object -ComObject HNetCfg.HNetShare
    foreach ($connection in @($sharing.EnumEveryConnection())) {
        if ($sharing.INetSharingConfigurationForINetConnection($connection).SharingEnabled) {
            throw 'Turn off existing Internet Connection Sharing before starting the hotspot.'
        }
    }
}

function New-StopManager($AdapterId) {
    $all = @([Windows.Networking.Connectivity.NetworkInformation]::GetConnectionProfiles())
    $matching = @($all | Where-Object { $null -ne $_.NetworkAdapter -and $_.NetworkAdapter.NetworkAdapterId -eq [guid]$AdapterId })
    if ($matching.Count -gt 0) { $selected = $matching[0] }
    else { $selected = [Windows.Networking.Connectivity.NetworkInformation]::GetInternetConnectionProfile() }
    if ($null -eq $selected) { throw 'Windows has no connection profile for hotspot shutdown.' }
    return [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager]::CreateFromConnectionProfile($selected)
}

function Stop-OwnedHotspot($AdapterId) {
    $lastError = 'Windows did not confirm hotspot shutdown.'
    for ($retry = 0; $retry -lt 2; $retry++) {
        # A fresh manager/process avoids retaining the object from startup.
        $stopManager = New-StopManager $AdapterId
        if ($stopManager.TetheringOperationalState.ToString() -eq 'Off') { return }
        $operation = $stopManager.StopTetheringAsync()
        $bridge = [System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
            $_.Name -eq 'AsTask' -and $_.IsGenericMethod -and $_.GetGenericArguments().Count -eq 1 -and
            $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'
        } | Select-Object -First 1
        $task = $bridge.MakeGenericMethod([Windows.Networking.NetworkOperators.NetworkOperatorTetheringOperationResult]).Invoke($null, @($operation))
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $offCount = 0
        while ($watch.Elapsed.TotalSeconds -lt 25) {
            # Stop operations may report a timeout after Windows has already
            # switched Off. Confirm the actual state instead of showing that
            # stale operation error, or discarding it while the hotspot is On.
            if ($stopManager.TetheringOperationalState.ToString() -eq 'Off') {
                $offCount++
                if ($offCount -ge 2) { return }
            } else { $offCount = 0 }
            if ($task.IsCompleted -and $offCount -eq 0) {
                try {
                    $result = $task.GetAwaiter().GetResult()
                    if ($result.Status.ToString() -ne 'Success') {
                        $lastError = [string]$result.Status + ': ' + $result.AdditionalErrorMessage
                        break
                    }
                } catch { $lastError = $_.Exception.Message; break }
            }
            Start-Sleep -Milliseconds 250
        }
        $fresh = New-StopManager $AdapterId
        if ($fresh.TetheringOperationalState.ToString() -eq 'Off') { return }
        Start-Sleep -Milliseconds 500
    }
    throw $lastError
}

$ownsSession = $false
$manager = $null
try {
    Add-Type -AssemblyName System.Runtime.WindowsRuntime
    $null = [Windows.Networking.Connectivity.NetworkInformation, Windows.Networking.Connectivity, ContentType = WindowsRuntime]
    $null = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager, Windows.Networking.NetworkOperators, ContentType = WindowsRuntime]
    $null = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringOperationResult, Windows.Networking.NetworkOperators, ContentType = WindowsRuntime]
    if ($Action -eq 'Stop') {
        if ([string]::IsNullOrEmpty($StopFile)) { throw 'The hotspot requires a cancellation marker.' }
        # Only a session started by this controller writes this metadata. Wait
        # for pending startup before treating an initial Off as stopped.
        $deadline = [DateTime]::UtcNow.AddSeconds(50)
        while (-not [IO.File]::Exists($StopFile + '.settled') -and [DateTime]::UtcNow -lt $deadline) {
            if (-not [IO.File]::Exists($StopFile + '.info') -and [DateTime]::UtcNow -gt $deadline.AddSeconds(-45)) { break }
            Start-Sleep -Milliseconds 200
        }
        if ([IO.File]::Exists($StopFile + '.info')) {
            if (-not [IO.File]::Exists($StopFile + '.settled')) { throw 'Hotspot startup has not finished yet. The VPN remains enabled.' }
            $sessionInfo = [IO.File]::ReadAllText($StopFile + '.info') | ConvertFrom-Json
            Stop-OwnedHotspot $sessionInfo.AdapterId
        }
        [IO.File]::WriteAllText($StopFile + '.stopped', 'Off')
        Send-Event @{event='stopped'}
        exit 0
    }
    $adapters = @([Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces())
    $tun = @($adapters | Where-Object { $_.Name -eq 'throne-tun' -and $_.OperationalStatus -eq 'Up' })
    if ($tun.Count -ne 1) { throw 'Enable TUN and connect a profile first.' }
    $tunId = [guid]$tun[0].Id
    $tunIndex = $tun[0].GetIPProperties().GetIPv4Properties().Index
    $profiles = @([Windows.Networking.Connectivity.NetworkInformation]::GetConnectionProfiles() | Where-Object {
        $null -ne $_.NetworkAdapter -and $_.NetworkAdapter.NetworkAdapterId -eq $tunId
    })
    if ($profiles.Count -ne 1) { throw 'Windows has not created the TUN connection profile yet.' }
    $capability = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager]::GetTetheringCapabilityFromConnectionProfile($profiles[0])
    if ($capability.ToString() -ne 'Enabled') { throw ('Windows cannot start a mobile hotspot: {0}' -f $capability) }
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Restart Throne as administrator to use the mobile hotspot.'
    }
    $manager = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager]::CreateFromConnectionProfile($profiles[0])
    $state = $manager.TetheringOperationalState.ToString()
    if ($state -ne 'Off') { throw 'Turn off the existing Windows mobile hotspot first.' }
    Assert-NoLegacySharing
    if ($Action -eq 'Probe') {
        Send-Event @{event='probe'; available=$true; reason=''}
        exit 0
    }
    if ($ParentProcessId -le 0) { throw 'The hotspot requires a live Throne parent process.' }
    $parent = [Diagnostics.Process]::GetProcessById($ParentProcessId)
    $null = $parent.Handle # Keep the original process handle; do not follow a recycled PID.
    if ($parent.HasExited) { throw 'Throne has already exited.' }
    if ([string]::IsNullOrEmpty($StopFile)) { throw 'The hotspot requires a cancellation marker.' }
    $egressIndexes = @($adapters | Where-Object {
        [guid]$_.Id -ne $tunId -and $_.OperationalStatus -eq 'Up' -and
        $_.GetIPProperties().GatewayAddresses.Count -gt 0
    } | ForEach-Object { $_.GetIPProperties().GetIPv4Properties().Index })
    [IO.File]::WriteAllText($StopFile + '.info', (@{AdapterId=$tunId.ToString()} | ConvertTo-Json -Compress))
    if ($parent.HasExited -or [IO.File]::Exists($StopFile)) {
        [IO.File]::WriteAllText($StopFile + '.settled', 'cancelled')
        Send-Event @{event='stopped'}
        exit 0
    }
    # The session was Off; only this operation owns any startup/cleanup below.
    $ownsSession = $true
    try {
        try { Wait-Operation ($manager.StartTetheringAsync()) }
        finally { [IO.File]::WriteAllText($StopFile + '.settled', 'complete') }
        $verified = $false
        for ($attempt = 0; $attempt -lt 10; $attempt++) {
            if ($parent.HasExited -or [IO.File]::Exists($StopFile)) { break }
            if ($manager.TetheringOperationalState.ToString() -eq 'On' -and (Test-Forwarding)) {
                $verified = $true
                break
            }
            Start-Sleep -Milliseconds 500
        }
        if (-not $verified) {
            if (-not $parent.HasExited -and -not [IO.File]::Exists($StopFile)) { throw 'Windows did not confirm forwarding through the TUN adapter.' }
        } else {
            $configuration = $manager.GetCurrentAccessPointConfiguration()
            Send-Event @{event='ready'; ssid=$configuration.Ssid}
            while (-not $parent.HasExited -and -not [IO.File]::Exists($StopFile)) {
                Start-Sleep -Milliseconds 1000
                if ($manager.TetheringOperationalState.ToString() -eq 'Off') { break }
                $active = @([Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces() | Where-Object {
                    [guid]$_.Id -eq $tunId -and $_.OperationalStatus -eq 'Up'
                })
                if ($active.Count -ne 1) { break }
                if (-not (Test-Forwarding)) { throw 'The hotspot network changed. Sharing was stopped.' }
            }
        }
    } finally {
        if ($ownsSession -and $manager.TetheringOperationalState.ToString() -ne 'Off') {
            if ([IO.File]::Exists($StopFile) -and -not $parent.HasExited) {
                # A separate Stop process owns normal shutdown. Keep the TUN
                # and session alive after a failed stop so the user can retry.
                while (-not [IO.File]::Exists($StopFile + '.stopped') -and -not $parent.HasExited) {
                    Start-Sleep -Milliseconds 200
                }
            }
            if (-not [IO.File]::Exists($StopFile + '.stopped')) { Stop-OwnedHotspot $tunId }
        }
        $ownsSession = $false
    }
    Send-Event @{event='stopped'}
    exit 0
} catch {
    if ($Action -eq 'Probe') {
        Send-Event @{event='probe'; available=$false; reason=$_.Exception.Message}
        exit 0
    }
    Send-Event @{event='error'; reason=$_.Exception.Message; off=(-not $ownsSession)}
    exit 1
}
