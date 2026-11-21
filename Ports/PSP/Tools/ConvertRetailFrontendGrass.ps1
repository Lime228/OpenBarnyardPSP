param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string] $RuntimeDirectory = '',
    [int] $TextureMaxWidth = 128,
    [switch] $KeepIntermediate
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainGrass'
}

$arguments = @{
    OutputDirectory = $OutputDirectory
    SourceModelName = 'Barn_L0Mod2'
    Prefix = 'frontend_grass'
    TextureMaxWidth = $TextureMaxWidth
}
if ($TMDLPath) { $arguments.TMDLPath = $TMDLPath }
if ($PythonPath) { $arguments.PythonPath = $PythonPath }
if ($RuntimeDirectory) { $arguments.RuntimeDirectory = $RuntimeDirectory }
if ($KeepIntermediate) { $arguments.KeepIntermediate = $true }

& (Join-Path $PSScriptRoot 'ConvertRetailFrontendDetail.ps1') @arguments
