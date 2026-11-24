[CmdletBinding()]
param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [ValidateSet(32, 64, 128, 256, 512)]
    [int] $TextureMaxWidth = 128
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$installRoot = Join-Path $repositoryRoot 'Game\BarnyardPC\Install'
$terrainPath = Join-Path $installRoot 'Data\Terrain\ss01\ss01.trb'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\SS01\Terrain'
}
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$visPath = Join-Path $outputPath 'ss01.vis.json'
if (-not $TMDLPath) {
    $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') -RepositoryRoot $repositoryRoot
}
$TMDLPath = (Resolve-Path -LiteralPath $TMDLPath).Path
New-Item -ItemType Directory -Force -Path $outputPath, (Join-Path (Split-Path -Parent $TMDLPath) 'Logs') | Out-Null

Push-Location (Split-Path -Parent $TMDLPath)
try {
    & $TMDLPath -psp-terrain-vis $terrainPath -o $visPath
    if ($LASTEXITCODE -ne 0) { throw 'SS01 terrain visibility export failed.' }
}
finally { Pop-Location }

$vis = Get-Content -LiteralPath $visPath -Raw | ConvertFrom-Json
$selection = $vis.sections | Where-Object name -CEQ 'Dummy'
if ($vis.version -ne 2 -or $selection.visibility.Count -ne $vis.sections.Count) {
    throw 'The SS01 terrain export is missing a complete Dummy visibility row.'
}
$converted = 0
$failures = 0
$packs = foreach ($index in 0..($vis.sections.Count - 1)) {
    $section = $vis.sections[$index]
    $requestedLod = $selection.visibility[$index]
    if ($requestedLod -eq 'none') { continue }
    if ($requestedLod -notin 'high', 'low') { throw "Invalid visibility level '$requestedLod'." }
    $lod = $requestedLod
    if ($lod -eq 'high' -and $section.high.Count -eq 0) { $lod = 'low' }
    if ($null -eq $section.material_libraries) {
        throw 'This TMDL does not export VIS material libraries. Use an updated PSP host-tools build.'
    }
    $materialLibrary = $section.material_libraries.$lod -replace '\\+', '/'
    if ($materialLibrary -and ([IO.Path]::IsPathRooted($materialLibrary) -or ($materialLibrary -split '/') -contains '..')) {
        throw "Invalid material library path in SS01 VIS: $materialLibrary"
    }
    $materialPath = if ($materialLibrary) { Join-Path $installRoot $materialLibrary } else {
        Join-Path $installRoot 'Data\Terrain\ss01\ss01_Common.ttl'
    }
    foreach ($entry in $section.$lod) {
        $model = [IO.Path]::GetFileNameWithoutExtension($entry.name)
        if ($model -notmatch '^[A-Za-z0-9_]+_L[01]Mod[0-9]+$') {
            throw "Unexpected SS01 terrain model name '$model'."
        }
        $directory = Join-Path $outputPath $model
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
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
            & (Join-Path $PSScriptRoot 'ConvertRetailFrontendDetail.ps1') -TerrainName ss01 `
                -SourceModelName $model -SourceMaterialLibraryPath $materialPath `
                -Prefix $model -OutputDirectory $directory -TextureMaxWidth $TextureMaxWidth `
                -TMDLPath $TMDLPath -PythonPath $PythonPath *> $modelLog
            $manifest = Get-Content -LiteralPath (Join-Path $directory "$model.json") -Raw | ConvertFrom-Json
            $pack.source_meshes = $manifest.source_meshes
            $pack.models = $manifest.models.Count
            $pack.textures = $manifest.textures.Count
            $pack.geometry_bytes = ($manifest.models | ForEach-Object {
                (Get-Item -LiteralPath (Join-Path $directory $_.mesh)).Length
            } | Measure-Object -Sum).Sum
            $pack.texture_bytes = ($manifest.textures | ForEach-Object {
                (Get-Item -LiteralPath (Join-Path $directory $_.texture)).Length
            } | Measure-Object -Sum).Sum
            $pack.status = 'converted'
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
    terrain = 'ss01'
    visibility_row = 'Dummy'
    texture_max_width = $TextureMaxWidth
    converted = $converted
    failed = $failures
    geometry_bytes = ($packs.geometry_bytes | Measure-Object -Sum).Sum
    texture_bytes = ($packs.texture_bytes | Measure-Object -Sum).Sum
    packs = @($packs)
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputPath 'ss01_terrain.json') -Encoding utf8NoBOM
Write-Output "SS01 Dummy visibility row converted $converted model files. Failures: $failures."
if ($failures -gt 0) { throw 'One or more SS01 terrain packs failed conversion.' }
