[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $RuntimeDirectory,
    [string] $PPSSPPPath = '',
    [switch] $Capture,
    [switch] $RequireMenuAudio,
    [switch] $RequireMenuMusic,
    [switch] $CheckNewGame,
    [switch] $CheckNewGameTerrain,
    [switch] $CheckNewGameOpening,
    [switch] $OpeningOnly,
    [switch] $CheckCharacterSetup,
    [switch] $CheckContinuation,
    [switch] $CheckMainGameStart,
    [switch] $CheckMainGameScene,
    [switch] $CheckMainGameInstances,
    [switch] $CheckMainGameMovement,
    [switch] $CheckMainGameCollision,
    [switch] $CheckMainGameCamera,
    [switch] $CheckMainGameRegions,
    [switch] $CheckMainGameCrossroads,
    [switch] $CheckMainGameCollisionStreaming,
    [switch] $CheckMainGameForest,
    [ValidateSet(0, 1)] [int] $PlayerGender = 1,
    [switch] $RequireOpeningSky,
    [switch] $RequireOpeningVoice,
    [switch] $RequireOpeningActors
)

$ErrorActionPreference = 'Stop'
$runtimePath = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$runner = Join-Path $PSScriptRoot '..\RunPPSSPP.ps1'
$eboot = Join-Path $runtimePath 'EBOOT.PBP'
$runnerArguments = @{ EbootPath = $eboot }
if ($PPSSPPPath) { $runnerArguments.PPSSPPPath = $PPSSPPPath }
$gameLog = Join-Path $runtimePath 'openbarnyard_psp_game.log'
$movieLog = Join-Path $runtimePath 'openbarnyard_psp_movies.log'
$terrainIndex = Join-Path $runtimePath 'Data\Terrain\SS01\terrain.ptld'
if (($CheckNewGameTerrain -or $CheckNewGameOpening) -and -not (Test-Path -LiteralPath $terrainIndex -PathType Leaf)) {
    throw "Stage the SS01 terrain before running this check. Missing index: $terrainIndex"
}
$openingConfig = Join-Path $runtimePath 'Data\ScriptedSequence\SS01\opening.psco'
$hasOpening = Test-Path -LiteralPath $openingConfig -PathType Leaf
if ($CheckNewGameOpening -and (-not $hasOpening -or $CheckNewGameTerrain)) {
    throw 'The opening check needs a staged opening camera and runs separately from the terrain residency check.'
}
if ($OpeningOnly -and (-not $CheckNewGameOpening -or $RequireMenuAudio -or $RequireMenuMusic)) {
    throw 'Use -OpeningOnly with -CheckNewGameOpening and the opening checks.'
}
$setupConfig = Join-Path $runtimePath 'Data\ScriptedSequence\SS01\opening-ui.psui'
if ($CheckCharacterSetup -and (-not $CheckNewGameOpening -or -not (Test-Path -LiteralPath $setupConfig -PathType Leaf))) {
    throw 'Character creation validation requires the staged original character creation camera and textures.'
}
$continuationConfig = Join-Path $runtimePath 'Data\ScriptedSequence\SS01\AfterCharacterSetup\opening.psco'
if ($CheckContinuation -and (-not $CheckCharacterSetup -or -not (Test-Path -LiteralPath $continuationConfig -PathType Leaf))) {
    throw 'Continuation validation requires -CheckCharacterSetup and the staged second SS01 narration.'
}
$mainGameIndex = Join-Path $runtimePath 'Data\Terrain\EnvMain\initial.pscl'
if ($CheckMainGameStart -and (-not $CheckContinuation -or -not (Test-Path -LiteralPath $mainGameIndex -PathType Leaf))) {
    throw 'The EnvMain start check requires -CheckContinuation and the staged original collision TRBs.'
}
$mainGameTerrain = Test-Path -LiteralPath (Join-Path $runtimePath 'Data\Terrain\EnvMain\Barn\terrain.ptld') -PathType Leaf
if ($CheckMainGameScene -and (-not $CheckMainGameStart -or -not $mainGameTerrain)) {
    throw 'The EnvMain scene check requires -CheckMainGameStart and the staged original EnvMain scene.'
}
if ($CheckMainGameInstances -and -not $CheckMainGameScene) {
    throw 'Use -CheckMainGameInstances with -CheckMainGameScene.'
}
if ($CheckMainGameMovement -and -not $CheckMainGameScene) {
    throw 'Use -CheckMainGameMovement with -CheckMainGameScene.'
}
if ($CheckMainGameCollision -and -not $CheckMainGameScene) {
    throw 'Use -CheckMainGameCollision with -CheckMainGameScene.'
}
if ($CheckMainGameCamera -and -not $CheckMainGameCollision) {
    throw 'Use -CheckMainGameCamera with -CheckMainGameCollision.'
}
if (($CheckMainGameRegions -or $CheckMainGameCrossroads) -and (-not $CheckMainGameScene -or
    -not (Test-Path -LiteralPath (Join-Path $runtimePath 'Data\Terrain\EnvMain\rows.psvs') -PathType Leaf))) {
    throw 'The region checks require -CheckMainGameScene and the staged VIS rows.'
}
if ($CheckMainGameCollisionStreaming -and (-not $CheckMainGameCrossroads -or
    -not (Test-Path -LiteralPath (Join-Path $runtimePath 'Data\Terrain\EnvMain\xroadeast\terrain.ptld') -PathType Leaf))) {
    throw 'Use -CheckMainGameCollisionStreaming with -CheckMainGameCrossroads and a staged xroadeast row.'
}
if ($CheckMainGameForest -and (-not $CheckMainGameCollisionStreaming -or
    -not (Test-Path -LiteralPath (Join-Path $runtimePath 'Data\Terrain\EnvMain\Forest\terrain.ptld') -PathType Leaf))) {
    throw 'Use -CheckMainGameForest with -CheckMainGameCollisionStreaming and a staged Forest row.'
}
if ($hasOpening -and -not $CheckNewGameOpening -and ($CheckNewGame -or $CheckNewGameTerrain)) {
    throw 'Use -CheckNewGameOpening for this staged runtime.'
}
if ($RequireOpeningSky -and (-not $CheckNewGameOpening -or -not (Test-Path -LiteralPath (Join-Path $runtimePath 'Data\ScriptedSequence\SS01\sky.psky') -PathType Leaf))) {
    throw 'The sky check needs the opening check and a staged sky.'
}
if ($RequireOpeningVoice -and (-not $CheckNewGameOpening -or -not (Test-Path -LiteralPath (Join-Path $runtimePath 'Data\ScriptedSequence\SS01\Voice') -PathType Container))) {
    throw 'The voice check needs the opening check and a staged voice cache.'
}
$actorConfig = Join-Path $runtimePath 'Data\ScriptedSequence\SS01\Actors\actors.psac'
if ($RequireOpeningActors -and (-not $CheckNewGameOpening -or -not (Test-Path -LiteralPath $actorConfig -PathType Leaf))) {
    throw 'The actor check needs the opening check and a staged actor cache.'
}
if (Get-Process -Name PPSSPPWindows64, PPSSPPWindows -ErrorAction SilentlyContinue) {
    throw 'An existing PPSSPP process is running.'
}
$startedAt = Get-Date
$lastKey = ''

