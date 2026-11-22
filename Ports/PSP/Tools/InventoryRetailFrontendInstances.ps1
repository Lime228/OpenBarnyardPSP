param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $LocatorPath = '',
    [string] $OutputDirectory = '',
    [string[]] $ResidentSections = @(
        'Barn',
        'Barnnorth',
        'BGbarneast',
        'BGbarnwest',
        'Hillbase',
        'Hilltop',
        'Meadowsouth'
    )
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
$installRoot = Join-Path $assetRoot 'Install'

if (-not $LocatorPath) {
    $LocatorPath = Join-Path $installRoot 'Data\Terrain\frontend\frontend_skel.trb'
}
if (-not (Test-Path -LiteralPath $LocatorPath -PathType Leaf)) {
    throw "Retail frontend locator input was not found: $LocatorPath"
}
$LocatorPath = (Resolve-Path -LiteralPath $LocatorPath).Path

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot 'Workspace\FrontendInstances'
}
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

if (-not $TMDLPath) {
    $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') `
        -RepositoryRoot $repositoryRoot
}
if (-not $TMDLPath -or -not (Test-Path -LiteralPath $TMDLPath -PathType Leaf)) {
    throw 'TMDL.exe was not found; pass -TMDLPath or download the OpenBarnyard-PSP-host-tools artifact'
}
$TMDLPath = (Resolve-Path -LiteralPath $TMDLPath).Path
New-Item -ItemType Directory -Force -Path (Join-Path (Split-Path -Parent $TMDLPath) 'Logs') | Out-Null

$pythonArguments = @()
if (-not $PythonPath) {
    foreach ($name in @('python3', 'python')) {
        if ($PythonPath) { break }
        $candidate = Get-Command $name -ErrorAction SilentlyContinue
        if ($candidate -and $candidate.Source -notlike '*\WindowsApps\python*.exe') {
            $PythonPath = $candidate.Source
        }
    }
    if (-not $PythonPath) {
        $candidate = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { $PythonPath = $candidate }
    }
    if (-not $PythonPath) {
        $launcher = Get-Command py -ErrorAction SilentlyContinue
        if ($launcher) {
            $PythonPath = $launcher.Source
            $pythonArguments = @('-3')
        }
    }
}
if (-not $PythonPath -or -not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
    throw 'Python 3 was not found; pass -PythonPath explicitly'
}
$PythonPath = (Resolve-Path -LiteralPath $PythonPath).Path

$locatorDumpPath = Join-Path $outputPath 'frontend_locators.json'
$inventoryPath = Join-Path $outputPath 'frontend_instances.inventory.json'
Push-Location (Split-Path -Parent $TMDLPath)
try {
    & $TMDLPath -psp-locators $LocatorPath -o $locatorDumpPath
    if ($LASTEXITCODE -ne 0) {
        throw "Retail frontend locator table export failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

$inventoryArguments = @(
    (Join-Path $PSScriptRoot 'inventory_frontend_instances.py'),
    $locatorDumpPath,
    '--model-definitions', (Join-Path $repositoryRoot 'OpenBarnyard\Source\Helpers\AInstanceManagerModelList.inl'),
    '--game-loader', (Join-Path $repositoryRoot 'OpenBarnyard\Source\Helpers\AGameLoader.cpp'),
    '--install-root', $installRoot,
    '--output', $inventoryPath
)
foreach ($section in $ResidentSections) {
    $inventoryArguments += @('--resident-section', $section)
}
& $PythonPath @pythonArguments @inventoryArguments
if ($LASTEXITCODE -ne 0) {
    throw "Retail frontend instance inventory failed with exit code $LASTEXITCODE"
}

$inventory = Get-Content -LiteralPath $inventoryPath -Raw | ConvertFrom-Json
[pscustomobject]@{
    SourceLocators = $inventory.source_locator_count
    VISSections = $inventory.vis_section_count
    FrontendModels = $inventory.frontend_model_count
    UsedModels = $inventory.used_model_count
    Instances = $inventory.instance_count
    ResidentModels = $inventory.resident_used_model_count
    ResidentInstances = $inventory.resident_instance_count
    Inventory = $inventoryPath
}
