param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $CameraPath = '',
    [string] $OutputDirectory = '',
    [string[]] $TerrainPaths = @(),
    [ValidateRange(1, 16)]
    [int] $ResidentSeedLimit = 16
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'

if (-not $CameraPath) {
    $CameraPath = Join-Path $assetRoot 'Workspace\FrontendCamera\frontend_camera.pcam'
}
if (-not (Test-Path -LiteralPath $CameraPath -PathType Leaf)) {
    throw "Converted retail frontend camera was not found: $CameraPath"
}
$CameraPath = (Resolve-Path -LiteralPath $CameraPath).Path

if ($TerrainPaths.Count -eq 0) {
    $TerrainPaths = @(
        'Install\Data\Terrain\frontend\Barn_L0Mod1.trb',
        'Install\Data\Terrain\frontend\Barn_L0Mod2.trb',
        'Install\Data\Terrain\frontend\Barn_L0Mod3.trb'
    )
}
$resolvedTerrainPaths = @()
foreach ($terrainPath in $TerrainPaths) {
    $candidate = if ([System.IO.Path]::IsPathRooted($terrainPath)) {
        [System.IO.Path]::GetFullPath($terrainPath)
    }
    else {
        [System.IO.Path]::GetFullPath((Join-Path $assetRoot $terrainPath))
    }
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Retail frontend terrain input was not found: $candidate"
    }
    $resolvedTerrainPaths += $candidate
}
$baseNames = @($resolvedTerrainPaths | ForEach-Object { [System.IO.Path]::GetFileNameWithoutExtension($_) })
if (@($baseNames | Select-Object -Unique).Count -ne $baseNames.Count) {
    throw 'Terrain input file names must be unique within one inventory operation'
}

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot 'Workspace\FrontendTerrainInventory'
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

$summaries = @()
for ($index = 0; $index -lt $resolvedTerrainPaths.Count; $index++) {
    $terrainPath = $resolvedTerrainPaths[$index]
    $baseName = $baseNames[$index]
    $inventoryPath = Join-Path $outputPath "$baseName.inventory.json"
    $selectionPath = Join-Path $outputPath "$baseName.residency.json"

    Push-Location (Split-Path -Parent $TMDLPath)
    try {
        & $TMDLPath -psp-terrain-inventory $terrainPath -world 0 -cell 0 -o $inventoryPath
        if ($LASTEXITCODE -ne 0) {
            throw "Retail frontend terrain inventory failed for $baseName with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }

    & $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'select_frontend_terrain.py') `
        $inventoryPath $CameraPath --limit $ResidentSeedLimit --output $selectionPath
    if ($LASTEXITCODE -ne 0) {
        throw "Retail frontend terrain selection failed for $baseName with exit code $LASTEXITCODE"
    }
    $selection = Get-Content -LiteralPath $selectionPath -Raw | ConvertFrom-Json
    $summaries += [pscustomobject]@{
        Terrain = $baseName
        InventoryMeshes = $selection.inventory_meshes
        VisibleMeshes = $selection.visible_meshes
        PriorityMeshes = $selection.priority_indices.Count
        GeometryBytes = $selection.priority_geometry_bytes
        Priority = ($selection.priority_indices -join ';')
    }
}

$summaries | Format-Table -AutoSize | Out-String | Write-Output
Write-Output "Retail frontend terrain inventory written to $outputPath"