function Send-Key([string] $Key) {
    # Allow the game to sample a released button before a second press.
    if ($lastKey -eq $Key -and $Key -ne 'Screenshot') { Start-Sleep -Milliseconds 150 }
    & $runner @runnerArguments -Action Press -Key $Key | Write-Host
    $script:lastKey = $Key
}

function Read-GameLog {
    $status = @{}
    if ((Test-Path -LiteralPath $gameLog -PathType Leaf) -and (Get-Item -LiteralPath $gameLog).LastWriteTime -ge $startedAt) {
        foreach ($line in (Get-Content -LiteralPath $gameLog -ErrorAction SilentlyContinue)) {
            if ($line -match '^([^=]+)=(.*)$') { $status[$Matches[1]] = $Matches[2] }
        }
    }
    return $status
}

function Wait-Menu([string] $Phase, [int] $Focus, [hashtable] $Expected = @{}, [int] $TimeoutSeconds = 10, [int] $AfterSimFrame = -1) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $status = Read-GameLog
        if ($status.phase -match '(?:data-failed|widgets-missing|script-failed|terrain-failed|opening-failed|sim-start-failed|sim-world-failed)$') { throw "Game resource failure: $($status.failed_path)" }
        $matching = $status.phase -eq $Phase -and [int]$status.sim_frames -gt $AfterSimFrame
        if ($Focus -ge 0) {
            $matching = $matching -and $status.menu_state -eq '5' -and $status.menu_focus -eq "$Focus"
        }
        else {
            $matching = $matching -and [int]$status.new_game_opening_frames -gt 0 -and [int]$status.new_game_opening_draws -gt 0
        }
        foreach ($key in $Expected.Keys) { $matching = $matching -and $status[$key] -eq [string]$Expected[$key] }
        if ($matching) { return $status }
        Start-Sleep -Milliseconds 150
    } while ((Get-Date) -lt $deadline)
    throw "Expected $Phase focus $Focus. Current phase $($status.phase), state $($status.menu_state), focus $($status.menu_focus), effects $($status.option_effects), inverted $($status.option_inverted), sensitivity $($status.option_sensitivity). See $gameLog"
}

function Convert-SimPosition([string] $Value) {
    $position = $Value -split ','
    if ($position.Count -ne 3) { throw "Invalid simulation position '$Value'. See $gameLog" }
    foreach ($axis in $position) {
        Convert-SimNumber $axis
    }
}

function Test-MainGamePause {
    Send-Key Start
    $paused = Wait-Menu 'sim-paused' 0
    if ($Capture) { Send-Key Screenshot }
    $frozen = @{
        sim_updates = $paused.sim_updates
        sim_player_position = $paused.sim_player_position
        sim_camera_position = $paused.sim_camera_position
        sim_terrain_row = $paused.sim_terrain_row
    }
    & $runner @runnerArguments -Action Press -Key AnalogUp -AnalogHoldMilliseconds 500 | Write-Host
    $afterStick = Wait-Menu 'sim-paused' -1 $frozen 15 ([int]$paused.sim_frames)
    # The original dialog map also lets the stick move the menu focus.
    if ($afterStick.menu_focus -eq '2') {
        Send-Key DpadUp
        $null = Wait-Menu 'sim-paused' 0 $frozen
    } elseif ($afterStick.menu_focus -eq '4') {
        Send-Key DpadDown
        $null = Wait-Menu 'sim-paused' 0 $frozen
    }
    Send-Key Cross
    $resumed = Wait-Menu 'sim-world-ready' -1 @{ sim_player_position = $paused.sim_player_position }
    $deadline = (Get-Date).AddSeconds(15)
    while ([int]$resumed.sim_updates -le [int]$paused.sim_updates -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 150
        $resumed = Read-GameLog
    }
    if ([int]$resumed.sim_updates -le [int]$paused.sim_updates) {
        throw "The world did not start updating after Resume. See $gameLog"
    }

    Send-Key Start
    $paused = Wait-Menu 'sim-paused' 0
    Send-Key DpadDown
    $null = Wait-Menu 'sim-paused' 2
    Send-Key Cross
    $null = Wait-Menu 'frontend-options-ready' 0 @{ sim_updates = $paused.sim_updates }
    Send-Key Triangle
    $null = Wait-Menu 'sim-paused' 2 @{ sim_updates = $paused.sim_updates }
    Send-Key Start
    $null = Wait-Menu 'sim-world-ready' -1

    Send-Key Start
    $paused = Wait-Menu 'sim-paused' 0
    Send-Key DpadDown
    $null = Wait-Menu 'sim-paused' 2
    Send-Key DpadDown
    $null = Wait-Menu 'sim-paused' 4
    Send-Key Cross
    $null = Wait-Menu 'sim-pause-confirm-quit' 1 @{ sim_updates = $paused.sim_updates }
    if ($Capture) { Send-Key Screenshot }
    Send-Key Cross
    $null = Wait-Menu 'sim-paused' 4 @{ sim_updates = $paused.sim_updates }
    Send-Key Cross
    $null = Wait-Menu 'sim-pause-confirm-quit' 1
    Send-Key DpadLeft
    $null = Wait-Menu 'sim-pause-confirm-quit' 0
    Send-Key Cross
    'Original pause froze EnvMain and preserved its position through Resume, Options and the default No response. Confirmed Quit with Yes.'
}

function Convert-SimNumber([string] $Value) {
    [double]::Parse($Value, [Globalization.CultureInfo]::InvariantCulture)
}

