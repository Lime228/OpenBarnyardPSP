[CmdletBinding()]
param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string[]] $VisibilityRows = @(),
    [switch] $AllVisibilityRows,
    [switch] $ReuseConvertedModels,
    [ValidateSet(32, 64, 128, 256, 512)]
    [int] $TextureMaxWidth = 128
)

$ErrorActionPreference = 'Stop'
if ($AllVisibilityRows -and $VisibilityRows) { throw 'Choose -AllVisibilityRows or list particular -VisibilityRows.' }
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $assetRoot 'Workspace\EnvMain' }
if (-not $TMDLPath) {
    $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') -RepositoryRoot $repositoryRoot
}
$TMDLPath = (Resolve-Path -LiteralPath $TMDLPath).Path
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$python = & (Join-Path $PSScriptRoot 'ResolvePSPPython.ps1') -PythonPath $PythonPath
$pythonArguments = $python.Arguments
$installPath = Join-Path $assetRoot 'Install'

& $python.Path @pythonArguments (Join-Path $PSScriptRoot 'inventory_retail_sim_start.py') `
    --tmdl $TMDLPath $installPath $outputPath
if ($LASTEXITCODE -ne 0) { throw 'The original gameplay start inventory failed.' }

$start = Get-Content -LiteralPath (Join-Path $outputPath 'sim_start.json') -Raw | ConvertFrom-Json
$visibilityPath = Join-Path $outputPath 'envmain.vis.json'
if ($AllVisibilityRows) {
    $visibility = Get-Content -LiteralPath $visibilityPath -Raw | ConvertFrom-Json
    $VisibilityRows = @($visibility.sections | Where-Object collision | ForEach-Object name)
}
if (-not $VisibilityRows) { $VisibilityRows = @($start.initial_visibility_row) }

if ($start.initial_visibility_row -cin $VisibilityRows -or -not (Test-Path -LiteralPath (Join-Path $outputPath 'Sky'))) {
    & $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_retail_opening_sky.py') `
        --tmdl $TMDLPath --phase dawn $installPath (Join-Path $outputPath 'Sky')
    if ($LASTEXITCODE -ne 0) { throw 'The original gameplay sky packaging failed.' }
}

$locators = Join-Path $outputPath 'envmain-locators.json'
& $TMDLPath -psp-locators (Join-Path $installPath 'Data\Terrain\EnvMain\envmain_skel.trb') -o $locators
if ($LASTEXITCODE -ne 0) { throw 'The original gameplay locators could not be exported.' }
$convertedModels = @{}
if ($ReuseConvertedModels) {
    foreach ($directory in Get-ChildItem -LiteralPath $outputPath -Filter 'Terrain_*' -Directory) {
        $manifestPath = Join-Path $directory.FullName 'EnvMain_terrain.json'
        if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { continue }
        $previous = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        if ($previous.version -ne 1 -or $previous.terrain -cne 'EnvMain' -or $previous.texture_max_width -ne $TextureMaxWidth) { continue }
        foreach ($pack in $previous.packs) {
            if ($pack.status -cne 'converted') { continue }
            $model = [IO.Path]::GetFullPath((Join-Path $directory.FullName $pack.manifest))
            $relative = [IO.Path]::GetRelativePath($outputPath, $model)
            if ([IO.Path]::IsPathRooted($relative) -or ($relative -split '[/\\]') -contains '..' -or
                -not (Test-Path -LiteralPath $model -PathType Leaf)) { continue }
            $convertedModels["$($pack.source)|$($pack.material_library)"] = $model
        }
    }
}
$packedRows = [System.Collections.Generic.List[string]]::new()
$failedRows = [System.Collections.Generic.List[string]]::new()
foreach ($row in $VisibilityRows) {
    if ($row -cnotmatch '^[A-Za-z0-9_]{1,23}$') { throw "Invalid EnvMain VIS row name '$row'." }
    try {
        $terrainOutput = Join-Path $outputPath "Terrain_$row"
        & (Join-Path $PSScriptRoot 'ConvertRetailSS01Terrain.ps1') -TMDLPath $TMDLPath -PythonPath $PythonPath `
            -TerrainName EnvMain -VisibilityRow $row -OutputDirectory $terrainOutput -TextureMaxWidth $TextureMaxWidth `
            -ConvertedModelManifests $convertedModels
        & $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_sequence_terrain.py') `
            --envmain-dawn (Join-Path $terrainOutput 'EnvMain_terrain.json') (Join-Path $outputPath "TerrainPack_$row")
        if ($LASTEXITCODE -ne 0) { throw "The original gameplay terrain packaging failed for $row." }

        $filename = if ($row -ceq 'Barn') { 'envmain-instances.json' } else { "envmain-$row-instances.json" }
        & $python.Path @pythonArguments (Join-Path $PSScriptRoot 'inventory_frontend_instances.py') $locators `
            --model-definitions (Join-Path $repositoryRoot 'OpenBarnyard\Source\Helpers\AInstanceManagerModelList.inl') `
            --game-loader (Join-Path $repositoryRoot 'OpenBarnyard\Source\Helpers\AGameLoader.cpp') --install-root $installPath `
            --main-game-vis $visibilityPath --visibility-row $row --output (Join-Path $outputPath $filename)
        if ($LASTEXITCODE -ne 0) { throw "The original gameplay instance inventory failed for $row." }
        $packedRows.Add($row)
    }
    catch {
        if (-not $AllVisibilityRows) { throw }
        $failedRows.Add($row)
        Write-Warning $_.Exception.Message
    }
}
if ($packedRows.Count -gt 0) {
    $rowArguments = @(foreach ($row in $packedRows) { '--visibility-row'; $row })
    & $python.Path @pythonArguments (Join-Path $PSScriptRoot 'package_retail_main_game_instances.py') `
        --tmdl $TMDLPath --max-width ([Math]::Min(64, $TextureMaxWidth)) @rowArguments $installPath $outputPath
    if ($LASTEXITCODE -ne 0) { throw 'The original gameplay instance packaging failed for one or more requested rows.' }
}
if ($failedRows.Count) { throw "The EnvMain terrain conversion failed for $($failedRows -join ', ')." }
