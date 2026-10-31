param(
    [ValidateSet('Start', 'Status', 'Stop', 'Restart')]
    [string] $Action = 'Start',
    [string] $EbootPath = '',
    [string] $PPSSPPPath = ''
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$runtimeRoot = Join-Path $repositoryRoot 'Runtime'
$runnerRoot = Join-Path $runtimeRoot 'PPSSPP'
$pidPath = Join-Path $runnerRoot 'openbarnyard-psp.pid'

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
        -ArgumentList @($EbootPath) `
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
}