function Test-MainGameMovement([hashtable] $Scene) {
    if ($Scene.sim_player_steering -ne '1' -or $Scene.sim_player_motion -ne '0') {
        throw "Expected the original player steering data and an idle avatar. See $gameLog"
    }
    $spawn = Convert-SimPosition $Scene.sim_player_position
    $cameraStart = Convert-SimPosition $Scene.sim_camera_position
    & $runner @runnerArguments -Action Press -Key AnalogUp -AnalogMagnitude 0.5 -AnalogHoldMilliseconds 2000 | Write-Host
    $walking = Wait-Menu 'sim-world-ready' -1 @{ sim_player_motion = 0; sim_player_motion_mask = 3; sim_player_speed = '0.0000' } 15
    $walkEnd = Convert-SimPosition $walking.sim_player_position
    $walkDistance = $walkEnd[2] - $spawn[2]
    if ($walkDistance -lt 0.4) { throw "The half-tilt stick moved only $walkDistance metres. See $gameLog" }

    & $runner @runnerArguments -Action Press -Key AnalogUp -AnalogHoldMilliseconds 2000 | Write-Host
    $running = Wait-Menu 'sim-world-ready' -1 @{ sim_player_motion = 0; sim_player_motion_mask = 7; sim_player_speed = '0.0000' } 15
    $runEnd = Convert-SimPosition $running.sim_player_position
    $runDistance = $runEnd[2] - $walkEnd[2]
    $cameraEnd = Convert-SimPosition $running.sim_camera_position
    if ($runDistance -le $walkDistance -or $cameraEnd[2] -le $cameraStart[2] + $walkDistance) {
        throw "Expected full-tilt movement and the following gameplay camera. See $gameLog"
    }
    if ([Math]::Abs($runEnd[1] - $spawn[1]) -gt 0.75) {
        throw "The player left the starting yard surface. See $gameLog"
    }
    if ($Capture) { Send-Key Screenshot }
    $deadline = (Get-Date).AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 150
        $settled = Read-GameLog
    } while ([int]$settled.sim_frames -le [int]$running.sim_frames -and (Get-Date) -lt $deadline)
    if ($settled.sim_player_position -ne $running.sim_player_position -or [int]$settled.sim_frames -le [int]$running.sim_frames) {
        throw "The avatar did not remain idle after releasing the analog stick. See $gameLog"
    }
    "Original EnvMain walked {0:F2} m, ran {1:F2} m, and returned to Idle at ({2})." -f $walkDistance, $runDistance, $settled.sim_player_position
}

function Send-SimAnalog([hashtable] $Before, [string] $Key, [int] $HoldMilliseconds, [double] $AnalogX = 0.0, [double] $AnalogY = 0.0) {
    & $runner @runnerArguments -Action Press -Key $Key -AnalogHoldMilliseconds $HoldMilliseconds -AnalogX $AnalogX -AnalogY $AnalogY | Write-Host
    $idle = @{ sim_player_motion = 0; sim_player_speed = '0.0000' }
    Wait-Menu 'sim-world-ready' -1 $idle 15 ([int]$Before.sim_frames)
}

function Test-MainGameCollision([hashtable] $Scene) {
    if ($Scene.sim_player_steering -ne '1' -or -not $Scene.ContainsKey('sim_terrain_contacts')) {
        throw "Expected player steering and terrain contact reporting. See $gameLog"
    }
    $blocked = $Scene
    # Barn_col contains the wall behind the original Barn spawn, near world Z -41.8.
    for ($press = 0; $press -lt 4 -and [uint32]$blocked.sim_terrain_contacts -eq [uint32]$Scene.sim_terrain_contacts; $press++) {
        $blocked = Send-SimAnalog $blocked AnalogDown 5000
    }
    $wallPosition = Convert-SimPosition $blocked.sim_player_position
    if ([uint32]$blocked.sim_terrain_contacts -le [uint32]$Scene.sim_terrain_contacts -or $wallPosition[2] -lt -41.65 -or $wallPosition[2] -gt -39.8) {
        throw "Expected the original Barn wall to stop the player. Observed ($($blocked.sim_player_position)), contacts $($blocked.sim_terrain_contacts). See $gameLog"
    }
    $pressed = Send-SimAnalog $blocked AnalogDown 1800
    $pressedPosition = Convert-SimPosition $pressed.sim_player_position
    if ([uint32]$pressed.sim_terrain_contacts -le [uint32]$blocked.sim_terrain_contacts -or [Math]::Abs($pressedPosition[2] - $wallPosition[2]) -gt 0.3) {
        throw "The player did not stay against the wall under continued input. See $gameLog"
    }
    $slid = Send-SimAnalog $pressed AnalogDownRight 1700
    $slidePosition = Convert-SimPosition $slid.sim_player_position
    $sideDistance = $slidePosition[0] - $pressedPosition[0]
    if ([uint32]$slid.sim_terrain_slides -le [uint32]$pressed.sim_terrain_slides -or $sideDistance -lt 0.25 -or $slidePosition[2] -lt -41.65) {
        throw "Expected lateral movement along the outside of the wall. Observed ($($slid.sim_player_position)), slides $($slid.sim_terrain_slides). See $gameLog"
    }
    if ($Capture) { Send-Key Screenshot }
    "Original Barn wall stopped the player at Z {0:F2}. The diagonal input slid {1:F2} m along the wall." -f $pressedPosition[2], $sideDistance
    if ($CheckMainGameCamera) { Test-MainGameCamera $slid }
}

function Test-MainGameCamera([hashtable] $Wall) {
    $distance = Convert-SimNumber $Wall.sim_camera_distance
    $cameraAtWall = Convert-SimPosition $Wall.sim_camera_position
    if ($distance -gt 2.5 -or $cameraAtWall[2] -lt -41.65) {
        throw "Expected the camera to stay outside the original wall. Observed ($($Wall.sim_camera_position)), distance $distance. See $gameLog"
    }
    $recovered = $Wall
    for ($press = 0; $press -lt 3 -and (Convert-SimNumber $recovered.sim_camera_distance) -lt 4.95; $press++) {
        $recovered = Send-SimAnalog $recovered AnalogUp 4500
    }
    $openDistance = Convert-SimNumber $recovered.sim_camera_distance
    $openPosition = Convert-SimPosition $recovered.sim_player_position
    $wallPosition = Convert-SimPosition $Wall.sim_player_position
    if ($openDistance -lt 4.95 -or $openPosition[2] -le $wallPosition[2] + 3.0) {
        throw "The camera did not recover its open-yard distance. Observed ($($recovered.sim_camera_position)), distance $openDistance. See $gameLog"
    }
    if ($Capture) { Send-Key Screenshot }
    "The original wall shortened the camera orbit to {0:F2} m. The camera returned to {1:F2} m in the open yard." -f $distance, $openDistance
}

