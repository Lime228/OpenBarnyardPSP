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
    $OutputDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainBGbarneast'
}

$arguments = @{
    OutputDirectory = $OutputDirectory
    SourceModelName = 'BGbarneast_L1Mod0'
    AdditionalSourceModelNames = @(
        'BGbarneast_L1Mod1',
        'BGbarneast_L1Mod2',
        'BGbarneast_L1Mod3',
        'BGbarneast_L1Mod4'
    )
    SourceMaterialLibraryName = 'BGbarneast_L1Mat'
    Prefix = 'frontend_bgbarneast'
    TextureMaxWidth = $TextureMaxWidth
}
if ($TMDLPath) { $arguments.TMDLPath = $TMDLPath }
if ($PythonPath) { $arguments.PythonPath = $PythonPath }
if ($RuntimeDirectory) { $arguments.RuntimeDirectory = $RuntimeDirectory }
if ($KeepIntermediate) { $arguments.KeepIntermediate = $true }

& (Join-Path $PSScriptRoot 'ConvertRetailFrontendDetail.ps1') @arguments
