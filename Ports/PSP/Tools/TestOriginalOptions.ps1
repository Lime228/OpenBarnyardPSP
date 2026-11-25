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
if ($hasOpening -and -not $CheckNewGameOpening -and ($CheckNewGame -or $CheckNewGameTerrain)) {
    throw 'Use -CheckNewGameOpening for this staged runtime.'
}
if ($RequireOpeningSky -and (-not $CheckNewGameOpening -or -not (Test-Path -LiteralPath (Join-Path $runtimePath 'Data\ScriptedSequence\SS01\sky.psky') -PathType Leaf))) {
    throw 'The sky check needs the opening check and a staged sky.'
}
if ($RequireOpeningVoice -and (-not $CheckNewGameOpening -or -not (Test-Path -LiteralPath (Join-Path $runtimePath 'Data\ScriptedSequence\SS01\Voice') -PathType Container))) {
    throw 'The voice check needs the opening check and a staged voice cache.'
}
if ($RequireOpeningActors -and (-not $CheckNewGameOpening -or -not (Test-Path -LiteralPath (Join-Path $runtimePath 'Data\ScriptedSequence\SS01\Actors\actors.psac') -PathType Leaf))) {
    throw 'The actor check needs the opening check and a staged actor cache.'
}
if (Get-Process -Name PPSSPPWindows64, PPSSPPWindows -ErrorAction SilentlyContinue) {
    throw 'An existing PPSSPP process is running.'
}
$startedAt = Get-Date

function Send-Key([string] $Key) {
    & $runner @runnerArguments -Action Press -Key $Key | Write-Host
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
        if ($status.phase -match '(?:data-failed|widgets-missing|script-failed|terrain-failed|opening-failed)$') { throw "Frontend resource failure: $($status.failed_path)" }
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
                $expected.new_game_actor_draws_shot2 = 5
            }
            if ($Capture) {
                Send-Key Screenshot
                $shotCaptureDelay = if ($RequireOpeningActors) { 2000 } else { 750 }
                Start-Sleep -Milliseconds $shotCaptureDelay
                Send-Key Screenshot
            }
            "Rendered the original SS01 opening ($($newGame.new_game_opening_draws) draws across $($newGame.new_game_opening_frames) frames)."
            if ($RequireOpeningVoice) {
                $timeline = [IO.File]::ReadAllBytes($openingConfig)
                if ($timeline.Length -ne 40 -or [BitConverter]::ToUInt32($timeline, 4) -ne 2) {
                    throw 'Voice validation needs the two-shot SS01 timeline.'
                }
                $firstShotSeconds = [BitConverter]::ToSingle($timeline, 24)
                $secondShotSeconds = [BitConverter]::ToSingle($timeline, 36)
                $firstCutMs = [int](1000 * $firstShotSeconds)
                $endMs = [int](1000 * ($firstShotSeconds + $secondShotSeconds))
                $expected.new_game_opening_shot = 2
                $spoken = Wait-Menu 'frontend-new-game-opening' -1 $expected 90
                if ($RequireOpeningActors) {
                    "Submitted two bird meshes in the first shot and all five bird, farmer and bull meshes in the second."
                }
                if ([int]$spoken.new_game_voice_ms -lt $firstCutMs -or [int]$spoken.new_game_voice_underruns -ne 0 -or
                    [int]$spoken.new_game_voice_non_silent_blocks -lt 40) {
                    throw "The second original shot did not follow a playing SS01 voice stream. See $gameLog"
                }
                "Original narration reached the camera cut at $($spoken.new_game_voice_ms) ms with no underruns."
                if ($Capture) {
                    if ($RequireOpeningActors) { Start-Sleep -Milliseconds 1500 }
                    Send-Key Screenshot
                }

                $deadline = (Get-Date).AddSeconds(45)
                $terminal = $null
                do {
                    $now = Read-GameLog
                    if ($now.phase -eq 'frontend-new-game-opening' -and [int]$now.new_game_voice_ms -ge $endMs) {
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
                    throw "Narration did not stop at the end of the two-shot preview. See $gameLog"
                }
                "Narration stopped at $($now.new_game_voice_ms) ms while the last pose held."
            }
            elseif ($RequireOpeningActors) {
                $null = Wait-Menu 'frontend-new-game-opening' -1 $expected 90
                if ($Capture) {
                    Start-Sleep -Milliseconds 1500
                    Send-Key Screenshot
                }
                "Submitted two bird meshes in the first shot and all five bird, farmer and bull meshes in the second."
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
        Send-Key Triangle
        $main = Wait-Menu 'frontend-new-game-return' 0 @{} 90
        'New Game opened the four original slots, read all 21 SS01 tracks from slot 4 and returned on Cancel.'
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