function Move-SimTowards([hashtable] $Before, [double] $TargetX, [double] $TargetZ, [string] $TargetRow = '') {
    $status = $Before
    for ($press = 0; $press -lt 18; $press++) {
        if ($TargetRow -and $status.sim_terrain_row -ceq $TargetRow) { return $status }
        $position = Convert-SimPosition $status.sim_player_position
        $camera = Convert-SimPosition $status.sim_camera_position
        $dx = $TargetX - $position[0]
        $dz = $TargetZ - $position[2]
        $distance = [Math]::Sqrt($dx * $dx + $dz * $dz)
        if ($distance -lt 0.65) {
            if (-not $TargetRow) { return $status }
            break
        }
        $fx = $position[0] - $camera[0]
        $fz = $position[2] - $camera[2]
        $length = [Math]::Sqrt($fx * $fx + $fz * $fz)
        $denom = $length * $distance
        $x = [Math]::Min(1.0, [Math]::Max(-1.0, ($dx * $fz - $dz * $fx) / $denom))
        $y = [Math]::Min(1.0, [Math]::Max(-1.0, ($dx * $fx + $dz * $fz) / $denom))
        $hold = [int][Math]::Min(4500.0, [Math]::Max(500.0, 950.0 * $distance))
        $status = Send-SimAnalog $status AnalogVector $hold $x $y
        if ($status.sim_terrain_swap_failures -ne '0') { throw "The terrain row swap failed. See $gameLog" }
    }
    if ($TargetRow -and $status.sim_terrain_row -ceq $TargetRow) { return $status }
    $destination = if ($TargetRow) { $TargetRow } else { "($TargetX, $TargetZ)" }
    throw "Expected to reach $destination. Current row $($status.sim_terrain_row) at ($($status.sim_player_position)). See $gameLog"
}

function Test-MainGameRegions([hashtable] $Scene) {
    if ($Scene.sim_terrain_row -cne 'Barn' -or $Scene.sim_terrain_collision_mask -ne '3f') {
        throw "Expected the original Barn VIS row and all six initial colliders. See $gameLog"
    }
    # The original northbound path crosses the Barn/barnnorth seam around X 19.
    $north = Move-SimTowards $Scene 19.25 -3.0 barnnorth
    $northMeshes = if ($CheckMainGameInstances) { 358 } else { 303 }
    if ($north.sim_terrain_collision_mask -ne '03' -or [int]$north.sim_terrain_meshes -ne $northMeshes -or [int]$north.sim_terrain_draws -le 0) {
        throw "The original barnnorth row did not activate its two high collision cells and render pack. See $gameLog"
    }
    if ($Capture) { Send-Key Screenshot }
    "Reached barnnorth at ($($north.sim_player_position)), camera ($($north.sim_camera_position)), $($north.sim_terrain_meshes) meshes."
    $returned = Move-SimTowards $north 19.25 -24.0 Barn
    if ($returned.sim_terrain_collision_mask -ne '3f' -or
        $returned.sim_terrain_meshes -ne $Scene.sim_terrain_meshes -or
        [int]$returned.render_main_peak -gt [int]$returned.render_main_budget) {
        throw "Returning to Barn did not restore the original pack within the render budget. See $gameLog"
    }
    if ($Capture) { Send-Key Screenshot }
    "Returned to Barn at ($($returned.sim_player_position)), restored $($returned.sim_terrain_meshes) meshes and completed $($returned.sim_terrain_swaps) row swaps."
}

function Test-MainGameCrossroads([hashtable] $Scene) {
    if ($Scene.sim_terrain_row -cne 'Barn') { throw "Expected to enter the original road from Barn. See $gameLog" }
    # Approach the original road entrance from the southern side of the yard.
    $south = Move-SimTowards $Scene 19.25 -36.75
    $east = Move-SimTowards $south 34.5 -37.0
    $road = Move-SimTowards $east 56.0 -48.5 xroad
    $roadMeshes = if ($CheckMainGameInstances) { 387 } else { 335 }
    if ($road.sim_terrain_collision_mask -ne '3d' -or [int]$road.sim_terrain_meshes -ne $roadMeshes -or
        [int]$road.sim_terrain_draws -le 0 -or [int]$road.render_main_peak -gt [int]$road.render_main_budget) {
        throw "The xroad row did not activate its five original high collision cells and render pack. See $gameLog"
    }
    if ($Capture) { Send-Key Screenshot }
    "Reached xroad at ($($road.sim_player_position)), camera ($($road.sim_camera_position)), $($road.sim_terrain_meshes) meshes."
    if ($CheckMainGameCollisionStreaming) { Test-MainGameCollisionStreaming $road }
}

function Test-MainGameCollisionStreaming([hashtable] $Road) {
    $roadBytes = [int]$Road.sim_collision_resident_bytes
    if ([int]$Road.sim_collision_resident_packs -ne 5 -or $roadBytes -le 0) {
        throw "Expected the five original xroad collision files to be resident. See $gameLog"
    }
    $junction = Move-SimTowards $Road 65.5 -47.0
    $bend = Move-SimTowards $junction 67.5 -39.5
    $east = Move-SimTowards $bend 72.0 -30.0 xroadeast
    $eastBytes = [int]$east.sim_collision_resident_bytes
    $eastMeshes = if ($CheckMainGameInstances) { 373 } else { 326 }
    $roadMask = [Convert]::ToUInt32($Road.sim_terrain_collision_mask, 16)
    $eastMask = [Convert]::ToUInt32($east.sim_terrain_collision_mask, 16)
    if (($eastMask -band $roadMask) -ne $roadMask -or [int]$east.sim_collision_resident_packs -ne 6 -or
        $eastBytes - $roadBytes -ne 24256 -or [int]$east.sim_terrain_meshes -ne $eastMeshes) {
        throw "xroadeast did not stream the extra original Forest collider and its matching render pack. See $gameLog"
    }
    if ($Capture) { Send-Key Screenshot }
    "xroadeast streamed Forest at ($($east.sim_player_position)), $eastBytes resident collision bytes in six files."
    if ($CheckMainGameForest) {
        Test-MainGameForest $east
        return
    }
    $bend = Move-SimTowards $east 67.5 -39.5
    $junction = Move-SimTowards $bend 65.5 -47.0
    $returned = Move-SimTowards $junction 59.0 -48.0 xroad
    if ($returned.sim_terrain_collision_mask -ne $Road.sim_terrain_collision_mask -or
        [int]$returned.sim_collision_resident_packs -ne 5 -or [int]$returned.sim_collision_resident_bytes -ne $roadBytes -or
        [int]$returned.sim_terrain_swaps -lt [int]$Road.sim_terrain_swaps + 2 -or
        [int]$returned.render_main_peak -gt [int]$returned.render_main_budget) {
        throw "Returning to xroad did not unload Forest and restore the original collision set. See $gameLog"
    }
    if ($Capture) { Send-Key Screenshot }
    "xroad unloaded Forest and returned to $roadBytes resident collision bytes. Peak was $($returned.sim_collision_peak_bytes) bytes."
}

