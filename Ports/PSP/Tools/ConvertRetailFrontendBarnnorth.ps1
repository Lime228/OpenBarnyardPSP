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
    $OutputDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainBarnnorth'
}

$packs = @(
    @{
        SourceModelName = 'Barnnorth_L0Mod0'
        Prefix = 'frontend_barnnorth_base'
    },
    @{
        SourceModelName = 'Barnnorth_L0Mod1'
        Prefix = 'frontend_barnnorth_detail'
    },
    @{
        SourceModelName = 'Barnnorth_L0Mod2'
        Prefix = 'frontend_barnnorth_grass'
    },
    @{
        SourceModelName = 'Barnnorth_L0Mod3'
        Prefix = 'frontend_barnnorth_glow'
        ForceAlphaMode = 'additive'
        LightProfiles = @(
            'lights_farmhouse=farmhouse',
            'nightlight=nightlight'
        )
    }
)

foreach ($pack in $packs) {
    $arguments = @{
        OutputDirectory = $OutputDirectory
        SourceModelName = $pack.SourceModelName
        SourceMaterialLibraryName = 'Barnnorth_L0Mat'
        Prefix = $pack.Prefix
        TextureMaxWidth = $TextureMaxWidth
    }
    if ($TMDLPath) { $arguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $arguments.PythonPath = $PythonPath }
    if ($RuntimeDirectory) { $arguments.RuntimeDirectory = $RuntimeDirectory }
    if ($KeepIntermediate) { $arguments.KeepIntermediate = $true }
    if ($pack.ForceAlphaMode) { $arguments.ForceAlphaMode = $pack.ForceAlphaMode }
    if ($pack.LightProfiles) { $arguments.LightProfiles = $pack.LightProfiles }

    & (Join-Path $PSScriptRoot 'ConvertRetailFrontendDetail.ps1') @arguments
}
