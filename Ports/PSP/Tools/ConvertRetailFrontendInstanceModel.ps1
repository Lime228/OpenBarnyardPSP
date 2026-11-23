param(
    [string] $ModelName = 'fncw02',
    [string] $Prefix = '',
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string] $RuntimeDirectory = '',
    [ValidateRange(16, 256)]
    [int] $TextureMaxWidth = 64,
    [switch] $SkipInventory,
    [switch] $KeepIntermediate
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
$installRoot = Join-Path $assetRoot 'Install'
$locatorPath = Join-Path $installRoot 'Data\Terrain\frontend\frontend_skel.trb'
$materialLibraryPath = Join-Path $installRoot 'Data\Assets\lib_startup.trb'
$materialPropertiesPath = Join-Path $installRoot 'Data\MiscTextures\MaterialProperties.trb'

if (-not $ModelName -or $ModelName -notmatch '^[A-Za-z0-9_]+$') {
    throw 'ModelName must contain only ASCII letters, digits and underscores'
}
if (-not $Prefix) {
    $Prefix = "frontend_instances_$($ModelName.ToLowerInvariant())"
}
if ($Prefix -notmatch '^[A-Za-z0-9_]+$') {
    throw 'Prefix must contain only ASCII letters, digits and underscores'
}
foreach ($requiredFile in @($locatorPath, $materialLibraryPath, $materialPropertiesPath)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required retail frontend input was not found: $requiredFile"
    }
}

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot "Workspace\FrontendInstances\$ModelName"
}
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

if (-not $TMDLPath) {
    $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') -RepositoryRoot $repositoryRoot
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

$inventoryPath = Join-Path $assetRoot 'Workspace\FrontendInstances\frontend_instances.inventory.json'
if (-not $SkipInventory) {
    & (Join-Path $PSScriptRoot 'InventoryRetailFrontendInstances.ps1') `
        -TMDLPath $TMDLPath `
        -PythonPath $PythonPath | Out-Null
}
if (-not (Test-Path -LiteralPath $inventoryPath -PathType Leaf)) {
    throw "Original frontend instance inventory was not found: $inventoryPath"
}
$inventory = Get-Content -LiteralPath $inventoryPath -Raw | ConvertFrom-Json
$model = @($inventory.frontend_models | Where-Object { $_.name -ieq $ModelName })
if ($model.Count -ne 1) {
    throw "Original frontend model '$ModelName' was not found uniquely in the type-1 model list"
}
$modelPath = Join-Path $installRoot ([string]$model[0].path).Replace('/', '\')
if (-not (Test-Path -LiteralPath $modelPath -PathType Leaf)) {
    throw "Original frontend model file was not found: $modelPath"
}

$modelDumpPath = Join-Path $outputPath "$Prefix.model.json"
$propertiesDumpPath = Join-Path $outputPath 'material_properties.json'
$manifestPath = Join-Path $outputPath "$Prefix.json"
$configPath = Join-Path $outputPath "$Prefix.pcfg"
Push-Location (Split-Path -Parent $TMDLPath)
try {
    & $TMDLPath -psp-dump $modelPath -o $modelDumpPath
    if ($LASTEXITCODE -ne 0) {
        throw "Original frontend model '$ModelName' export failed with exit code $LASTEXITCODE"
    }
    & $TMDLPath -psp-material-properties $materialPropertiesPath -o $propertiesDumpPath
    if ($LASTEXITCODE -ne 0) {
        throw "Original frontend material properties export failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_frontend_instance_pack.py') `
    $inventoryPath $modelDumpPath $outputPath --model-name $ModelName --prefix $Prefix
if ($LASTEXITCODE -ne 0) {
    throw "Original frontend instance '$ModelName' conversion failed with exit code $LASTEXITCODE"
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$alphaModes = @()
$intermediatePaths = @($modelDumpPath, $propertiesDumpPath)
for ($textureIndex = 0; $textureIndex -lt $manifest.textures.Count; $textureIndex++) {
    $texture = $manifest.textures[$textureIndex]
    $ddsPath = Join-Path $outputPath "${Prefix}_texture_$textureIndex.dds"
    $ptexPath = Join-Path $outputPath $texture.texture
    $intermediatePaths += $ddsPath
    Push-Location (Split-Path -Parent $TMDLPath)
    try {
        & $TMDLPath -psp-texture $materialLibraryPath -texture-symbol 'instances_TTL' `
            -texture $texture.source_texture -o $ddsPath
        if ($LASTEXITCODE -ne 0) {
            throw "Original instance texture '$($texture.source_texture)' was not found in lib_startup/instances_TTL"
        }
    }
    finally {
        Pop-Location
    }

    $conversionOutput = @(& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_dds.py') `
        --max-width $TextureMaxWidth $ddsPath $ptexPath)
    if ($LASTEXITCODE -ne 0 -or $conversionOutput.Count -eq 0) {
        throw "Original instance texture '$($texture.source_texture)' conversion failed"
    }
    $textureSummary = $conversionOutput[-1] | ConvertFrom-Json
    $alphaModes += $textureSummary.alpha_mode
}

& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'finalize_frontend_terrain_pack.py') `
    $manifestPath $propertiesDumpPath $configPath --texture-alpha-modes ($alphaModes -join ',')
if ($LASTEXITCODE -ne 0) {
    throw "Original frontend instance '$ModelName' material finalization failed with exit code $LASTEXITCODE"
}

if ($RuntimeDirectory) {
    $runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
    New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null
    Get-ChildItem -LiteralPath $runtimePath -Filter "$Prefix*" -File -ErrorAction SilentlyContinue | Remove-Item -Force
    Get-ChildItem -LiteralPath $outputPath -Filter "$Prefix*.pmes" -File | Copy-Item -Destination $runtimePath -Force
    Get-ChildItem -LiteralPath $outputPath -Filter "$Prefix*.ptex" -File | Copy-Item -Destination $runtimePath -Force
    Copy-Item -LiteralPath $configPath -Destination $runtimePath -Force
    Write-Output "Published original frontend instance pack to $runtimePath"
}

if (-not $KeepIntermediate) {
    foreach ($intermediatePath in $intermediatePaths) {
        Remove-Item -LiteralPath $intermediatePath -Force -ErrorAction SilentlyContinue
    }
}

$geometryBytes = ($manifest.models | Measure-Object bytes -Sum).Sum
[pscustomobject]@{
    Model = $manifest.model_name
    ModelIndex = ('0x{0:X2}' -f [int]$manifest.model_index)
    SourceInstances = $manifest.source_instances
    SourceMeshes = $manifest.source_meshes
    DrawGroups = $manifest.models.Count
    Textures = $manifest.textures.Count
    GeometryBytes = $geometryBytes
    TextureMaxWidth = $TextureMaxWidth
    AlphaModes = (($alphaModes | Group-Object | ForEach-Object { "$($_.Name):$($_.Count)" }) -join ',')
}
