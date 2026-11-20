[CmdletBinding()]
param(
    [string] $RuntimeDirectory = '',
    [string] $PPSSPPPath = '',
    [ValidateRange(5, 60)]
    [int] $TimeoutSeconds = 20,
    [switch] $SkipMenuInput,
    [switch] $SkipScreenshot
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $RuntimeDirectory) {
    $RuntimeDirectory = Join-Path $repositoryRoot 'Runtime\OpenBarnyardPSPGame'
}
if (-not $PPSSPPPath) {
    $developmentBuild = Join-Path $repositoryRoot 'Runtime\Tools\PPSSPPDev\PPSSPPWindows64.exe'
    $stableBuild = Join-Path $repositoryRoot 'Runtime\Tools\PPSSPP\PPSSPPWindows64.exe'
    $PPSSPPPath = if (Test-Path -LiteralPath $developmentBuild -PathType Leaf) { $developmentBuild } else { $stableBuild }
}

$runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
$ppssppFullPath = [System.IO.Path]::GetFullPath($PPSSPPPath)
$ebootPath = Join-Path $runtimePath 'EBOOT.PBP'
$reportPath = Join-Path $runtimePath 'openbarnyard_psp_game.log'
$runnerPath = Join-Path $PSScriptRoot 'RunPPSSPP.ps1'
$screenshotDirectory = Join-Path (Split-Path -Parent $ppssppFullPath) 'memstick\PSP\SCREENSHOT'

foreach ($requiredFile in @(
    $ebootPath,
    (Join-Path $runtimePath 'frontend_camera.pcam'),
    (Join-Path $runtimePath 'barnyard_terrain_0.pmes'),
    (Join-Path $runtimePath 'frontend_barnsign_0.pmes'),
    (Join-Path $runtimePath 'frontend_barnsign_1.pmes'),
    $ppssppFullPath,
    $runnerPath
)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Original PSP validation input was not found: $requiredFile"
    }
}

function Read-RuntimeReport([string] $Path) {
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        $pair = $line.Split('=', 2)
        if ($pair.Count -eq 2 -and $pair[0]) {
            $values[$pair[0]] = $pair[1]
        }
    }
    return $values
}

function Assert-ReportValue([hashtable] $Report, [string] $Name, [string] $Expected) {
    if (-not $Report.ContainsKey($Name) -or $Report[$Name] -ne $Expected) {
        $actual = if ($Report.ContainsKey($Name)) { $Report[$Name] } else { '<missing>' }
        throw "Runtime report expected $Name=$Expected but found $actual"
    }
}

function Assert-ReportPositive([hashtable] $Report, [string] $Name) {
    $value = 0
    if (-not $Report.ContainsKey($Name) -or -not [int]::TryParse($Report[$Name], [ref] $value) -or $value -le 0) {
        $actual = if ($Report.ContainsKey($Name)) { $Report[$Name] } else { '<missing>' }
        throw "Runtime report expected positive $Name but found $actual"
    }
}

$startedAt = Get-Date
$screenshotPath = $null
try {
    & $runnerPath -Action Restart -EbootPath $ebootPath -PPSSPPPath $ppssppFullPath

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Milliseconds 250
        if (Test-Path -LiteralPath $reportPath -PathType Leaf) {
            $reportItem = Get-Item -LiteralPath $reportPath
            if ($reportItem.LastWriteTime -ge $startedAt -and (Get-Content -LiteralPath $reportPath -Raw) -match '(?m)^phase=frontend-menu') {
                break
            }
        }
    } while ((Get-Date) -lt $deadline)

    if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf) -or
        (Get-Item -LiteralPath $reportPath).LastWriteTime -lt $startedAt) {
        throw "PPSSPP did not produce a fresh runtime report within $TimeoutSeconds seconds"
    }

    if (-not $SkipScreenshot) {
        & $runnerPath -Action Press -EbootPath $ebootPath -PPSSPPPath $ppssppFullPath -Key Screenshot
    }
    if (-not $SkipMenuInput) {
        & $runnerPath -Action Press -EbootPath $ebootPath -PPSSPPPath $ppssppFullPath -Key DpadDown
        Start-Sleep -Milliseconds 500
        & $runnerPath -Action Press -EbootPath $ebootPath -PPSSPPPath $ppssppFullPath -Key Cross
        Start-Sleep -Milliseconds 750
    }

    $report = Read-RuntimeReport $reportPath
    if (-not $report.ContainsKey('phase') -or $report.phase -notlike 'frontend-menu*') {
        $actualPhase = if ($report.ContainsKey('phase')) { $report.phase } else { '<missing>' }
        throw "Runtime report did not reach the original front-end menu: $actualPhase"
    }
    foreach ($name in @(
        'engine',
        'application',
        'root',
        'gui2',
        'gui_system',
        'controls',
        'frontend_terrain_loaded',
        'frontend_camera_loaded',
        'frontend_barnsign_loaded'
    )) {
        Assert-ReportValue $report $name '1'
    }
    Assert-ReportValue $report 'frontend_terrain_meshes' '11'
    Assert-ReportValue $report 'frontend_barnsign_meshes' '2'
    foreach ($name in @('renderer_frames', 'frontend_terrain_draws', 'frontend_barnsign_draws')) {
        Assert-ReportPositive $report $name
    }
    if (-not $SkipMenuInput) {
        foreach ($name in @('menu_command_down_count', 'menu_command_ok_count', 'menu_activation_count')) {
            Assert-ReportPositive $report $name
        }
    }

    if (-not $SkipScreenshot -and (Test-Path -LiteralPath $screenshotDirectory -PathType Container)) {
        $screenshotPath = Get-ChildItem -LiteralPath $screenshotDirectory -Filter '*.jpg' -File |
            Where-Object { $_.LastWriteTime -ge $startedAt } |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1 -ExpandProperty FullName
        if (-not $screenshotPath) {
            throw 'PPSSPP did not write the requested screenshot'
        }
    }

    [pscustomobject]@{
        Phase = $report.phase
        Frames = [int] $report.renderer_frames
        TerrainDraws = [int] $report.frontend_terrain_draws
        BarnSignDraws = [int] $report.frontend_barnsign_draws
        HeapFree = [int] $report.heap_free
        Screenshot = $screenshotPath
        Report = $reportPath
    }
}
finally {
    & $runnerPath -Action Stop -EbootPath $ebootPath -PPSSPPPath $ppssppFullPath
}
