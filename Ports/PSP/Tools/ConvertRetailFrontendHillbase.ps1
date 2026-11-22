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
    $OutputDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainHillbase'
}

$arguments = @{
    OutputDirectory = $OutputDirectory
    SourceModelName = 'Hillbase_L1Mod0'
    AdditionalSourceModelNames = @(
        'Hillbase_L1Mod1',
        'Hillbase_L1Mod2',
        'Hillbase_L1Mod3'
    )
    SourceMaterialLibraryName = 'Hillbase_L1Mat'
    Prefix = 'frontend_hillbase'
    TextureMaxWidth = $TextureMaxWidth
}
if ($TMDLPath) { $arguments.TMDLPath = $TMDLPath }
if ($PythonPath) { $arguments.PythonPath = $PythonPath }
if ($RuntimeDirectory) { $arguments.RuntimeDirectory = $RuntimeDirectory }
if ($KeepIntermediate) { $arguments.KeepIntermediate = $true }

& (Join-Path $PSScriptRoot 'ConvertRetailFrontendDetail.ps1') @arguments
