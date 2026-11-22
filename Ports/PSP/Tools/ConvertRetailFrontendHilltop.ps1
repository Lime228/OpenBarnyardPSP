[CmdletBinding()]
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
    $OutputDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainHilltop'
}

$arguments = @{
    OutputDirectory = $OutputDirectory
    SourceModelName = 'Hilltop_L1Mod0'
    AdditionalSourceModelNames = @(
        'Hilltop_L1Mod1',
        'Hilltop_L1Mod2'
    )
    SourceMaterialLibraryName = 'Hilltop_L1Mat'
    Prefix = 'frontend_hilltop'
    TextureMaxWidth = $TextureMaxWidth
}
if ($TMDLPath) { $arguments.TMDLPath = $TMDLPath }
if ($PythonPath) { $arguments.PythonPath = $PythonPath }
if ($RuntimeDirectory) { $arguments.RuntimeDirectory = $RuntimeDirectory }
if ($KeepIntermediate) { $arguments.KeepIntermediate = $true }

& (Join-Path $PSScriptRoot 'ConvertRetailFrontendDetail.ps1') @arguments
