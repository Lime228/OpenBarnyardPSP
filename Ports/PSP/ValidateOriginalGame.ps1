[CmdletBinding()]
param(
    [string] $RuntimeDirectory = '',
    [string] $PPSSPPPath = '',
    [ValidateRange(5, 60)]
    [int] $TimeoutSeconds = 60,
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

$requiredFiles = @(
    $ebootPath,
    (Join-Path $runtimePath 'frontend_camera.pcam'),
    (Join-Path $runtimePath 'barnyard_terrain_0.pmes'),
    (Join-Path $runtimePath 'frontend_detail.pcfg'),
    (Join-Path $runtimePath 'frontend_detail_model_0.pmes'),
    (Join-Path $runtimePath 'frontend_detail_texture_0.ptex'),
    (Join-Path $runtimePath 'frontend_grass.pcfg'),
    (Join-Path $runtimePath 'frontend_grass_model_0.pmes'),
    (Join-Path $runtimePath 'frontend_grass_texture_0.ptex'),
    (Join-Path $runtimePath 'frontend_glow.pcfg'),
    (Join-Path $runtimePath 'frontend_glow_model_0.pmes'),
    (Join-Path $runtimePath 'frontend_glow_model_1.pmes'),
    (Join-Path $runtimePath 'frontend_glow_model_2.pmes'),
    (Join-Path $runtimePath 'frontend_glow_texture_0.ptex'),
    (Join-Path $runtimePath 'frontend_barnsign_0.pmes'),
    (Join-Path $runtimePath 'frontend_barnsign_1.pmes'),
    $ppssppFullPath,
    $runnerPath
)
$frontendInstancePacks = @(
    @{ Prefix = 'frontend_instances_fncw'; Models = 3; Textures = 1 },
    @{ Prefix = 'frontend_instances_fncp'; Models = 3; Textures = 1 },
    @{ Prefix = 'frontend_instances_fncborder'; Models = 7; Textures = 3 },
    @{ Prefix = 'frontend_instances_flowers'; Models = 9; Textures = 4 },
    @{ Prefix = 'frontend_instances_rocks'; Models = 5; Textures = 4 },
    @{ Prefix = 'frontend_instances_props'; Models = 9; Textures = 7 },
    @{ Prefix = 'frontend_instances_trees'; Models = 5; Textures = 4 }
)
foreach ($pack in $frontendInstancePacks) {
    $requiredFiles += Join-Path $runtimePath "$($pack.Prefix).pcfg"
    for ($modelIndex = 0; $modelIndex -lt $pack.Models; $modelIndex++) {
        $requiredFiles += Join-Path $runtimePath "$($pack.Prefix)_model_$modelIndex.pmes"
    }
    for ($textureIndex = 0; $textureIndex -lt $pack.Textures; $textureIndex++) {
        $requiredFiles += Join-Path $runtimePath "$($pack.Prefix)_texture_$textureIndex.ptex"
    }
}
foreach ($pack in @(
    @{ Prefix = 'frontend_barnnorth_base'; Models = 44; Textures = 44 },
    @{ Prefix = 'frontend_barnnorth_detail'; Models = 8; Textures = 8 },
    @{ Prefix = 'frontend_barnnorth_grass'; Models = 1; Textures = 1 },
    @{ Prefix = 'frontend_barnnorth_glow'; Models = 2; Textures = 1 },
    @{ Prefix = 'frontend_bgbarneast'; Models = 7; Textures = 7 },
    @{ Prefix = 'frontend_bgbarnwest'; Models = 5; Textures = 5 },
    @{ Prefix = 'frontend_hillbase'; Models = 7; Textures = 7 },
    @{ Prefix = 'frontend_hilltop'; Models = 9; Textures = 9 },
    @{ Prefix = 'frontend_meadowsouth'; Models = 15; Textures = 15 }
)) {
    $requiredFiles += Join-Path $runtimePath "$($pack.Prefix).pcfg"
    for ($modelIndex = 0; $modelIndex -lt $pack.Models; $modelIndex++) {
        $requiredFiles += Join-Path $runtimePath "$($pack.Prefix)_model_$modelIndex.pmes"
    }
    for ($textureIndex = 0; $textureIndex -lt $pack.Textures; $textureIndex++) {
        $requiredFiles += Join-Path $runtimePath "$($pack.Prefix)_texture_$textureIndex.ptex"
    }
}
foreach ($requiredFile in $requiredFiles) {
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
        $menuReadyDeadline = (Get-Date).AddSeconds(20)
        do {
            $menuReadyReport = Read-RuntimeReport $reportPath
            if ($menuReadyReport.ContainsKey('phase') -and $menuReadyReport.phase -eq 'frontend-menu-ready') {
                break
            }
            Start-Sleep -Milliseconds 100
        } while ((Get-Date) -lt $menuReadyDeadline)
        if (-not $menuReadyReport.ContainsKey('phase') -or $menuReadyReport.phase -ne 'frontend-menu-ready') {
            throw 'PPSSPP did not reach the stable original menu before the screenshot'
        }

        $menuVisibleStartFrame = [int] $menuReadyReport.renderer_frames
        $menuVisibleTargetFrame = $menuVisibleStartFrame + 240
        $menuVisibleDeadline = (Get-Date).AddSeconds(15)
        do {
            Start-Sleep -Milliseconds 100
            $menuVisibleReport = Read-RuntimeReport $reportPath
            $menuVisible = $menuVisibleReport.ContainsKey('renderer_frames') -and
                [int] $menuVisibleReport.renderer_frames -ge $menuVisibleTargetFrame
        } while (-not $menuVisible -and (Get-Date) -lt $menuVisibleDeadline)
        if (-not $menuVisible) {
            throw 'PPSSPP did not finish the original menu fade before the screenshot'
        }

        $screenshotStartReport = Read-RuntimeReport $reportPath
        $screenshotStartFrame = [int] $screenshotStartReport.renderer_frames
        $screenshotFrameTarget = $screenshotStartFrame + 120
        $screenshotFrameAdvanced = $false
        $screenshotStableLength = -1L
        $screenshotStablePolls = 0
        $screenshotRequestedAt = Get-Date
        & $runnerPath -Action Press -EbootPath $ebootPath -PPSSPPPath $ppssppFullPath -Key Screenshot
        $screenshotDeadline = (Get-Date).AddSeconds(15)
        do {
            Start-Sleep -Milliseconds 100
            if (Test-Path -LiteralPath $screenshotDirectory -PathType Container) {
                $screenshotPath = Get-ChildItem -LiteralPath $screenshotDirectory -File |
                    Where-Object {
                        $_.Extension -in @('.jpg', '.png') -and
                        $_.LastWriteTime -ge $screenshotRequestedAt.AddSeconds(-1)
                    } |
                    Sort-Object LastWriteTime -Descending |
                    Select-Object -First 1 -ExpandProperty FullName
            }
            if ($screenshotPath) {
                $screenshotLength = (Get-Item -LiteralPath $screenshotPath).Length
                if ($screenshotLength -gt 0 -and $screenshotLength -eq $screenshotStableLength) {
                    $screenshotStablePolls++
                }
                else {
                    $screenshotStableLength = $screenshotLength
                    $screenshotStablePolls = 0
                }
            }
            $screenshotReport = Read-RuntimeReport $reportPath
            if ($screenshotReport.ContainsKey('renderer_frames')) {
                $screenshotFrameAdvanced = [int] $screenshotReport.renderer_frames -ge $screenshotFrameTarget
            }
        } while ((-not $screenshotPath -or -not $screenshotFrameAdvanced -or $screenshotStablePolls -lt 2) -and (Get-Date) -lt $screenshotDeadline)
        if (-not $screenshotPath -or -not $screenshotFrameAdvanced -or $screenshotStablePolls -lt 2) {
            throw 'PPSSPP did not complete the requested screenshot before menu input'
        }
        # A stable fresh file plus two later 60-frame runtime checkpoints keep
        # asynchronous framebuffer capture ahead of menu activation/fade.
    }
    if (-not $SkipMenuInput) {
        & $runnerPath -Action Press -EbootPath $ebootPath -PPSSPPPath $ppssppFullPath -Key DpadDown
        Start-Sleep -Milliseconds 350
        & $runnerPath -Action Press -EbootPath $ebootPath -PPSSPPPath $ppssppFullPath -Key DpadDown
        Start-Sleep -Milliseconds 350
        & $runnerPath -Action Press -EbootPath $ebootPath -PPSSPPPath $ppssppFullPath -Key Cross
        $optionsDeadline = (Get-Date).AddSeconds(10)
        do {
            Start-Sleep -Milliseconds 100
            $optionsReport = Read-RuntimeReport $reportPath
        } while ((!$optionsReport.ContainsKey('phase') -or $optionsReport.phase -ne 'frontend-options-ready') -and
            (Get-Date) -lt $optionsDeadline)
        if (-not $optionsReport.ContainsKey('phase') -or $optionsReport.phase -ne 'frontend-options-ready') {
            $actualPhase = if ($optionsReport.ContainsKey('phase')) { $optionsReport.phase } else { '<missing>' }
            throw "Original options state did not open: $actualPhase"
        }
    }

    $report = Read-RuntimeReport $reportPath
    $expectedPhase = if ($SkipMenuInput) { 'frontend-menu-ready' } else { 'frontend-options-ready' }
    if (-not $report.ContainsKey('phase') -or $report.phase -ne $expectedPhase) {
        $actualPhase = if ($report.ContainsKey('phase')) { $report.phase } else { '<missing>' }
        throw "Runtime report expected $expectedPhase but found $actualPhase"
    }
    Assert-ReportValue $report 'version' '22'
    foreach ($name in @(
        'engine',
        'application',
        'root',
        'gui2',
        'gui_system',
        'controls',
        'frontend_terrain_loaded',
        'frontend_terrain_pack_loaded',
        'frontend_grass_loaded',
        'frontend_glow_loaded',
        'frontend_barnnorth_loaded',
        'frontend_bgbarneast_loaded',
        'frontend_bgbarnwest_loaded',
        'frontend_hillbase_loaded',
        'frontend_hilltop_loaded',
        'frontend_meadowsouth_loaded',
        'frontend_instances_loaded',
        'frontend_camera_loaded',
        'frontend_barnsign_loaded'
    )) {
        Assert-ReportValue $report $name '1'
    }
    Assert-ReportValue $report 'frontend_terrain_meshes' '11'
    Assert-ReportValue $report 'frontend_terrain_pack_models' '36'
    Assert-ReportValue $report 'frontend_terrain_pack_textures' '34'
    Assert-ReportValue $report 'frontend_grass_models' '1'
    Assert-ReportValue $report 'frontend_grass_textures' '1'
    Assert-ReportValue $report 'frontend_glow_models' '3'
    Assert-ReportValue $report 'frontend_glow_textures' '1'
    Assert-ReportValue $report 'frontend_glow_draws' '0'
    Assert-ReportValue $report 'frontend_barnnorth_models' '55'
    Assert-ReportValue $report 'frontend_barnnorth_textures' '54'
    Assert-ReportValue $report 'frontend_bgbarneast_models' '7'
    Assert-ReportValue $report 'frontend_bgbarneast_textures' '7'
    Assert-ReportValue $report 'frontend_bgbarnwest_models' '5'
    Assert-ReportValue $report 'frontend_bgbarnwest_textures' '5'
    Assert-ReportValue $report 'frontend_hillbase_models' '7'
    Assert-ReportValue $report 'frontend_hillbase_textures' '7'
    Assert-ReportValue $report 'frontend_hilltop_models' '9'
    Assert-ReportValue $report 'frontend_hilltop_textures' '9'
    Assert-ReportValue $report 'frontend_meadowsouth_models' '15'
    Assert-ReportValue $report 'frontend_meadowsouth_textures' '15'
    Assert-ReportValue $report 'frontend_instance_packs' '7'
    Assert-ReportValue $report 'frontend_instances' '640'
    Assert-ReportValue $report 'frontend_instance_models' '41'
    Assert-ReportValue $report 'frontend_instance_textures' '24'
    Assert-ReportValue $report 'frontend_hour' '6.000000'
    Assert-ReportValue $report 'frontend_barnsign_meshes' '2'
    foreach ($name in @('renderer_frames', 'frontend_terrain_draws', 'frontend_terrain_pack_draws', 'frontend_grass_draws', 'frontend_barnnorth_draws', 'frontend_bgbarneast_draws', 'frontend_bgbarnwest_draws', 'frontend_hillbase_draws', 'frontend_hilltop_draws', 'frontend_meadowsouth_draws', 'frontend_instance_draws', 'frontend_barnsign_draws')) {
        Assert-ReportPositive $report $name
    }
    if (-not $SkipMenuInput) {
        foreach ($name in @('menu_command_down_count', 'menu_command_ok_count', 'menu_activation_count')) {
            Assert-ReportPositive $report $name
        }
        Assert-ReportValue $report 'menu_command_down_count' '2'
        Assert-ReportValue $report 'menu_activated_button' '2'
        Assert-ReportValue $report 'menu_activation_count' '1'
        Assert-ReportValue $report 'menu_close_count' '1'
        Assert-ReportValue $report 'menu_focus' '0'
    }

    if (-not $SkipScreenshot -and -not $screenshotPath) {
        throw 'PPSSPP did not write the requested screenshot'
    }

    [pscustomobject]@{
        Phase = $report.phase
        Frames = [int] $report.renderer_frames
        TerrainDraws = [int] $report.frontend_terrain_draws
        TerrainPackDraws = [int] $report.frontend_terrain_pack_draws
        GrassDraws = [int] $report.frontend_grass_draws
        GlowDraws = [int] $report.frontend_glow_draws
        BarnnorthDraws = [int] $report.frontend_barnnorth_draws
        BGbarneastDraws = [int] $report.frontend_bgbarneast_draws
        BGbarnwestDraws = [int] $report.frontend_bgbarnwest_draws
        HillbaseDraws = [int] $report.frontend_hillbase_draws
        HilltopDraws = [int] $report.frontend_hilltop_draws
        MeadowsouthDraws = [int] $report.frontend_meadowsouth_draws
        FrontendInstanceDraws = [int] $report.frontend_instance_draws
        BarnSignDraws = [int] $report.frontend_barnsign_draws
        HeapFree = [int] $report.heap_free
        Screenshot = $screenshotPath
        Report = $reportPath
    }
}
finally {
    & $runnerPath -Action Stop -EbootPath $ebootPath -PPSSPPPath $ppssppFullPath
}
