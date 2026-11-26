[CmdletBinding()]
param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [ValidatePattern('^[A-Za-z0-9_]+$')]
    [string] $VisibilityRow = 'Dummy',
    [hashtable] $ConvertedModelManifests,
    [ValidateSet(32, 64, 128, 256, 512)]
    [int] $TextureMaxWidth = 128,
    [ValidatePattern('^[A-Za-z0-9_]+$')]
    [string] $TerrainName = 'ss01'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$installRoot = Join-Path $repositoryRoot 'Game\BarnyardPC\Install'
$terrainRoot = Join-Path $installRoot "Data\Terrain\$TerrainName"
$terrainPath = Join-Path $terrainRoot "$TerrainName.trb"
if (-not $OutputDirectory) {
    $folder = if ($VisibilityRow -ceq 'Dummy') { 'Terrain' } else { "Terrain_$VisibilityRow" }
    $OutputDirectory = Join-Path $repositoryRoot "Game\BarnyardPC\Workspace\$TerrainName\$folder"
}
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$visPath = Join-Path $outputPath "$TerrainName.vis.json"
if (-not $TMDLPath) {
    $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') -RepositoryRoot $repositoryRoot
}
$TMDLPath = (Resolve-Path -LiteralPath $TMDLPath).Path
New-Item -ItemType Directory -Force -Path $outputPath, (Join-Path (Split-Path -Parent $TMDLPath) 'Logs') | Out-Null

Push-Location (Split-Path -Parent $TMDLPath)
try {
    & $TMDLPath -psp-terrain-vis $terrainPath -o $visPath
    if ($LASTEXITCODE -ne 0) { throw "$TerrainName terrain visibility export failed." }
}
finally { Pop-Location }

$vis = Get-Content -LiteralPath $visPath -Raw | ConvertFrom-Json
$selection = @($vis.sections | Where-Object name -CEQ $VisibilityRow)
if ($vis.version -ne 2 -or $selection.Count -ne 1 -or $selection[0].visibility.Count -ne $vis.sections.Count) {
    throw "The $TerrainName terrain export is missing a complete '$VisibilityRow' visibility row."
}
$visibility = $selection[0].visibility
$converted = 0
$failures = 0
$packs = foreach ($index in 0..($vis.sections.Count - 1)) {
    $section = $vis.sections[$index]
    $requestedLod = $visibility[$index]
    if ($requestedLod -eq 'none') { continue }
    if ($requestedLod -notin 'high', 'low') { throw "Invalid visibility level '$requestedLod'." }
    $lod = $requestedLod
    if ($lod -eq 'high' -and $section.high.Count -eq 0) { $lod = 'low' }
    if ($null -eq $section.material_libraries) {
        throw 'This TMDL does not export VIS material libraries. Use an updated PSP host-tools build.'
    }
    $materialLibrary = $section.material_libraries.$lod -replace '\\+', '/'
    if ($materialLibrary -and ([IO.Path]::IsPathRooted($materialLibrary) -or ($materialLibrary -split '/') -contains '..')) {
        throw "Invalid material library path in $TerrainName VIS: $materialLibrary"
    }
    $materialPath = if ($materialLibrary) { Join-Path $installRoot $materialLibrary } else {
        Join-Path $terrainRoot "${TerrainName}_Common.ttl"
    }
    foreach ($entry in $section.$lod) {
        $model = [IO.Path]::GetFileNameWithoutExtension($entry.name)
        if ($model -notmatch '^[A-Za-z0-9_]+_L[01]Mod[0-9]+$') {
            throw "Unexpected $TerrainName terrain model name '$model'."
        }
        $directory = Join-Path $outputPath $model
        $modelLog = Join-Path $directory 'conversion.log'
        $pack = [ordered]@{
            section = $section.name
            requested_lod = $requestedLod
            lod = $lod
            material_library = $materialLibrary
            source = ($entry.name -replace '\\+', '/')
            glow = [bool]$entry.glow
            manifest = "$model/$model.json"
        }
        try {
            $key = "$($pack.source)|$materialLibrary"
            $manifestPath = Join-Path $directory "$model.json"
            if ($null -ne $ConvertedModelManifests -and $ConvertedModelManifests.ContainsKey($key)) {
                $manifestPath = $ConvertedModelManifests[$key]
                $pack.manifest = ([IO.Path]::GetRelativePath($outputPath, $manifestPath)) -replace '\\', '/'
            }
            else {
                New-Item -ItemType Directory -Force -Path $directory | Out-Null
                & (Join-Path $PSScriptRoot 'ConvertRetailFrontendDetail.ps1') -TerrainName $TerrainName `
                    -SourceModelName $model -SourceMaterialLibraryPath $materialPath `
                    -Prefix $model -OutputDirectory $directory -TextureMaxWidth $TextureMaxWidth `
                    -TMDLPath $TMDLPath -PythonPath $PythonPath *> $modelLog
            }
            $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            $assetDirectory = Split-Path -Parent $manifestPath
            $pack.source_meshes = $manifest.source_meshes
            $pack.models = $manifest.models.Count
            $pack.textures = $manifest.textures.Count
            $pack.geometry_bytes = ($manifest.models | ForEach-Object {
                (Get-Item -LiteralPath (Join-Path $assetDirectory $_.mesh)).Length
            } | Measure-Object -Sum).Sum
            $pack.texture_bytes = ($manifest.textures | ForEach-Object {
                (Get-Item -LiteralPath (Join-Path $assetDirectory $_.texture)).Length
            } | Measure-Object -Sum).Sum
            $pack.status = 'converted'
            if ($null -ne $ConvertedModelManifests) { $ConvertedModelManifests[$key] = $manifestPath }
            $converted++
        }
        catch {
            $pack.status = 'failed'
            $pack.error = $_.Exception.Message
            $pack.log = "$model/conversion.log"
            Write-Warning "$model failed. $($pack.error) See $modelLog"
            $failures++
        }
        $pack
    }
}

[ordered]@{
    version = 1
    terrain = $TerrainName
    visibility_row = $VisibilityRow
    texture_max_width = $TextureMaxWidth
    converted = $converted
    failed = $failures
    geometry_bytes = ($packs.geometry_bytes | Measure-Object -Sum).Sum
    texture_bytes = ($packs.texture_bytes | Measure-Object -Sum).Sum
    packs = @($packs)
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputPath "${TerrainName}_terrain.json") -Encoding utf8NoBOM
Write-Output "$TerrainName $VisibilityRow visibility row converted $converted model files. Failures: $failures."
if ($failures -gt 0) { throw "One or more $TerrainName terrain packs failed conversion." }
