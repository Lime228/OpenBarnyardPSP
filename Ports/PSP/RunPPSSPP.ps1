param(
    [ValidateSet('Start', 'Status', 'Stop', 'Restart', 'Press')]
    [string] $Action = 'Start',
    [string] $EbootPath = '',
    [string] $PPSSPPPath = '',

    [ValidateSet('Cross', 'Circle', 'Start', 'DpadUp', 'DpadDown', 'AnalogUp', 'AnalogDown', 'AnalogLeft', 'AnalogRight')]
    [string] $Key = 'Cross',

    [ValidateRange(1, 60)]
    [int] $Repeat = 1
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
        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool PostMessage(IntPtr window, uint message, UIntPtr wParam, IntPtr lParam);
    }
}
'@
}

if (-not $PPSSPPPath) {
    $PPSSPPPath = Join-Path $runtimeRoot 'Tools\PPSSPP\PPSSPPWindows64.exe'
}

if (-not $EbootPath) {
    $EbootPath = Join-Path $repositoryRoot 'bin\PSP\Release\EBOOT.PBP'
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

function Start-OpenBarnyardPPSSPP {
    $process = Get-OpenBarnyardPPSSPPProcess
    if ($process) {
        Remove-DuplicatePPSSPPProcesses $process
        Write-Output "PPSSPP is already running (PID $($process.Id))."
        return
    }

    Remove-StalePIDFile

    if (-not (Test-Path -LiteralPath $PPSSPPPath -PathType Leaf)) {
        throw "PPSSPP executable was not found: $PPSSPPPath"
    }

    if (-not (Test-Path -LiteralPath $EbootPath -PathType Leaf)) {
        throw "PSP executable was not found: $EbootPath"
    }

    New-Item -ItemType Directory -Path $runnerRoot -Force | Out-Null

    $process = Start-Process `
        -FilePath $PPSSPPPath `
        -ArgumentList @('--windowed', '--scale=1', $EbootPath) `
        -WorkingDirectory (Split-Path -Parent $PPSSPPPath) `
        -WindowStyle Minimized `
        -PassThru

    Set-Content -LiteralPath $pidPath -Value $process.Id -NoNewline
    if ($process.WaitForExit(2000)) {
        Remove-Item -LiteralPath $pidPath -Force -ErrorAction SilentlyContinue
        throw "PPSSPP exited during startup with code $($process.ExitCode)."
    }
    Write-Output "Started PPSSPP in the background (PID $($process.Id))."
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

function Send-OpenBarnyardPPSSPPKey {
    param(
        [System.Diagnostics.Process] $Process,
        [string] $Button,
        [int] $Count
    )

    $virtualKeys = @{
        Cross = 0x5A
        Circle = 0x58
        Start = 0x20
        DpadUp = 0x26
        DpadDown = 0x28
        AnalogUp = 0x49
        AnalogDown = 0x4B
        AnalogLeft = 0x4A
        AnalogRight = 0x4C
    }

    for ($attempt = 0; $attempt -lt 50; $attempt++) {
        $Process.Refresh()
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            break
        }

        Start-Sleep -Milliseconds 20
    }

    if ($Process.MainWindowHandle -eq [IntPtr]::Zero) {
        throw "PPSSPP process $($Process.Id) has no window handle."
    }

    $window = $Process.MainWindowHandle
    $virtualKey = [UIntPtr]::new([uint32] $virtualKeys[$Button])
    for ($index = 0; $index -lt $Count; $index++) {
        if (-not [OpenBarnyard.PPSSPPWindow]::PostMessage($window, 0x0100, $virtualKey, [IntPtr]::Zero)) {
            throw "Could not send $Button to PPSSPP process $($Process.Id)."
        }

        Start-Sleep -Milliseconds 20
        [OpenBarnyard.PPSSPPWindow]::PostMessage($window, 0x0101, $virtualKey, [IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 20
    }

    Write-Output "Sent $Button to PPSSPP (PID $($Process.Id), count $Count)."
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
