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
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $assetRoot 'Workspace\EnvMain' }
if (-not $TMDLPath) {
    $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') -RepositoryRoot $repositoryRoot
}
$TMDLPath = (Resolve-Path -LiteralPath $TMDLPath).Path
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$python = & (Join-Path $PSScriptRoot 'ResolvePSPPython.ps1') -PythonPath $PythonPath
$pythonArguments = $python.Arguments
$installPath = Join-Path $assetRoot 'Install'

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'inventory_retail_sim_start.py') `
    --tmdl $TMDLPath $installPath $outputPath
if ($LASTEXITCODE -ne 0) { throw 'The original gameplay start inventory failed.' }

$start = Get-Content -LiteralPath (Join-Path $outputPath 'sim_start.json') -Raw | ConvertFrom-Json
$row = $start.initial_visibility_row
$terrainOutput = Join-Path $outputPath "Terrain_$row"
& (Join-Path $PSScriptRoot 'ConvertRetailSS01Terrain.ps1') -TMDLPath $TMDLPath -PythonPath $PythonPath `
    -TerrainName EnvMain -VisibilityRow $row -OutputDirectory $terrainOutput -TextureMaxWidth $TextureMaxWidth
& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_sequence_terrain.py') `
    (Join-Path $terrainOutput 'EnvMain_terrain.json') (Join-Path $outputPath "TerrainPack_$row")
if ($LASTEXITCODE -ne 0) { throw 'The original gameplay terrain packaging failed.' }
