[CmdletBinding()]
param(
    [string] $BuildDirectory = '',
    [string] $InstallDirectory = '',
    [string] $FrontendDirectory = '',
    [string] $RuntimeDirectory = '',
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [switch] $SkipConversion
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $repositoryRoot 'bin\PSP\Release'
}
if (-not $InstallDirectory) {
    $InstallDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Install'
}
if (-not $FrontendDirectory) {
    $FrontendDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\Frontend'
}
if (-not $RuntimeDirectory) {
    $RuntimeDirectory = Join-Path $repositoryRoot 'Runtime\OpenBarnyardPSPGame'
}

$buildPath = [System.IO.Path]::GetFullPath($BuildDirectory)
$installPath = [System.IO.Path]::GetFullPath($InstallDirectory)
$frontendPath = [System.IO.Path]::GetFullPath($FrontendDirectory)
$runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
$gamePbpPath = Join-Path $buildPath 'EBOOT_GAME.PBP'
$gameSfoPath = Join-Path $buildPath 'PARAM_GAME.SFO'

foreach ($requiredFile in @($gamePbpPath, $gameSfoPath)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Original PSP game build output was not found: $requiredFile"
    }
}

$controlsPath = Join-Path $installPath 'Data\Options\Controls_Win.trb'
if (-not (Test-Path -LiteralPath $controlsPath -PathType Leaf)) {
    throw "Retail controls file was not found: $controlsPath"
}

New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null

if (-not $SkipConversion) {
    $conversionArguments = @{
        OutputDirectory = $frontendPath
        RuntimeDirectory = $runtimePath
    }
    if ($TMDLPath) { $conversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $conversionArguments.PythonPath = $PythonPath }

    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontend.ps1') @conversionArguments
}

$frontendFiles = @('barnyard_logo.ptex')
foreach ($picture in @('bkg_by_legal1', 'bkg_Bink_Big')) {
    foreach ($tile in 1..6) {
        $frontendFiles += "${picture}_${tile}.ptex"
    }
}

foreach ($fileName in $frontendFiles) {
    $sourcePath = Join-Path $frontendPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail frontend asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

$controlsRuntimeDirectory = Join-Path $runtimePath 'Data\Options'
New-Item -ItemType Directory -Force -Path $controlsRuntimeDirectory | Out-Null
Copy-Item -LiteralPath $controlsPath -Destination (Join-Path $controlsRuntimeDirectory 'Controls_Win.trb') -Force
Copy-Item -LiteralPath $gamePbpPath -Destination (Join-Path $runtimePath 'EBOOT.PBP') -Force
Copy-Item -LiteralPath $gameSfoPath -Destination (Join-Path $runtimePath 'PARAM.SFO') -Force

Write-Output "Staged original Barnyard PSP runtime at $runtimePath"
Write-Output "Run it with: .\Ports\PSP\RunPPSSPP.ps1 -Action Restart -OriginalGame"
