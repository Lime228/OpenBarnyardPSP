[CmdletBinding()]
param(
    [string[]] $ModelNames = @(),
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string] $ReportPath = '',
    [ValidateRange(16, 256)]
    [int] $TextureMaxWidth = 64,
    [switch] $AllowFailures
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot 'Workspace\FrontendInstances'
}
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
if (-not $ReportPath) {
    $ReportPath = Join-Path $outputPath 'frontend_instances.batch.json'
}
$reportFullPath = [System.IO.Path]::GetFullPath($ReportPath)

if (-not $TMDLPath) {
    $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') -RepositoryRoot $repositoryRoot
}
if (-not $TMDLPath -or -not (Test-Path -LiteralPath $TMDLPath -PathType Leaf)) {
    throw 'TMDL.exe was not found; pass -TMDLPath or download the OpenBarnyard-PSP-host-tools artifact'
}
$TMDLPath = (Resolve-Path -LiteralPath $TMDLPath).Path

$inventoryArguments = @{ TMDLPath = $TMDLPath }
if ($PythonPath) { $inventoryArguments.PythonPath = $PythonPath }
& (Join-Path $PSScriptRoot 'InventoryRetailFrontendInstances.ps1') @inventoryArguments | Out-Null
$inventoryPath = Join-Path $outputPath 'frontend_instances.inventory.json'
if (-not (Test-Path -LiteralPath $inventoryPath -PathType Leaf)) {
    throw "Original frontend instance inventory was not found: $inventoryPath"
}
$inventory = Get-Content -LiteralPath $inventoryPath -Raw | ConvertFrom-Json

$residentSections = @($inventory.resident_section_names)
$residentNames = @(
    $inventory.instances |
        Where-Object { $residentSections -contains $_.vis_section_name } |
        ForEach-Object { [string] $_.model_name } |
        Sort-Object -Unique
)
if (-not $ModelNames -or $ModelNames.Count -eq 0) {
    $residentNameLookup = @{}
    foreach ($residentName in $residentNames) {
        $residentNameLookup[$residentName.ToLowerInvariant()] = $true
    }
    $ModelNames = @(
        $inventory.frontend_models |
            Where-Object { $residentNameLookup.ContainsKey(([string] $_.name).ToLowerInvariant()) } |
            ForEach-Object { [string] $_.name }
    )
}
if (-not $ModelNames -or $ModelNames.Count -eq 0) {
    throw 'No resident original frontend instance models were selected'
}

$alphaModeNames = @('opaque', 'mask', 'blend', 'additive')
$modelReports = @()
foreach ($modelName in $ModelNames) {
    $modelReport = [ordered]@{
        name = $modelName
        status = 'failed'
        model_index = $null
        source_instances = 0
        source_meshes = 0
        draw_groups = 0
        textures = 0
        geometry_bytes = 0
        alpha_modes = @()
        materials = @()
        source_textures = @()
        error = $null
    }
    try {
        if (-not $modelName -or $modelName -notmatch '^[A-Za-z0-9_]+$') {
            throw "Invalid original frontend instance model name: $modelName"
        }
        $modelOutputPath = Join-Path $outputPath $modelName
        $conversionArguments = @{
            ModelName = $modelName
            Prefix = "frontend_instances_$($modelName.ToLowerInvariant())"
            TMDLPath = $TMDLPath
            OutputDirectory = $modelOutputPath
            TextureMaxWidth = $TextureMaxWidth
            SkipInventory = $true
        }
        if ($PythonPath) { $conversionArguments.PythonPath = $PythonPath }
        & (Join-Path $PSScriptRoot 'ConvertRetailFrontendInstanceModel.ps1') @conversionArguments | Out-Null

        $prefix = $conversionArguments.Prefix
        $manifestPath = Join-Path $modelOutputPath "$prefix.json"
        $configPath = Join-Path $modelOutputPath "$prefix.pcfg"
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        $config = [System.IO.File]::ReadAllBytes($configPath)
        $configModelCount = [System.BitConverter]::ToUInt32($config, 8)
        $configTextureCount = [System.BitConverter]::ToUInt32($config, 12)
        if ($config.Length -ne (24 + $configModelCount * 20) -or
            $configModelCount -ne $manifest.models.Count -or
            $configTextureCount -ne $manifest.textures.Count) {
            throw "Converted original frontend instance config is inconsistent: $configPath"
        }

        $alphaModes = @()
        for ($modelIndex = 0; $modelIndex -lt $configModelCount; $modelIndex++) {
            $alphaMode = [System.BitConverter]::ToUInt32($config, 24 + $modelIndex * 20 + 8)
            if ($alphaMode -ge $alphaModeNames.Count) {
                throw "Converted original frontend instance alpha mode is invalid: $alphaMode"
            }
            $alphaModes += $alphaModeNames[$alphaMode]
        }

        $modelReport.status = 'converted'
        $modelReport.model_index = [int] $manifest.model_index
        $modelReport.source_instances = [int] $manifest.source_instances
        $modelReport.source_meshes = [int] $manifest.source_meshes
        $modelReport.draw_groups = [int] $manifest.models.Count
        $modelReport.textures = [int] $manifest.textures.Count
        $modelReport.geometry_bytes = [int] (($manifest.models | Measure-Object bytes -Sum).Sum)
        $modelReport.alpha_modes = @($alphaModes | Sort-Object -Unique)
        $modelReport.materials = @($manifest.models.material | Sort-Object -Unique)
        $modelReport.source_textures = @($manifest.textures.source_texture | Sort-Object -Unique)
    }
    catch {
        $modelReport.error = $_.Exception.Message
    }
    $modelReports += [pscustomobject] $modelReport
}

$converted = @($modelReports | Where-Object status -eq 'converted')
$failed = @($modelReports | Where-Object status -eq 'failed')
$report = [ordered]@{
    version = 1
    resident_model_count = [int] $inventory.resident_used_model_count
    resident_instance_count = [int] $inventory.resident_instance_count
    selected_model_count = $ModelNames.Count
    converted_model_count = $converted.Count
    failed_model_count = $failed.Count
    converted_instance_count = [int] (($converted | Measure-Object source_instances -Sum).Sum)
    draw_groups = [int] (($converted | Measure-Object draw_groups -Sum).Sum)
    textures = [int] (($converted | Measure-Object textures -Sum).Sum)
    geometry_bytes = [int64] (($converted | Measure-Object geometry_bytes -Sum).Sum)
    models = $modelReports
}
New-Item -ItemType Directory -Path (Split-Path -Parent $reportFullPath) -Force | Out-Null
$temporaryReportPath = "$reportFullPath.tmp"
[System.IO.File]::WriteAllText(
    $temporaryReportPath,
    ($report | ConvertTo-Json -Depth 8) + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false)
)
Move-Item -LiteralPath $temporaryReportPath -Destination $reportFullPath -Force

[pscustomobject]@{
    SelectedModels = $report.selected_model_count
    ConvertedModels = $report.converted_model_count
    FailedModels = $report.failed_model_count
    ConvertedInstances = $report.converted_instance_count
    DrawGroups = $report.draw_groups
    Textures = $report.textures
    GeometryBytes = $report.geometry_bytes
    Report = $reportFullPath
}

if ($failed.Count -gt 0 -and -not $AllowFailures) {
    throw "$($failed.Count) original frontend instance model conversions failed; see $reportFullPath"
}
