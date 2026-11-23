param(
    [string[]] $ModelNames = @('fncw01', 'fncw02', 'fncw03'),
    [string] $Prefix = 'frontend_instances_fncw',
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string] $RuntimeDirectory = '',
    [switch] $ReuseConvertedModels
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $ModelNames -or $ModelNames.Count -lt 1) {
    throw 'At least one original frontend instance model is required'
}
if ($Prefix -notmatch '^[A-Za-z0-9_]+$') {
    throw 'Prefix must contain only ASCII letters, digits and underscores'
}
foreach ($modelName in $ModelNames) {
    if ($modelName -notmatch '^[A-Za-z0-9_]+$') {
        throw "Invalid original frontend instance model name: $modelName"
    }
}

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot "Game\BarnyardPC\Workspace\FrontendInstances\$Prefix"
}
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$workspacePath = Split-Path -Parent $outputPath
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

if (-not $ReuseConvertedModels) {
    if (-not $TMDLPath) {
        $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') -RepositoryRoot $repositoryRoot
    }
    if (-not $TMDLPath -or -not (Test-Path -LiteralPath $TMDLPath -PathType Leaf)) {
        throw 'TMDL.exe was not found; pass -TMDLPath or download the OpenBarnyard-PSP-host-tools artifact'
    }
    $TMDLPath = (Resolve-Path -LiteralPath $TMDLPath).Path
}

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

$sourceManifests = @()
foreach ($modelName in $ModelNames) {
    $sourceDirectory = Join-Path $workspacePath $modelName
    $sourceManifestPath = Join-Path $sourceDirectory "frontend_instances_$modelName.json"
    if (-not $ReuseConvertedModels) {
        & (Join-Path $PSScriptRoot 'ConvertRetailFrontendInstanceModel.ps1') `
            -ModelName $modelName `
            -TMDLPath $TMDLPath `
            -PythonPath $PythonPath `
            -OutputDirectory $sourceDirectory | Out-Null
    }
    if (-not (Test-Path -LiteralPath $sourceManifestPath -PathType Leaf)) {
        throw "Converted original frontend instance model was not found: $sourceManifestPath"
    }
    $sourceManifests += $sourceManifestPath
}

& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'combine_frontend_instance_packs.py') `
    $outputPath @sourceManifests --prefix $Prefix
if ($LASTEXITCODE -ne 0) {
    throw "Original frontend instance set conversion failed with exit code $LASTEXITCODE"
}

$manifestPath = Join-Path $outputPath "$Prefix.json"
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($RuntimeDirectory) {
    $runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
    New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null
    Get-ChildItem -LiteralPath $runtimePath -Filter "$Prefix*" -File -ErrorAction SilentlyContinue | Remove-Item -Force
    Get-ChildItem -LiteralPath $outputPath -Filter "$Prefix*.pmes" -File | Copy-Item -Destination $runtimePath -Force
    Get-ChildItem -LiteralPath $outputPath -Filter "$Prefix*.ptex" -File | Copy-Item -Destination $runtimePath -Force
    Copy-Item -LiteralPath (Join-Path $outputPath "$Prefix.pcfg") -Destination $runtimePath -Force
    Write-Output "Published original frontend instance set to $runtimePath"
}

[pscustomobject]@{
    Prefix = $manifest.prefix
    SourceModels = $manifest.source_model_count
    SourceInstances = $manifest.source_instances
    DrawGroups = $manifest.models.Count
    Textures = $manifest.textures.Count
    GeometryBytes = ($manifest.models | Measure-Object bytes -Sum).Sum
}
