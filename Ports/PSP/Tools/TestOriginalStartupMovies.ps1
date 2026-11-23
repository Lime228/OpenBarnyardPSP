[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $RuntimeDirectory,
    [ValidateRange(20, 300)] [int] $TimeoutSeconds = 175,
    [switch] $CaptureAttract,
    [switch] $SkipAttract,
    [switch] $RequireSteadyAudio
)

$ErrorActionPreference = 'Stop'
$runtimePath = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$runner = Join-Path $PSScriptRoot '..\RunPPSSPP.ps1'
$eboot = Join-Path $runtimePath 'EBOOT.PBP'
$movieLog = Join-Path $runtimePath 'openbarnyard_psp_movies.log'
$gameLog = Join-Path $runtimePath 'openbarnyard_psp_game.log'
$movies = @('NickMovs', 'THQ', 'BTE', 'Attract')
if (Get-Process -Name PPSSPPWindows64, PPSSPPWindows -ErrorAction SilentlyContinue) {
    throw 'An existing PPSSPP process is running.'
}

$expected = @{}
foreach ($movie in $movies) {
    $path = Join-Path $runtimePath "Data\Movies\$movie.pmvf"
    $stream = [System.IO.File]::OpenRead($path)
    try {
        $header = [byte[]]::new(48)
        if ($stream.Read($header, 0, $header.Length) -ne 48 -or
            [System.Text.Encoding]::ASCII.GetString($header, 0, 4) -cne 'PMVF' -or
            [System.BitConverter]::ToUInt32($header, 4) -ne 2) {
            throw "Invalid PMVF v2 startup asset: $path"
        }
        $numerator = [System.BitConverter]::ToUInt32($header, 20)
        $denominator = [System.BitConverter]::ToUInt32($header, 24)
        $frames = [System.BitConverter]::ToUInt32($header, 28)
        $pcmRate = [System.BitConverter]::ToUInt32($header, 40)
        if ($numerator -eq 0 -or $denominator -eq 0 -or $frames -eq 0) { throw "Invalid movie timing: $path" }
        $expected[$movie] = @{
            Rate = "$numerator/$denominator"
            Frames = $frames
            PCMFrames = [uint32][math]::Floor([decimal]$frames * $pcmRate * $denominator / $numerator)
        }
    }
    finally { $stream.Dispose() }
}

$startedAt = Get-Date
$attractActionAt = $null
$attractActionDone = $false
$movieLines = @()
$menuVisible = $false
try {
    & $runner -Action Start -EbootPath $eboot
    $deadline = $startedAt.AddSeconds($TimeoutSeconds)
    do {
        if ((Test-Path -LiteralPath $movieLog -PathType Leaf) -and (Get-Item -LiteralPath $movieLog).LastWriteTime -ge $startedAt) {
            $movieLines = @(Get-Content -LiteralPath $movieLog)
            if ($movieLines -match ' result=(?:missing|.*-failed) ') {
                $movieLines | Write-Output
                throw 'Original startup movie playback failed.'
            }
        }
        if (($CaptureAttract -or $SkipAttract) -and !$attractActionDone -and ($movieLines -match '/Attract\.pmvf result=started ')) {
            if (!$attractActionAt) { $attractActionAt = (Get-Date).AddSeconds(3) }
            if ((Get-Date) -ge $attractActionAt) {
                if ($CaptureAttract) { & $runner -Action Press -Key Screenshot -EbootPath $eboot }
                if ($SkipAttract) { & $runner -Action Press -Key Cross -EbootPath $eboot }
                $attractActionDone = $true
            }
        }
        if ((Test-Path -LiteralPath $gameLog -PathType Leaf) -and (Get-Item -LiteralPath $gameLog).LastWriteTime -ge $startedAt) {
            $gameStatus = Get-Content -LiteralPath $gameLog -Raw
            $menuVisible = $gameStatus -match '(?m)^phase=frontend-menu-ready\r?$' -and $gameStatus -match '(?m)^menu_state=5\r?$'
        }
        if ($menuVisible -and $movieLines.Count -ge 2 * $movies.Count) { break }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)

    $movieLines | Write-Output
    if (!$menuVisible) { throw "The original menu did not become visible. Logs: $movieLog and $gameLog" }
    if (($CaptureAttract -or $SkipAttract) -and !$attractActionDone) { throw 'The requested Attract action did not run.' }
    if ($movieLines.Count -ne 2 * $movies.Count) { throw 'Unexpected number of startup movie events.' }
    for ($index = 0; $index -lt $movies.Count; $index++) {
        $movie = $movies[$index]
        $origin = $expected[$movie]
        $result = $movieLines[2 * $index + 1]
        if ($movieLines[2 * $index] -notmatch "/$movie\.pmvf result=started ") { throw "Out of order startup: $movie." }
        if ($RequireSteadyAudio -and $result -notmatch ' underruns=0 ') { throw "PCM underrun detected for $movie, or its counter is missing." }
        if ($SkipAttract -and $movie -eq 'Attract') {
            if ($result -notmatch '/Attract\.pmvf result=stopped ' -or
                $result -notmatch ' presented=(\d+) last=(\d+) pcm_queued=(\d+) pcm_played=(\d+) ' -or
                [uint32]$Matches[1] -eq 0 -or [uint32]$Matches[2] -ge $origin.Frames -or
                [uint32]$Matches[4] -eq 0 -or [uint32]$Matches[4] -gt [uint32]$Matches[3]) {
                throw 'Attract did not stop during playback.'
            }
            continue
        }
        if ($result -notmatch "/$movie\.pmvf result=finished " -or
            $result -notmatch " rate=$($origin.Rate) frames=$($origin.Frames) read=$($origin.Frames) presented=(\d+) last=$($origin.Frames) " -or
            [uint32]$Matches[1] -eq 0 -or [uint32]$Matches[1] -gt $origin.Frames) {
            throw "Incomplete playback for $movie."
        }
        if ($result -notmatch " pcm_queued=$($origin.PCMFrames) pcm_played=$($origin.PCMFrames) pcm_peak=(\d+) " -or
            ($origin.PCMFrames -gt 0 -and [uint32]$Matches[1] -eq 0)) {
            throw "The original PCM track did not play to completion for $movie."
        }
    }
    'Original startup sequence completed and reached the visible main menu.'
    $movieLog
    $gameLog
}
finally {
    & $runner -Action Stop -EbootPath $eboot
}