function Test-MainGameForest([hashtable] $East) {
    # Original xroadeast track curves around the hill before crossing into Forest.
    $status = $East
    $route = @(
        @{ X = 78.0; Z = -35.0 }
        @{ X = 88.0; Z = -35.0 }
        @{ X = 91.5; Z = -33.0 }
        @{ X = 94.5; Z = -30.5 }
        @{ X = 96.0; Z = -27.0 }
        @{ X = 96.0; Z = -14.0 }
        @{ X = 93.0; Z = -8.0 }
        @{ X = 88.0; Z = -1.0 }
        @{ X = 86.5; Z = 4.0 }
    )
    foreach ($point in $route) {
        if ($status.sim_terrain_row -ceq 'Forest') { break }
        $status = Move-SimTowards $status $point.X $point.Z
    }
    $forest = Move-SimTowards $status 88.0 13.0 Forest
    $expectedMeshes = if ($CheckMainGameInstances) { 304 } else { 249 }
    if ([int]$forest.sim_collision_resident_packs -ne 4 -or [int]$forest.sim_collision_resident_bytes -ne 180704 -or
        [int]$forest.sim_terrain_swaps -le [int]$East.sim_terrain_swaps -or [int]$forest.sim_terrain_meshes -ne $expectedMeshes -or
        [int]$forest.sim_terrain_draws -le 0 -or [int]$forest.render_main_peak -gt [int]$forest.render_main_budget) {
        throw "Forest did not load its visual pack and original four-cell collision set. See $gameLog"
    }
    if ($Capture) { Send-Key Screenshot }
    "Reached Forest at ($($forest.sim_player_position)), $($forest.sim_terrain_meshes) meshes and $($forest.sim_collision_resident_bytes) resident collision bytes in four files."
}

function Wait-MenuCue([int] $Cue, [hashtable] $Before) {
    $deadline = (Get-Date).AddSeconds(8)
    do {
        $status = Read-GameLog
        if ($status.menu_audio_last_cue -eq "$Cue" -and
            [int]$status.menu_audio_cues -gt [int]$Before.menu_audio_cues -and
            [int]$status.menu_audio_non_silent_blocks -gt [int]$Before.menu_audio_non_silent_blocks) {
            "Original cue $Cue queued audio. Mixed non-silent blocks: $($status.menu_audio_non_silent_blocks)"
            return
        }
        Start-Sleep -Milliseconds 150
    } while ((Get-Date) -lt $deadline)
    throw "Failed to verify menu cue $Cue. Last cue $($status.menu_audio_last_cue), accepted $($status.menu_audio_cues), non-silent blocks $($status.menu_audio_non_silent_blocks). See $gameLog"
}

function Wait-MenuMusic([int] $MinimumLoops, [int] $Volume = -1, [int] $TimeoutSeconds = 8) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $status = Read-GameLog
        if ($status.menu_music_ready -eq '0' -or [int]$status.menu_music_underruns -ne 0) {
            throw "Music bank ready: $($status.menu_music_ready). Stream underruns: $($status.menu_music_underruns). See $gameLog"
        }
        $gainMatches = $Volume -lt 0 -or [Math]::Abs([int]$status.menu_music_gain_per_mille - $Volume * 1000.0 / 255) -le 1
        if ($status.menu_music_ready -eq '1' -and $gainMatches -and [int]$status.menu_music_loops -ge $MinimumLoops -and
            [int]$status.menu_music_position_ms -gt 0 -and [int]$status.menu_audio_non_silent_blocks -gt 0) {
            return $status
        }
        Start-Sleep -Milliseconds 150
    } while ((Get-Date) -lt $deadline)
    throw "Expected an active music stream, loops >= $MinimumLoops, volume $Volume. Current position $($status.menu_music_position_ms) ms, loops $($status.menu_music_loops), gain $($status.menu_music_gain_per_mille) per mille. See $gameLog"
}

function Assert-MenuMusicProgress([hashtable] $Before, [hashtable] $After) {
    $loopCountBefore = [int]$Before.menu_music_loops
    $loopCountAfter = [int]$After.menu_music_loops
    if ($loopCountAfter -lt $loopCountBefore -or
        ($loopCountAfter -eq $loopCountBefore -and
            [int]$After.menu_music_position_ms -le [int]$Before.menu_music_position_ms)) {
        throw "Music stopped or restarted between positions $($Before.menu_music_position_ms) and $($After.menu_music_position_ms) ms. See $gameLog"
    }
}

