param(
    [ValidateSet('Start', 'Status', 'Stop', 'Restart', 'Press')]
    [string] $Action = 'Start',
    [string] $EbootPath = '',
    [string] $PPSSPPPath = '',
    [switch] $OriginalGame,
    [switch] $ShowWindow,

    [ValidateSet('Cross', 'Circle', 'Triangle', 'Start', 'DpadUp', 'DpadDown', 'DpadLeft', 'DpadRight', 'AnalogUp', 'AnalogDown', 'AnalogLeft', 'AnalogRight', 'Screenshot')]
    [string] $Key = 'Cross',

    [ValidateRange(1, 60)]
    [int] $Repeat = 1,

    [ValidateRange(1, 60)]
    [int] $ButtonDurationFrames = 15,

    [ValidateRange(20, 500)]
    [int] $AnalogHoldMilliseconds = 100,

    [ValidateRange(1024, 65535)]
    [int] $DebuggerPort = 7755,

    [ValidateRange(100, 5000)]
    [int] $StartupWaitMilliseconds = 2000
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$runtimeRoot = Join-Path $repositoryRoot 'Runtime'
$runnerRoot = Join-Path $runtimeRoot 'PPSSPP'
$pidPath = Join-Path $runnerRoot 'openbarnyard-psp.pid'

if (-not ('OpenBarnyard.PPSSPPWindow' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

namespace OpenBarnyard
{
    public static class PPSSPPWindow
    {
        private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

        [DllImport("user32.dll")]
        public static extern bool ShowWindowAsync(IntPtr window, int command);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool SetWindowPos(
            IntPtr window,
            IntPtr insertAfter,
            int x,
            int y,
            int width,
            int height,
            uint flags);

        [DllImport("user32.dll")]
        private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);

        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

        [DllImport("user32.dll")]
        private static extern IntPtr GetWindow(IntPtr window, uint command);

        public static IntPtr FindTopLevelWindow(int processId)
        {
            IntPtr result = IntPtr.Zero;
            EnumWindows(delegate(IntPtr window, IntPtr parameter)
            {
                uint windowProcessId;
                GetWindowThreadProcessId(window, out windowProcessId);
                if (windowProcessId == (uint)processId && GetWindow(window, 4) == IntPtr.Zero)
                {
                    result = window;
                    return false;
                }

                return true;
            }, IntPtr.Zero);
            return result;
        }

    }
}
'@
}

if (-not $PPSSPPPath) {
    $developmentBuild = Join-Path $runtimeRoot 'Tools\PPSSPPDev\PPSSPPWindows64.exe'
    $stableBuild = Join-Path $runtimeRoot 'Tools\PPSSPP\PPSSPPWindows64.exe'
    $PPSSPPPath = if (Test-Path -LiteralPath $developmentBuild -PathType Leaf) { $developmentBuild } else { $stableBuild }
}

if (-not $EbootPath) {
    if ($OriginalGame) {
        $EbootPath = Join-Path $runtimeRoot 'OpenBarnyardPSPGame\EBOOT.PBP'
    }
    else {
        $EbootPath = Join-Path $repositoryRoot 'bin\PSP\Release\EBOOT.PBP'
    }
}

$PPSSPPPath = [System.IO.Path]::GetFullPath($PPSSPPPath)
$EbootPath = [System.IO.Path]::GetFullPath($EbootPath)

function Get-ManagedPPSSPPProcesses {
    $processName = [System.IO.Path]::GetFileNameWithoutExtension($PPSSPPPath)
    return @(Get-Process -Name $processName -ErrorAction SilentlyContinue | Where-Object {
        try {
            [System.IO.Path]::GetFullPath($_.Path) -ieq $PPSSPPPath
        }
        catch {
            $false
        }
    })
}

function Get-OpenBarnyardPPSSPPProcess {
    if (Test-Path -LiteralPath $pidPath) {
        $savedPID = (Get-Content -LiteralPath $pidPath -Raw).Trim()
        if ($savedPID -match '^\d+$') {
            $process = Get-Process -Id ([int] $savedPID) -ErrorAction SilentlyContinue
            if ($process) {
                try {
                    if ([System.IO.Path]::GetFullPath($process.Path) -ieq $PPSSPPPath) {
                        return $process
                    }
                }
                catch {}
            }
        }
    }

    $managedProcesses = @(Get-ManagedPPSSPPProcesses | Sort-Object StartTime -Descending)
    if ($managedProcesses.Count -eq 0) {
        return $null
    }

    $process = $managedProcesses[0]
    New-Item -ItemType Directory -Path $runnerRoot -Force | Out-Null
    Set-Content -LiteralPath $pidPath -Value $process.Id -NoNewline
    return $process
}

function Remove-DuplicatePPSSPPProcesses([System.Diagnostics.Process] $KeepProcess) {
    foreach ($process in Get-ManagedPPSSPPProcesses) {
        if ($KeepProcess -and $process.Id -eq $KeepProcess.Id) {
            continue
        }
        Stop-Process -Id $process.Id
        $process.WaitForExit()
        $label = if ($KeepProcess) { 'duplicate PPSSPP' } else { 'PPSSPP' }
        Write-Output "Stopped $label (PID $($process.Id))."
    }
}

function Remove-StalePIDFile {
    if ((Test-Path -LiteralPath $pidPath) -and -not (Get-OpenBarnyardPPSSPPProcess)) {
        Remove-Item -LiteralPath $pidPath -Force
    }
}

function Enable-PPSSPPDebugger {
    $configPath = Join-Path (Split-Path -Parent $PPSSPPPath) 'memstick\PSP\SYSTEM\ppsspp.ini'
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
        throw "PPSSPP configuration was not found: $configPath"
    }

    $config = Get-Content -LiteralPath $configPath -Raw
    if ($config -notmatch '(?m)^RemoteISOPort = ' -or $config -notmatch '(?m)^RemoteDebuggerOnStartup = ') {
        throw "PPSSPP configuration does not expose the remote debugger settings: $configPath"
    }

    $config = $config -replace '(?m)^RemoteISOPort = .*$', "RemoteISOPort = $DebuggerPort"
    $config = $config -replace '(?m)^RemoteDebuggerOnStartup = .*$', 'RemoteDebuggerOnStartup = True'
    Set-Content -LiteralPath $configPath -Value $config -NoNewline
}

