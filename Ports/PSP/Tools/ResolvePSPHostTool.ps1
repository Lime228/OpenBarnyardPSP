[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RepositoryRoot,
    [string] $ToolName = 'TMDL.exe'
)

$ErrorActionPreference = 'Stop'

$searchRoots = @(
    (Join-Path $RepositoryRoot 'Runtime\CIArtifacts'),
    (Join-Path $RepositoryRoot 'Game\BarnyardPC\Tools')
)
$candidates = foreach ($searchRoot in $searchRoots) {
    if (Test-Path -LiteralPath $searchRoot -PathType Container) {
        Get-ChildItem -LiteralPath $searchRoot -Filter $ToolName -Recurse -File
    }
}
$tool = $candidates |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1
if (-not $tool) {
    throw "$ToolName was not found; pass its path explicitly or download the OpenBarnyard-PSP-host-tools artifact"
}

$tool.FullName