try {
    & $runner @runnerArguments -Action Start
    foreach ($name in @('NickMovs', 'THQ', 'BTE', 'Attract')) {
        $deadline = (Get-Date).AddSeconds(75)
        do {
            if ((Test-Path -LiteralPath $movieLog -PathType Leaf) -and (Get-Item -LiteralPath $movieLog).LastWriteTime -ge $startedAt -and
                (Select-String -Quiet -LiteralPath $movieLog -Pattern "/$name\.pmvf result=started ")) { break }
            if ((Get-Date) -ge $deadline) {
                $status = Read-GameLog
                throw "Waiting for $name timed out at phase $($status.phase), frame $($status.renderer_frames). See $gameLog"
            }
            Start-Sleep -Milliseconds 150
        } while ($true)
        Start-Sleep -Milliseconds 1500
        Send-Key Cross
    }

    $main = Wait-Menu 'frontend-menu-ready' 0
    if ($OpeningOnly -and $Capture) {
        Start-Sleep -Milliseconds 750
        Send-Key Screenshot
    }
    if ($CheckNewGame -or $CheckNewGameTerrain -or $CheckNewGameOpening) {
        Send-Key Cross
        $null = Wait-Menu 'frontend-new-game-ready' 0 @{ new_game_requested_slot = -1 }
        foreach ($slot in 1..3) {
            Send-Key DpadDown
            $null = Wait-Menu 'frontend-new-game-ready' $slot
        }
        if ($Capture -and -not $CheckNewGameOpening) { Send-Key Screenshot }
        Send-Key Cross
        $expected = @{ new_game_requested_slot = 3; new_game_script_tracks = 21 }
        if ($CheckNewGameTerrain -or $CheckNewGameOpening) {
            $index = [IO.File]::ReadAllBytes($terrainIndex)
            $expected.new_game_terrain_meshes = [BitConverter]::ToUInt32($index, 8)
            $expected.new_game_terrain_textures = [BitConverter]::ToUInt32($index, 12)
        }
        if ($CheckNewGameTerrain) { $expected.new_game_terrain_released = 1 }
        if ($CheckNewGameOpening) {
            $newGame = Wait-Menu 'frontend-new-game-opening' -1 $expected 90
            if ($RequireOpeningSky -and [int]$newGame.new_game_sky_draws -ne 2 * [int]$newGame.new_game_opening_frames) {
                throw "The original opening sky did not render both layers. See $gameLog"
            }
            if ($RequireOpeningActors) {
                if ($newGame.new_game_actor_draws_shot1 -ne '2') {
                    throw "The first shot did not draw both original birds. See $gameLog"
                }
            }
            if ($Capture) {
                Send-Key Screenshot
                $shotCaptureDelay = if ($RequireOpeningActors) { 2000 } else { 750 }
                Start-Sleep -Milliseconds $shotCaptureDelay
                Send-Key Screenshot
            }
            "Rendered the original SS01 opening ($($newGame.new_game_opening_draws) draws across $($newGame.new_game_opening_frames) frames)."
            if ($RequireOpeningVoice -or $RequireOpeningActors) {
                $timeline = [IO.File]::ReadAllBytes($openingConfig)
                $shots = if ($timeline.Length -ge 16) { [BitConverter]::ToUInt32($timeline, 8) } else { 0 }
                if ($shots -lt 2 -or $shots -gt 16 -or [BitConverter]::ToUInt32($timeline, 4) -ne 2 -or
                    $timeline.Length -ne 16 + 12 * $shots) {
                    throw 'Voice and actor checks need a multi-shot SS01 timeline.'
                }
                $seconds = [double]0
                for ($shot = 1; $shot -le $shots; $shot++) {
                    $seconds += [BitConverter]::ToSingle($timeline, 12 + 12 * $shot)
                }
                $endMs = [int](1000 * $seconds)
                $firstCutMs = [int](1000 * [BitConverter]::ToSingle($timeline, 24))
                $expected.new_game_opening_shot = 2
                $secondShot = Wait-Menu 'frontend-new-game-opening' -1 $expected 90
                if ($RequireOpeningVoice -and ([int]$secondShot.new_game_voice_ms -lt $firstCutMs -or
                    [int]$secondShot.new_game_voice_underruns -ne 0 -or [int]$secondShot.new_game_voice_non_silent_blocks -lt 40)) {
                    throw "The second shot did not follow the SS01 voice at $firstCutMs ms. See $gameLog"
                }
                if ($Capture) {
                    Start-Sleep -Milliseconds 1500
                    Send-Key Screenshot
                    if ($shots -ge 4) {
                        $expected.new_game_opening_shot = 4
                        $null = Wait-Menu 'frontend-new-game-opening' -1 $expected 25
                        Send-Key Screenshot
                    }
                    elseif ($shots -eq 3) {
                        $expected.new_game_opening_shot = 3
                        $null = Wait-Menu 'frontend-new-game-opening' -1 $expected 25
                        Start-Sleep -Milliseconds 1500
                        Send-Key Screenshot
                    }
                }
                $expected.new_game_opening_shot = $shots
                $lastShot = Wait-Menu 'frontend-new-game-opening' -1 $expected 120
                if ($RequireOpeningActors) {
                    $actorData = [IO.File]::ReadAllBytes($actorConfig)
                    $actorVersion = [BitConverter]::ToUInt32($actorData, 4)
                    $actorCount = [BitConverter]::ToUInt32($actorData, 8)
                    $actorRecordSize = if ($actorVersion -eq 1) { 20 } else { 24 }
                    if ($actorData.Length -ne 16 + $actorRecordSize * $actorCount) {
                        throw 'Invalid staged actor index.'
                    }
                    $meshDraws = if ($lastShot.new_game_actor_draws_by_shot) {
                        $lastShot.new_game_actor_draws_by_shot.Split(',')
                    } else {
                        @($lastShot.new_game_actor_draws_shot1, $lastShot.new_game_actor_draws_shot2, $lastShot.new_game_actor_draws_shot3)
                    }
                    for ($shot = 0; $shot -lt $shots; $shot++) {
                        $count = 0
                        for ($actor = 0; $actor -lt $actorCount; $actor++) {
                            $record = 16 + $actor * $actorRecordSize
                            if ([BitConverter]::ToUInt32($actorData, $record + 4) -band (1 -shl $shot)) {
                                $count += [BitConverter]::ToUInt32($actorData, $record + 8)
                            }
                        }
                        if ([int]$meshDraws[$shot] -ne $count) {
                            throw "Shot $($shot + 1) drew $($meshDraws[$shot]) actor meshes, expected $count. See $gameLog"
                        }
                    }
                    "Actor mesh draws across ${shots} shots: $($meshDraws[0..($shots - 1)] -join ', '). Peak animated vertices: $($lastShot.animation_peak_vertices)."
                }
            }
            if ($RequireOpeningVoice) {
                $deadline = (Get-Date).AddSeconds(45)
                $terminal = $null
                do {
                    $now = Read-GameLog
                    if (($now.phase -eq 'frontend-new-game-opening' -or
                         ($CheckCharacterSetup -and $now.phase -eq 'frontend-new-game-character-setup')) -and
                        [int]$now.new_game_voice_ms -ge $endMs) {
                        if (-not $terminal) { $terminal = $now }
                        elseif ([int]$now.new_game_opening_frames -gt [int]$terminal.new_game_opening_frames) { break }
                    }
                    if ((Get-Date) -ge $deadline) { throw "SS01 did not hold after reaching $endMs ms. See $gameLog" }
                    Start-Sleep -Milliseconds 150
                } while ($true)
                if ([int]$now.new_game_voice_ms -gt $endMs + 100 -or
                    $now.new_game_voice_ms -ne $terminal.new_game_voice_ms -or
                    $now.new_game_voice_non_silent_blocks -ne $terminal.new_game_voice_non_silent_blocks -or
                    [int]$now.new_game_voice_underruns -ne 0) {
                    throw "Narration did not stop at the end of the opening preview. See $gameLog"
                }
                "Narration stopped at $($now.new_game_voice_ms) ms while the last pose held."
            }
        }
        else {
            $timeout = if ($CheckNewGameTerrain) { 90 } else { 10 }
            $newGame = Wait-Menu 'frontend-new-game-awaiting-sequence' 3 $expected $timeout
        }
        if ($CheckNewGameTerrain) {
            if ([int]$newGame.new_game_terrain_bytes -le 0 -or [int]$newGame.new_game_terrain_heap_free -le 0) {
                throw "No SS01 memory measurement in $gameLog"
            }
            foreach ($key in $main.Keys) {
                if ($key -match '^frontend_.*_(loaded|models|textures|meshes|instances|packs)$' -and $newGame[$key] -ne $main[$key]) {
                    throw "Frontend resource count $key differs after the SS01 round trip. See $gameLog"
                }
            }
            "Loaded SS01 terrain: $($newGame.new_game_terrain_meshes) meshes, $($newGame.new_game_terrain_textures) textures, $($newGame.new_game_terrain_bytes) render bytes, $($newGame.new_game_terrain_heap_free) heap bytes free. Restored the menu."
        }
        if ($CheckCharacterSetup) {
            $setupPhase = 'frontend-new-game-character-setup'
            $null = Wait-Menu $setupPhase -1 @{ new_game_player_page = 0; new_game_player_gender = 0 } 30
            if ($Capture) { Send-Key Screenshot }
            if ($PlayerGender -eq 1) {
                Send-Key DpadDown
                $null = Wait-Menu $setupPhase -1 @{ new_game_player_page = 0; new_game_player_gender = 1 }
            }
            Send-Key Cross
            $skinPage = @{ new_game_player_page = 1; new_game_player_gender = $PlayerGender }
            if ($RequireOpeningVoice) { $skinPage.new_game_player_voice = 47 }
            $null = Wait-Menu $setupPhase -1 $skinPage
            foreach ($costume in 1, 2, 5) {
                Send-Key DpadRight
                $null = Wait-Menu $setupPhase -1 @{ new_game_player_page = 1; new_game_player_costume = $costume }
            }
            if ($Capture) { Send-Key Screenshot }
            Send-Key Cross
            $namePage = @{ new_game_player_page = 2 }
            if ($RequireOpeningVoice) { $namePage.new_game_player_voice = 48 }
            $null = Wait-Menu $setupPhase -1 $namePage
            if ($Capture) { Send-Key Screenshot }
            if ($RequireOpeningVoice) {
                $null = Wait-Menu $setupPhase -1 @{ new_game_player_page = 2; new_game_player_voice = 49 }
            }
            Send-Key DpadLeft
            Send-Key DpadLeft
            Send-Key Cross
            Send-Key DpadRight
            Send-Key DpadRight
            Send-Key Cross
            $profilePhase = if ($CheckContinuation) { 'frontend-new-game-continuation' } else { 'frontend-new-game-player-ready' }
            $profile = Wait-Menu $profilePhase -1 @{
                new_game_player_accepted = 1; new_game_player_gender = $PlayerGender; new_game_player_costume = 5
            }
            if ([int]$profile.new_game_player_name_length -ne 1) {
                throw "Expected one localized letter in the accepted player name. See $gameLog"
            }
            "The original character creation screens accepted gender $PlayerGender, costume index 5 and an edited name."
            if ($CheckContinuation) {
                $firstShot = Wait-Menu $profilePhase -1 @{ new_game_continuation_shot = 1; new_game_beady_int_day = 0 }
                if ($Capture) { Send-Key Screenshot }
                $completePhase = if ($CheckMainGameScene) { 'sim-world-ready' } else { 'frontend-new-game-sequence-complete' }
                $complete = Wait-Menu $completePhase -1 @{
                    new_game_continuation_shot = 13; new_game_beady_int_day = 1
                    new_game_terrain_meshes = 54; new_game_terrain_textures = 46
                } 65
                $expectedDraws = '8,2,2,2,3,8,3,5,1,3,2,2,6'
                $draws = ($complete.new_game_continuation_actor_draws_by_shot -split ',' | Select-Object -First 13) -join ','
                if ($draws -ne $expectedDraws -or [int]$complete.new_game_continuation_voice_ms -lt 35000 -or
                    [int]$complete.new_game_voice_underruns -ne 0 -or
                    [int]$complete.new_game_voice_non_silent_blocks -le [int]$firstShot.new_game_voice_non_silent_blocks -or
                    [int]$complete.animation_peak_vertices -gt 12288) {
                    throw "Unexpected SS01 continuation cast or narration. Draws $draws, voice $($complete.new_game_continuation_voice_ms) ms, peak $($complete.animation_peak_vertices) vertices. See $gameLog"
                }
                if ($Capture -and -not $CheckMainGameScene) { Send-Key Screenshot }
                "SS01b played to $($complete.new_game_continuation_voice_ms) ms without underruns, drew all 13 original shots and finished in BeadyIntDay."
                if ($CheckMainGameStart) {
                    $triangles = [int]$complete.new_game_sim_collision_triangles
                    if ($complete.new_game_sim_start_ready -ne '1' -or $complete.new_game_sim_collision_packs -ne '6' -or
                        $complete.new_game_sim_area -ne '105' -or $triangles -ne 5559 -or
                        $complete.new_game_sim_spawn -ne '19.087500,-8.363579,-35.884602' -or
                        $complete.new_game_sim_facing -ne '-0.012220,0.000000,0.999925') {
                        throw "Original EnvMain start did not resolve. See $gameLog"
                    }
                    $ground = [double]::Parse($complete.new_game_sim_ground_y, [Globalization.CultureInfo]::InvariantCulture)
                    if ([Math]::Abs($ground + 8.363581) -gt 0.0001) { throw "Unexpected EnvMain ground Y $ground. See $gameLog" }
                    "Resolved the original park2 start and queried ground Y $ground against $triangles original EnvMain collision triangles."
                }
                if ($CheckMainGameScene) {
                    Start-Sleep -Milliseconds 2500
                    $terrainMeshes = if ($CheckMainGameInstances) { 445 } else { 388 }
                    $terrainTextures = if ($CheckMainGameInstances) { 152 } else { 128 }
                    $scene = Wait-Menu 'sim-world-ready' -1 @{
                        sim_terrain_meshes = $terrainMeshes; sim_terrain_textures = $terrainTextures
                        sim_sky_draws = 2; sim_player_draws = 2
                    }
                    if ([int]$scene.sim_frames -le [int]$complete.sim_frames -or [int]$scene.sim_terrain_draws -le 0) {
                        throw "The EnvMain scene did not continue rendering. See $gameLog"
                    }
                    if ([int]$scene.render_main_peak -gt [int]$scene.render_main_budget) {
                        throw "EnvMain exceeded the PSP render budget. See $gameLog"
                    }
                    if ($Capture) { Send-Key Screenshot }
                    "Rendered $($scene.sim_frames) EnvMain frames with $($scene.sim_terrain_draws) terrain draws and two gameplay player meshes."
                    if ($CheckMainGameMovement) { Test-MainGameMovement $scene }
                    if ($CheckMainGameCollision) { Test-MainGameCollision (Read-GameLog) }
                    if ($CheckMainGameRegions) { Test-MainGameRegions (Read-GameLog) }
                    if ($CheckMainGameCrossroads) { Test-MainGameCrossroads (Read-GameLog) }
                }
            }
        }
        if ($CheckMainGameScene -and (Read-GameLog).ContainsKey('sim_updates')) {
            Test-MainGamePause
            $exitButton = 'confirmed Quit'
        } elseif ($CheckMainGameScene) {
            Send-Key Start
            $exitButton = 'Start'
        } else {
            Send-Key Triangle
            $exitButton = 'Cancel'
        }
        $main = Wait-Menu 'frontend-new-game-return' 0 @{} 90
        if ($RequireOpeningActors -and $main.ContainsKey('animation_live_vertices') -and
            ($main.animation_live_vertices -ne '0' -or $main.animation_sample_bytes -ne '0')) {
            throw "Animation resources remained resident after returning to the menu. See $gameLog"
        }
        "New Game opened the four original slots, read all 21 SS01 tracks from slot 4 and returned on $exitButton."
        if ($OpeningOnly) { return }
    }
    $mainPhase = $main.phase
    if ($RequireMenuAudio -and $main.menu_audio_ready -ne '1') {
        throw "Original UI wave bank or PSP audio channel did not initialize. See $gameLog"
    }
    if ($RequireMenuMusic) {
        $musicStart = Wait-MenuMusic 0
        "Original main-menu music playing at $($musicStart.menu_music_position_ms) ms."
    }
    Send-Key DpadDown
    $null = Wait-Menu $mainPhase 1
    if ($RequireMenuAudio) { Wait-MenuCue 174 $main }
    Send-Key DpadDown
    $before = Wait-Menu $mainPhase 2
    Send-Key Cross
    $original = Wait-Menu 'frontend-options-ready' 0
    if ($RequireMenuAudio) { Wait-MenuCue 175 $before }
    if ($RequireMenuMusic) {
        $musicInOptions = Wait-MenuMusic 0 ([int]$original.option_music)
        Assert-MenuMusicProgress $musicStart $musicInOptions
    }
    $effects = [int]$original.option_effects
    $invert = [int]$original.option_inverted
    $sensitivity = $original.option_sensitivity
    $volumeKey = if ($effects -gt 0) { 'DpadLeft' } else { 'DpadRight' }
    $volume = if ($effects -gt 0) { [Math]::Max(0, $effects - 51) } else { 51 }

    Send-Key Cross
    $before = Wait-Menu 'frontend-audio-ready' 0 @{ option_effects = $effects }
    if ($RequireMenuAudio) { Wait-MenuCue 168 $original }
    if ($Capture) { Send-Key Screenshot }
    Send-Key $volumeKey
    $null = Wait-Menu 'frontend-audio-ready' 0 @{ option_effects = $volume }
    if ($RequireMenuAudio) { Wait-MenuCue 166 $before }
    $changedVolumes = @{ option_effects = $volume }
    $row = 0
    foreach ($category in @('option_music', 'option_speech')) {
        $row++
        Send-Key DpadDown
        $before = Wait-Menu 'frontend-audio-ready' $row
        $value = [int]$original[$category]
        if ($value -eq 255) {
            Send-Key DpadLeft
            $changedVolumes[$category] = $value - 51
        }
        else {
            Send-Key DpadRight
            $changedVolumes[$category] = [Math]::Min(255, $value + 51)
        }
        $null = Wait-Menu 'frontend-audio-ready' $row $changedVolumes
        if ($RequireMenuAudio -and $category -eq 'option_speech') { Wait-MenuCue 189 $before }
        if ($RequireMenuMusic -and $category -eq 'option_music') {
            $musicChanged = Wait-MenuMusic 0 ([int]$changedVolumes.option_music)
            Assert-MenuMusicProgress $musicInOptions $musicChanged
            "Music slider applied gain $($musicChanged.menu_music_gain_per_mille) per mille."
        }
    }
    $before = Read-GameLog
    Send-Key Triangle
    $null = Wait-Menu 'frontend-options-ready' 0 @{ option_effects = $effects; option_music = $original.option_music; option_speech = $original.option_speech }
    if ($RequireMenuAudio) { Wait-MenuCue 167 $before }
    if ($RequireMenuMusic) {
        $musicRestored = Wait-MenuMusic 0 ([int]$original.option_music)
        Assert-MenuMusicProgress $musicChanged $musicRestored
        "Cancel restored music gain $($musicRestored.menu_music_gain_per_mille) per mille."
    }
    Send-Key Cross
    $null = Wait-Menu 'frontend-audio-ready' 0 @{ option_effects = $effects }
    Send-Key $volumeKey
    $null = Wait-Menu 'frontend-audio-ready' 0 @{ option_effects = $volume }
    Send-Key Cross
    $null = Wait-Menu 'frontend-options-ready' 0 @{ option_effects = $volume }
    "Audio validated in session: $effects -> $volume -> $effects on Cancel, then $volume on OK."

    Send-Key DpadDown
    $null = Wait-Menu 'frontend-options-ready' 1
    Send-Key Cross
    $null = Wait-Menu 'frontend-controls-ready' 0 @{ option_inverted = $invert; option_sensitivity = $sensitivity }
    if ($Capture) { Send-Key Screenshot }
    Send-Key DpadRight
    $null = Wait-Menu 'frontend-controls-ready' 0 @{ option_inverted = 1 - $invert }
    Send-Key DpadDown
    $null = Wait-Menu 'frontend-controls-ready' 1
    $culture = [Globalization.CultureInfo]::InvariantCulture
    $originalSensitivity = [double]::Parse($sensitivity, $culture)
    $sensitivityKey = if ($originalSensitivity -lt 0.95) { 'DpadRight' } else { 'DpadLeft' }
    $changedSensitivity = ($originalSensitivity + $(if ($sensitivityKey -eq 'DpadRight') { 0.1 } else { -0.1 })).ToString('0.000000', $culture)
    Send-Key $sensitivityKey
    $null = Wait-Menu 'frontend-controls-ready' 1 @{ option_sensitivity = $changedSensitivity }
    Send-Key Triangle
    $null = Wait-Menu 'frontend-options-ready' 1 @{ option_inverted = $invert; option_sensitivity = $sensitivity }
    Send-Key Cross
    $null = Wait-Menu 'frontend-controls-ready' 0 @{ option_inverted = $invert }
    Send-Key DpadRight
    $null = Wait-Menu 'frontend-controls-ready' 0 @{ option_inverted = 1 - $invert }
    Send-Key DpadDown
    $null = Wait-Menu 'frontend-controls-ready' 1
    Send-Key $sensitivityKey
    $null = Wait-Menu 'frontend-controls-ready' 1 @{ option_sensitivity = $changedSensitivity }
    Send-Key Cross
    $null = Wait-Menu 'frontend-options-ready' 1 @{ option_inverted = 1 - $invert; option_sensitivity = $changedSensitivity }
    "Controls validated in session: Cancel restored inversion and sensitivity, OK applied both ($sensitivity -> $changedSensitivity)."
    Send-Key Triangle
    $null = Wait-Menu 'frontend-options-return' 2
    'Returned to the original main menu.'
    if ($RequireMenuMusic) {
        $musicInMainMenu = Wait-MenuMusic 0 ([int]$original.option_music)
        Assert-MenuMusicProgress $musicRestored $musicInMainMenu
        $musicLoop = Wait-MenuMusic 1 ([int]$original.option_music) -TimeoutSeconds 75
        "Original music loop verified. Loops: $($musicLoop.menu_music_loops). Stream underruns: $($musicLoop.menu_music_underruns)."
    }
}
finally {
    & $runner @runnerArguments -Action Stop
}