function Move-PPSSPPWindowOffScreen([System.Diagnostics.Process] $Process) {
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 100; $attempt++) {
        $Process.Refresh()
        $window = $Process.MainWindowHandle
        if ($window -eq [IntPtr]::Zero) {
            $window = [OpenBarnyard.PPSSPPWindow]::FindTopLevelWindow($Process.Id)
        }
        if ($window -ne [IntPtr]::Zero) {
            break
        }

        Start-Sleep -Milliseconds 20
    }

    if ($window -eq [IntPtr]::Zero) {
        throw "PPSSPP process $($Process.Id) has no window handle."
    }

    $noSizeNoZOrderNoActivate = 0x0015
    if (-not [OpenBarnyard.PPSSPPWindow]::SetWindowPos(
        $window,
        [IntPtr]::Zero,
        -32000,
        -32000,
        0,
        0,
        $noSizeNoZOrderNoActivate
    )) {
        throw "Could not move PPSSPP process $($Process.Id) off screen."
    }
    # D3D11 must see a non-minimised window to keep producing framebuffer
    # captures. SW_SHOWNOACTIVATE keeps that window off screen and unfocused.
    [OpenBarnyard.PPSSPPWindow]::ShowWindowAsync($window, 4) | Out-Null
}

function Start-OpenBarnyardPPSSPP {
    $process = Get-OpenBarnyardPPSSPPProcess
    if ($process) {
        Remove-DuplicatePPSSPPProcesses $process
        Write-Output "PPSSPP is already running (PID $($process.Id))."
        return
    }

    Remove-StalePIDFile
    if (Get-Process -Name PPSSPPWindows64, PPSSPPWindows -ErrorAction SilentlyContinue) {
        throw 'Another PPSSPP instance is already running.'
    }

    if (-not (Test-Path -LiteralPath $PPSSPPPath -PathType Leaf)) {
        throw "PPSSPP executable was not found: $PPSSPPPath"
    }

    if (-not (Test-Path -LiteralPath $EbootPath -PathType Leaf)) {
        throw "PSP executable was not found: $EbootPath"
    }

    New-Item -ItemType Directory -Path $runnerRoot -Force | Out-Null
    Enable-PPSSPPDebugger

    $windowStyle = if ($ShowWindow) { 'Normal' } else { 'Hidden' }
    $process = Start-Process `
        -FilePath $PPSSPPPath `
        -ArgumentList @('--windowed', '--scale=1', $EbootPath) `
        -WorkingDirectory (Split-Path -Parent $PPSSPPPath) `
        -WindowStyle $windowStyle `
        -PassThru

    Set-Content -LiteralPath $pidPath -Value $process.Id -NoNewline
    if ($process.WaitForExit($StartupWaitMilliseconds)) {
        Remove-Item -LiteralPath $pidPath -Force -ErrorAction SilentlyContinue
        throw "PPSSPP exited during startup with code $($process.ExitCode)."
    }
    if ($ShowWindow) {
        Write-Output "Started PPSSPP with a visible window (PID $($process.Id))."
    }
    else {
        Move-PPSSPPWindowOffScreen $process
        Write-Output "Started PPSSPP in the background (PID $($process.Id))."
    }
}

function Stop-OpenBarnyardPPSSPP {
    $process = Get-OpenBarnyardPPSSPPProcess
    if (-not $process) {
        Remove-StalePIDFile
        Write-Output 'PPSSPP is not running.'
        return
    }

    Remove-DuplicatePPSSPPProcesses $null
    Remove-Item -LiteralPath $pidPath -Force -ErrorAction SilentlyContinue
}

function Send-PPSSPPDebuggerText {
    param(
        [System.Net.WebSockets.ClientWebSocket] $Socket,
        [string] $Text
    )

    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    $segment = [System.ArraySegment[byte]]::new($bytes)
    $null = $Socket.SendAsync(
        $segment,
        [System.Net.WebSockets.WebSocketMessageType]::Text,
        $true,
        [System.Threading.CancellationToken]::None
    ).GetAwaiter().GetResult()
}

function Receive-PPSSPPDebuggerText {
    param(
        [System.Net.WebSockets.ClientWebSocket] $Socket,
        [int] $TimeoutMilliseconds
    )

    $buffer = New-Object byte[] 4096
    $stream = [System.IO.MemoryStream]::new()
    $cancellation = [System.Threading.CancellationTokenSource]::new($TimeoutMilliseconds)
    try {
        do {
            $segment = [System.ArraySegment[byte]]::new($buffer)
            $result = $Socket.ReceiveAsync($segment, $cancellation.Token).GetAwaiter().GetResult()
            if ($result.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Close) {
                throw 'The PPSSPP debugger closed the WebSocket connection.'
            }

            $stream.Write($buffer, 0, $result.Count)
        } while (-not $result.EndOfMessage)

        return [System.Text.Encoding]::UTF8.GetString($stream.ToArray())
    }
    finally {
        $cancellation.Dispose()
        $stream.Dispose()
    }
}

function Wait-PPSSPPDebuggerResponse {
    param(
        [System.Net.WebSockets.ClientWebSocket] $Socket,
        [string] $EventName,
        [int] $Ticket,
        [int] $TimeoutMilliseconds = 5000
    )

    $deadline = [System.DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    while ([System.DateTime]::UtcNow -lt $deadline) {
        $remaining = [int] [System.Math]::Max(1, ($deadline - [System.DateTime]::UtcNow).TotalMilliseconds)
        $message = Receive-PPSSPPDebuggerText -Socket $Socket -TimeoutMilliseconds $remaining
        Write-Verbose "PPSSPP debugger: $message"
        $response = $message | ConvertFrom-Json
        $hasTicket = $response.PSObject.Properties.Name -contains 'ticket'
        if ($response.event -eq 'error' -and $hasTicket -and [string] $response.ticket -eq [string] $Ticket) {
            throw "PPSSPP debugger rejected $EventName`: $($response.message)"
        }

        if ($response.event -eq $EventName -and $hasTicket -and [string] $response.ticket -eq [string] $Ticket) {
            return $response
        }
    }

    throw "Timed out waiting for the PPSSPP debugger response to $EventName."
}

