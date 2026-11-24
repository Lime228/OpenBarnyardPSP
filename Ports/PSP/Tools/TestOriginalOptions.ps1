[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $RuntimeDirectory,
    [switch] $Capture,
    [switch] $RequireMenuAudio,
    [switch] $RequireMenuMusic,
    [switch] $CheckNewGame
)

$ErrorActionPreference = 'Stop'
$runtimePath = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$runner = Join-Path $PSScriptRoot '..\RunPPSSPP.ps1'
$eboot = Join-Path $runtimePath 'EBOOT.PBP'
$gameLog = Join-Path $runtimePath 'openbarnyard_psp_game.log'
$movieLog = Join-Path $runtimePath 'openbarnyard_psp_movies.log'
if (Get-Process -Name PPSSPPWindows64, PPSSPPWindows -ErrorAction SilentlyContinue) {
    throw 'An existing PPSSPP process is running.'
}
$startedAt = Get-Date

function Send-Key([string] $Key) {
    & $runner -Action Press -Key $Key -EbootPath $eboot | Write-Host
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

function Wait-Menu([string] $Phase, [int] $Focus, [hashtable] $Expected = @{}) {
    $deadline = (Get-Date).AddSeconds(10)
    do {
        $status = Read-GameLog
        if ($status.phase -match '(?:data-failed|widgets-missing|script-failed)$') { throw "Frontend resource failure: $($status.failed_path)" }
        $matching = $status.phase -eq $Phase -and $status.menu_state -eq '5' -and $status.menu_focus -eq "$Focus"
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
    & $runner -Action Start -EbootPath $eboot
    foreach ($name in @('NickMovs', 'THQ', 'BTE', 'Attract')) {
        $deadline = (Get-Date).AddSeconds(35)
        do {
            if ((Test-Path -LiteralPath $movieLog -PathType Leaf) -and (Get-Item -LiteralPath $movieLog).LastWriteTime -ge $startedAt -and
                (Select-String -Quiet -LiteralPath $movieLog -Pattern "/$name\.pmvf result=started ")) { break }
            if ((Get-Date) -ge $deadline) { throw "Waiting for $name timed out." }
            Start-Sleep -Milliseconds 150
        } while ($true)
        Start-Sleep -Milliseconds 1500
        Send-Key Cross
    }

    $main = Wait-Menu 'frontend-menu-ready' 0
    if ($CheckNewGame) {
        Send-Key Cross
        $null = Wait-Menu 'frontend-new-game-ready' 0 @{ new_game_requested_slot = -1 }
        foreach ($slot in 1..3) {
            Send-Key DpadDown
            $null = Wait-Menu 'frontend-new-game-ready' $slot
        }
        if ($Capture) { Send-Key Screenshot }
        Send-Key Cross
        $null = Wait-Menu 'frontend-new-game-awaiting-sequence' 3 @{ new_game_requested_slot = 3; new_game_script_tracks = 21 }
        Send-Key Triangle
        $main = Wait-Menu 'frontend-new-game-return' 0
        'New Game opened the four original slots, read all 21 SS01 tracks from slot 4 and returned on Cancel.'
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
    & $runner -Action Stop -EbootPath $eboot
}
