param(
    [ValidateSet('Start', 'Status', 'Stop', 'Restart', 'Press')]
    [string] $Action = 'Start',
    [string] $EbootPath = '',
    [string] $PPSSPPPath = '',

    [ValidateSet('Cross', 'Circle', 'AnalogLeft', 'AnalogRight')]
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

function Get-OpenBarnyardPPSSPPProcess {
    if (-not (Test-Path -LiteralPath $pidPath)) {
        return $null
    }

    $savedPID = (Get-Content -LiteralPath $pidPath -Raw).Trim()
    if ($savedPID -notmatch '^\d+$') {
        return $null
    }

    $process = Get-Process -Id ([int] $savedPID) -ErrorAction SilentlyContinue
    if (-not $process) {
        return $null
    }

    try {
        if ([System.IO.Path]::GetFullPath($process.Path) -ne $PPSSPPPath) {
            return $null
        }
    }
    catch {
        return $null
    }

    return $process
}

function Remove-StalePIDFile {
    if ((Test-Path -LiteralPath $pidPath) -and -not (Get-OpenBarnyardPPSSPPProcess)) {
        Remove-Item -LiteralPath $pidPath -Force
    }
}

function Start-OpenBarnyardPPSSPP {
    $process = Get-OpenBarnyardPPSSPPProcess
    if ($process) {
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
        -ArgumentList @('--graphics=software', '--windowed', '--scale=1', $EbootPath) `
        -WorkingDirectory (Split-Path -Parent $PPSSPPPath) `
        -WindowStyle Minimized `
        -PassThru

    Set-Content -LiteralPath $pidPath -Value $process.Id -NoNewline
    Write-Output "Started PPSSPP in the background (PID $($process.Id))."
}

function Stop-OpenBarnyardPPSSPP {
    $process = Get-OpenBarnyardPPSSPPProcess
    if (-not $process) {
        Remove-StalePIDFile
        Write-Output 'PPSSPP is not running.'
        return
    }

    Stop-Process -Id $process.Id
    $process.WaitForExit()
    Remove-Item -LiteralPath $pidPath -Force -ErrorAction SilentlyContinue
    Write-Output "Stopped PPSSPP (PID $($process.Id))."
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
    $virtualKey = [UIntPtr] $virtualKeys[$Button]
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

        Send-OpenBarnyardPPSSPPKey -Process $process -Button $Key -Count $Repeat
    }
}