function Wait-PPSSPPDebuggerEvent {
    param(
        [System.Net.WebSockets.ClientWebSocket] $Socket,
        [string] $EventName,
        [int] $TimeoutMilliseconds = 5000
    )

    $deadline = [System.DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    while ([System.DateTime]::UtcNow -lt $deadline) {
        $remaining = [int] [System.Math]::Max(1, ($deadline - [System.DateTime]::UtcNow).TotalMilliseconds)
        $message = Receive-PPSSPPDebuggerText -Socket $Socket -TimeoutMilliseconds $remaining
        Write-Verbose "PPSSPP debugger: $message"
        $response = $message | ConvertFrom-Json
        if ($response.event -eq 'error') {
            throw "PPSSPP debugger rejected $EventName`: $($response.message)"
        }
        if ($response.event -eq $EventName) {
            return $response
        }
    }

    throw "Timed out waiting for the PPSSPP debugger event $EventName."
}

function Invoke-PPSSPPDebuggerRequest {
    param(
        [System.Net.WebSockets.ClientWebSocket] $Socket,
        [string] $EventName,
        [hashtable] $Parameters,
        [int] $Ticket,
        [int] $TimeoutMilliseconds = 5000
    )

    $request = @{
        event = $EventName
        ticket = $Ticket
    }
    foreach ($key in $Parameters.Keys) {
        $request[$key] = $Parameters[$key]
    }

    Send-PPSSPPDebuggerText -Socket $Socket -Text ($request | ConvertTo-Json -Compress)
    return Wait-PPSSPPDebuggerResponse `
        -Socket $Socket `
        -EventName $EventName `
        -Ticket $Ticket `
        -TimeoutMilliseconds $TimeoutMilliseconds
}

function Connect-PPSSPPDebugger {
    param([System.Diagnostics.Process] $Process)

    $uri = [System.Uri]::new("ws://127.0.0.1:$DebuggerPort/debugger")
    $lastError = $null
    for ($attempt = 0; $attempt -lt 40; $attempt++) {
        $socket = [System.Net.WebSockets.ClientWebSocket]::new()
        $socket.Options.AddSubProtocol('debugger.ppsspp.org')
        $cancellation = [System.Threading.CancellationTokenSource]::new(500)
        try {
            $null = $socket.ConnectAsync($uri, $cancellation.Token).GetAwaiter().GetResult()
            $response = Invoke-PPSSPPDebuggerRequest `
                -Socket $socket `
                -EventName 'version' `
                -Parameters @{ name = 'OpenBarnyard PSP runner'; version = '1' } `
                -Ticket 1
            if ($response.name -ne 'PPSSPP') {
                throw "Debugger port $DebuggerPort did not identify itself as PPSSPP."
            }
            $hasPID = $response.PSObject.Properties.Name -contains 'pid'
            if ($hasPID -and [int] $response.pid -ne $Process.Id) {
                throw "Debugger port $DebuggerPort belongs to PPSSPP process $($response.pid), not $($Process.Id)."
            }

            return $socket
        }
        catch {
            $lastError = $_
            $socket.Dispose()
            Start-Sleep -Milliseconds 50
        }
        finally {
            $cancellation.Dispose()
        }
    }

    throw "Could not connect to the PPSSPP debugger on port $DebuggerPort. Restart PPSSPP through this script. Last error: $lastError"
}

function Send-OpenBarnyardPPSSPPKey {
    param(
        [System.Diagnostics.Process] $Process,
        [string] $Button,
        [int] $Count
    )

    $buttons = @{
        Cross = 'cross'
        Circle = 'circle'
        Triangle = 'triangle'
        Start = 'start'
        DpadUp = 'up'
        DpadDown = 'down'
        DpadLeft = 'left'
        DpadRight = 'right'
    }

    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 50; $attempt++) {
        $Process.Refresh()
        $window = $Process.MainWindowHandle
        if ($window -eq [IntPtr]::Zero) {
            $window = [OpenBarnyard.PPSSPPWindow]::FindTopLevelWindow($Process.Id)
        }
        if ($window -ne [IntPtr]::Zero) {
            break
        }

        Start-Sleep -Milliseconds 20
    }

    if ($window -eq [IntPtr]::Zero) {
        throw "PPSSPP process $($Process.Id) has no window handle."
    }

    if ($Button -eq 'Screenshot') {
        if (-not $ShowWindow) { Move-PPSSPPWindowOffScreen $Process }
        $socket = Connect-PPSSPPDebugger -Process $Process
        $cpuStopped = $false
        try {
            Send-PPSSPPDebuggerText -Socket $socket -Text '{"event":"cpu.stepping"}'
            $null = Wait-PPSSPPDebuggerEvent -Socket $socket -EventName 'cpu.stepping'
            $cpuStopped = $true
            $response = Invoke-PPSSPPDebuggerRequest `
                -Socket $socket `
                -EventName 'gpu.buffer.screenshot' `
                -Parameters @{ type = 'uri'; alpha = $false } `
                -Ticket 2 `
                -TimeoutMilliseconds 10000
            if (-not $response.uri -or $response.uri -notmatch '^data:image/png;base64,') {
                throw 'PPSSPP debugger returned an invalid render-buffer screenshot.'
            }

            $screenshotDirectory = Join-Path (Split-Path -Parent $PPSSPPPath) 'memstick\PSP\SCREENSHOT'
            New-Item -ItemType Directory -Path $screenshotDirectory -Force | Out-Null
            $screenshotName = 'OpenBarnyardPSP_{0}.png' -f (Get-Date -Format 'yyyyMMdd_HHmmss_fff')
            $screenshotPath = Join-Path $screenshotDirectory $screenshotName
            $base64 = $response.uri.Substring('data:image/png;base64,'.Length)
            [System.IO.File]::WriteAllBytes($screenshotPath, [System.Convert]::FromBase64String($base64))
        }
        finally {
            if ($cpuStopped) {
                Send-PPSSPPDebuggerText -Socket $socket -Text '{"event":"cpu.resume"}'
            }
            $socket.Dispose()
        }

        Write-Output "Saved a debugger screenshot from PPSSPP (PID $($Process.Id)): $screenshotPath"
        return
    }

    $socket = Connect-PPSSPPDebugger -Process $Process
    try {
        for ($index = 0; $index -lt $Count; $index++) {
            $ticket = $index + 2
            if ($buttons.ContainsKey($Button)) {
                $null = Invoke-PPSSPPDebuggerRequest `
                    -Socket $socket `
                    -EventName 'input.buttons.press' `
                    -Parameters @{ button = $buttons[$Button]; duration = $ButtonDurationFrames } `
                    -Ticket $ticket
                continue
            }

            $analog = switch ($Button) {
                'AnalogUp' { @{ x = 0.0; y = -1.0 } }
                'AnalogDown' { @{ x = 0.0; y = 1.0 } }
                'AnalogLeft' { @{ x = -1.0; y = 0.0 } }
                'AnalogRight' { @{ x = 1.0; y = 0.0 } }
            }
            $null = Invoke-PPSSPPDebuggerRequest `
                -Socket $socket `
                -EventName 'input.analog.send' `
                -Parameters $analog `
                -Ticket $ticket
            Start-Sleep -Milliseconds $AnalogHoldMilliseconds
            $null = Invoke-PPSSPPDebuggerRequest `
                -Socket $socket `
                -EventName 'input.analog.send' `
                -Parameters @{ x = 0.0; y = 0.0 } `
                -Ticket ($ticket + $Count)
        }
    }
    finally {
        $socket.Dispose()
    }

    Write-Output "Injected $Button through the PPSSPP debugger (PID $($Process.Id), count $Count)."
}

switch ($Action) {
    'Start' { Start-OpenBarnyardPPSSPP }
    'Status' {
        $process = Get-OpenBarnyardPPSSPPProcess
        if ($process) {
            Write-Output "PPSSPP is running (PID $($process.Id))."
        }
        else {
            Remove-StalePIDFile
            Write-Output 'PPSSPP is not running.'
        }
    }
    'Stop' { Stop-OpenBarnyardPPSSPP }
    'Restart' {
        Stop-OpenBarnyardPPSSPP
        Start-OpenBarnyardPPSSPP
    }
    'Press' {
        $process = Get-OpenBarnyardPPSSPPProcess
        if (-not $process) {
            throw 'PPSSPP is not running. Start it before sending background input.'
        }

        Remove-DuplicatePPSSPPProcesses $process
        Send-OpenBarnyardPPSSPPKey -Process $process -Button $Key -Count $Repeat
    }
}
