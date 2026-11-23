[CmdletBinding()]
param(
    [string] $TMDLPath = '',
    [string] $OutputDirectory = '',
    [string] $ReportPath = '',
    [switch] $ReuseInventories,
    [switch] $AllowFailures
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
$installRoot = Join-Path $assetRoot 'Install'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot 'Workspace\TextureLibraries'
}
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
if (-not $ReportPath) {
    $ReportPath = Join-Path $outputPath 'texture_libraries.inventory.json'
}
$reportFullPath = [System.IO.Path]::GetFullPath($ReportPath)

if (-not $TMDLPath) {
    $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') -RepositoryRoot $repositoryRoot
}
if (-not $TMDLPath -or -not (Test-Path -LiteralPath $TMDLPath -PathType Leaf)) {
    throw 'TMDL.exe was not found; pass -TMDLPath or download the OpenBarnyard-PSP-host-tools artifact'
}
$TMDLPath = (Resolve-Path -LiteralPath $TMDLPath).Path
if (-not (Test-Path -LiteralPath $installRoot -PathType Container)) {
    throw "Original Barnyard install was not found: $installRoot"
}

New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
$toolDirectory = Split-Path -Parent $TMDLPath
$libraries = @(
    Get-ChildItem -LiteralPath $installRoot -Recurse -File -Filter '*.ttl' |
        Sort-Object FullName
)
if ($libraries.Count -eq 0) {
    throw "No original Barnyard TTL libraries were found under $installRoot"
}

function Invoke-TMDLInventory {
    param([string[]] $Arguments, [string] $Destination, [datetime] $SourceWriteTime)

    if ($ReuseInventories -and (Test-Path -LiteralPath $Destination -PathType Leaf) -and
        (Get-Item -LiteralPath $Destination).LastWriteTimeUtc -ge $SourceWriteTime) {
        return (Get-Content -LiteralPath $Destination -Raw | ConvertFrom-Json)
    }

    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
    $pending = "$Destination.tmp"
    Push-Location $toolDirectory
    try {
        & $TMDLPath @Arguments -o $pending | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "TMDL inventory failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
    $inventory = Get-Content -LiteralPath $pending -Raw | ConvertFrom-Json
    Move-Item -LiteralPath $pending -Destination $Destination -Force
    return $inventory
}

function Get-LibraryInventory {
    param([System.IO.FileInfo] $Library, [string] $Symbol)

    $relativePath = [System.IO.Path]::GetRelativePath($installRoot, $Library.FullName)
    $inventoryPath = if ($Symbol -eq 'TTL') {
        Join-Path $outputPath "$relativePath.json"
    } else {
        Join-Path $outputPath "$relativePath.d\$Symbol.json"
    }
    $libraryReport = [ordered]@{
        path = $relativePath.Replace('/', '\')
        symbol = $Symbol
        status = 'failed'
        pack_name = $null
        texture_count = 0
        textures = @()
        error = $null
    }

    try {
        $dumpArguments = @('-psp-textures', $Library.FullName, '-texture-symbol', $Symbol)
        $inventory = Invoke-TMDLInventory -Arguments $dumpArguments -Destination $inventoryPath -SourceWriteTime $Library.LastWriteTimeUtc
        if ([int] $inventory.version -ne 1 -or
            [int] $inventory.texture_count -ne @($inventory.textures).Count) {
            throw 'TMDL returned an inconsistent texture inventory'
        }

        $libraryReport.status = 'inventoried'
        $libraryReport.pack_name = [string] $inventory.pack_name
        $libraryReport.texture_count = [int] $inventory.texture_count
        $libraryReport.textures = @($inventory.textures)
    }
    catch {
        $libraryReport.error = $_.Exception.Message
    }
    [pscustomobject] $libraryReport
}

$packFailures = [System.Collections.Generic.List[object]]::new()
$packs = @(Get-ChildItem -LiteralPath (Join-Path $installRoot 'Data\Assets') -File -Filter '*.trb' -Recurse | Sort-Object FullName)
$libraryReports = @(
    foreach ($library in $libraries) {
        Get-LibraryInventory -Library $library -Symbol 'TTL'
    }
    foreach ($pack in $packs) {
        $relativePath = [System.IO.Path]::GetRelativePath($installRoot, $pack.FullName)
        try {
            $destination = Join-Path $outputPath "$relativePath.symbols.json"
            $inventory = Invoke-TMDLInventory -Arguments @('-psp-symbols', $pack.FullName) -Destination $destination -SourceWriteTime $pack.LastWriteTimeUtc
            if ($inventory.version -ne 1 -or $null -eq $inventory.symbols) {
                throw 'TMDL returned an invalid symbol list'
            }
        }
        catch {
            $packFailures.Add(@{ path = $relativePath; error = $_.Exception.Message })
            continue
        }
        foreach ($entry in $inventory.symbols | Where-Object { $_.name -match '^[a-zA-Z0-9_]+_TTL$' }) {
            Get-LibraryInventory -Library $pack -Symbol $entry.name
        }
    }
)

$inventoried = @($libraryReports | Where-Object status -eq 'inventoried')
$failed = @($libraryReports | Where-Object status -eq 'failed')
$report = [ordered]@{
    version = 2
    standalone_library_count = $libraries.Count
    asset_pack_count = $packs.Count
    failed_asset_packs = @($packFailures.ToArray())
    library_count = $libraryReports.Count
    inventoried_library_count = $inventoried.Count
    failed_library_count = $failed.Count
    texture_count = [int] (($inventoried | Measure-Object texture_count -Sum).Sum)
    libraries = $libraryReports
}
New-Item -ItemType Directory -Path (Split-Path -Parent $reportFullPath) -Force | Out-Null
$temporaryReportPath = "$reportFullPath.tmp"
[System.IO.File]::WriteAllText(
    $temporaryReportPath,
    ($report | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false)
)
Move-Item -LiteralPath $temporaryReportPath -Destination $reportFullPath -Force

[pscustomobject]@{
    Libraries = $report.library_count
    InventoriedLibraries = $report.inventoried_library_count
    FailedLibraries = $report.failed_library_count
    Textures = $report.texture_count
    Report = $reportFullPath
}

if (($failed.Count -gt 0 -or $packFailures.Count -gt 0) -and -not $AllowFailures) {
    throw "$($failed.Count) texture libraries and $($packFailures.Count) asset packs failed; see $reportFullPath"
}
