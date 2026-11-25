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

function Wait-Menu([string] $Phase, [int] $Focus, [hashtable] $Expected = @{}, [int] $TimeoutSeconds = 10) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $status = Read-GameLog
        if ($status.phase -match '(?:data-failed|widgets-missing|script-failed|terrain-failed|opening-failed|sim-start-failed|sim-world-failed)$') { throw "Game resource failure: $($status.failed_path)" }
        $matching = $status.phase -eq $Phase
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
                    $scene = Wait-Menu 'sim-world-ready' -1 @{
                        sim_terrain_meshes = 394; sim_terrain_textures = 128; sim_sky_draws = 2; sim_player_draws = 2
                    }
                    if ([int]$scene.sim_frames -le [int]$complete.sim_frames -or [int]$scene.sim_terrain_draws -le 0) {
                        throw "The EnvMain scene did not continue rendering. See $gameLog"
                    }
                    if ($Capture) { Send-Key Screenshot }
                    "Rendered $($scene.sim_frames) EnvMain frames with $($scene.sim_terrain_draws) terrain draws and two gameplay player meshes."
                }
            }
        }
        if ($CheckMainGameScene) { Send-Key Start } else { Send-Key Triangle }
        $main = Wait-Menu 'frontend-new-game-return' 0 @{} 90
        if ($RequireOpeningActors -and $main.ContainsKey('animation_live_vertices') -and
            ($main.animation_live_vertices -ne '0' -or $main.animation_sample_bytes -ne '0')) {
            throw "Animation resources remained resident after returning to the menu. See $gameLog"
        }
        $exitButton = if ($CheckMainGameScene) { 'Start' } else { 'Cancel' }
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
