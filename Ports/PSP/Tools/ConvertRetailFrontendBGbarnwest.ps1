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
    $OutputDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainBGbarnwest'
}

$arguments = @{
    OutputDirectory = $OutputDirectory
    SourceModelName = 'BGbarnwest_L1Mod0'
    AdditionalSourceModelNames = @(
        'BGbarnwest_L1Mod1',
        'BGbarnwest_L1Mod2',
        'BGbarnwest_L1Mod3',
        'BGbarnwest_L1Mod4'
    )
    SourceMaterialLibraryName = 'BGbarnwest_L1Mat'
    Prefix = 'frontend_bgbarnwest'
    TextureMaxWidth = $TextureMaxWidth
}
if ($TMDLPath) { $arguments.TMDLPath = $TMDLPath }
if ($PythonPath) { $arguments.PythonPath = $PythonPath }
if ($RuntimeDirectory) { $arguments.RuntimeDirectory = $RuntimeDirectory }
if ($KeepIntermediate) { $arguments.KeepIntermediate = $true }

& (Join-Path $PSScriptRoot 'ConvertRetailFrontendDetail.ps1') @arguments
