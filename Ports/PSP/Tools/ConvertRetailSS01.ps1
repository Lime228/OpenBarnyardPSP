[CmdletBinding()]
param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string] $EncodedAudioDirectory = '',
    [ValidateSet(32, 64, 128, 256, 512)]
    [int] $TextureMaxWidth = 128
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$installPath = Join-Path $repositoryRoot 'Game\BarnyardPC\Install'
$encodedAudioRequired = [bool]$EncodedAudioDirectory
if (-not $EncodedAudioDirectory) {
    $EncodedAudioDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\AudioBanks\Encoded'
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\SS01'
}
if (-not $TMDLPath) {
    $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') -RepositoryRoot $repositoryRoot
}
$TMDLPath = (Resolve-Path -LiteralPath $TMDLPath).Path
$python = & (Join-Path $PSScriptRoot 'ResolvePSPPython.ps1') -PythonPath $PythonPath
$pythonArguments = $python.Arguments

$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$sequencePath = Join-Path $outputPath 'Sequence'
$modelsPath = Join-Path $outputPath 'SequenceModels'
$voicePath = Join-Path $outputPath 'Voice'
$sequenceManifestPath = Join-Path $sequencePath 'sequence.json'

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'inventory_retail_sequence.py') `
    --tmdl $TMDLPath $installPath SS01_start $sequencePath
if ($LASTEXITCODE -ne 0) { throw 'SS01 sequence inventory failed.' }

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_retail_sequence_opening.py') `
    --tmdl $TMDLPath $installPath $sequenceManifestPath (Join-Path $outputPath 'Opening')
if ($LASTEXITCODE -ne 0) { throw 'SS01 opening camera packaging failed.' }

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_retail_sequence_opening.py') `
    --all-shots --tmdl $TMDLPath $installPath $sequenceManifestPath (Join-Path $outputPath 'CameraSequence')
if ($LASTEXITCODE -ne 0) { throw 'SS01 camera sequence conversion failed.' }

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_retail_character_setup.py') `
    --tmdl $TMDLPath --camera-sequence (Join-Path $outputPath 'CameraSequence\camera_sequence.json') `
    $installPath (Join-Path $outputPath 'CharacterSetup')
if ($LASTEXITCODE -ne 0) { throw 'SS01 character creation graphics packaging failed.' }

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_retail_player_costumes.py') `
    --tmdl $TMDLPath --max-width $TextureMaxWidth $installPath $sequenceManifestPath (Join-Path $outputPath 'CharacterSetup\Costumes')
if ($LASTEXITCODE -ne 0) { throw 'SS01 character costume packaging failed.' }

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'convert_retail_sequence_models.py') `
    --tmdl $TMDLPath --max-width $TextureMaxWidth $installPath $sequenceManifestPath $modelsPath
if ($LASTEXITCODE -ne 0) { throw 'SS01 sequence model conversion failed.' }

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_retail_opening_actors.py') `
    --tmdl $TMDLPath $installPath $sequenceManifestPath $modelsPath (Join-Path $outputPath 'Opening')
if ($LASTEXITCODE -ne 0) { throw 'SS01 opening actor packaging failed.' }

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_retail_opening_actors.py') `
    --after-character-setup --tmdl $TMDLPath $installPath $sequenceManifestPath $modelsPath (Join-Path $outputPath 'AfterCharacterSetup')
if ($LASTEXITCODE -ne 0) { throw 'SS01 second narration actor packaging failed.' }

if ($encodedAudioRequired -or (Test-Path -LiteralPath (Join-Path $EncodedAudioDirectory 'fsb.encode.json') -PathType Leaf)) {
    & $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_retail_sequence_voice.py') `
        $sequenceManifestPath $EncodedAudioDirectory $voicePath
    if ($LASTEXITCODE -ne 0) { throw 'SS01 voice packaging failed.' }
}

$sequence = Get-Content -LiteralPath $sequenceManifestPath -Raw | ConvertFrom-Json
$rows = @('Dummy'
    foreach ($camera in $sequence.script.camera_tracks) {
        foreach ($event in $camera.events) {
            if ($event.type -ceq 'command' -and $event.name -ceq 'SetTerrain') { $event.arguments[0] }
        }
    }
)
foreach ($row in $rows | Select-Object -Unique) {
    $suffix = if ($row -ceq 'Dummy') { '' } else { "_$row" }
    $terrainPath = Join-Path $outputPath "Terrain$suffix"
    & (Join-Path $PSScriptRoot 'ConvertRetailSS01Terrain.ps1') -TMDLPath $TMDLPath -PythonPath $PythonPath `
        -OutputDirectory $terrainPath -VisibilityRow $row -TextureMaxWidth $TextureMaxWidth

    & $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_sequence_terrain.py') `
        (Join-Path $terrainPath 'ss01_terrain.json') (Join-Path $outputPath "TerrainPack$suffix")
    if ($LASTEXITCODE -ne 0) { throw "SS01 terrain packaging failed for '$row'." }
}
