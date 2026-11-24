[CmdletBinding()]
param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [ValidateSet(32, 64, 128, 256, 512)]
    [int] $TextureMaxWidth = 128
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$installPath = Join-Path $repositoryRoot 'Game\BarnyardPC\Install'
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
$terrainPath = Join-Path $outputPath 'Terrain'

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'inventory_retail_sequence.py') `
    --tmdl $TMDLPath $installPath SS01_start $sequencePath
if ($LASTEXITCODE -ne 0) { throw 'SS01 sequence inventory failed.' }

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'convert_retail_sequence_models.py') `
    --tmdl $TMDLPath --max-width $TextureMaxWidth $installPath (Join-Path $sequencePath 'sequence.json') $modelsPath
if ($LASTEXITCODE -ne 0) { throw 'SS01 sequence model conversion failed.' }

& (Join-Path $PSScriptRoot 'ConvertRetailSS01Terrain.ps1') -TMDLPath $TMDLPath -PythonPath $PythonPath `
    -OutputDirectory $terrainPath -TextureMaxWidth $TextureMaxWidth
