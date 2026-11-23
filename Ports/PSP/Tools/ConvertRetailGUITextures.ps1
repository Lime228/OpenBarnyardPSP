[CmdletBinding()]
param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [ValidateSet(16, 32, 64, 128, 256, 512)]
    [int] $TextureMaxWidth = 512,
    [switch] $ReuseInventories,
    [switch] $KeepIntermediate,
    [switch] $AllowFailures
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
$installRoot = Join-Path $assetRoot 'Install'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot 'Workspace\GUITextures'
}
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$dumpPath = Join-Path $outputPath 'Dumps'
$convertedPath = Join-Path $outputPath 'Converted'
$sectionInventoryPath = Join-Path $outputPath 'gui_sections.inventory.json'
$planPath = Join-Path $outputPath 'gui_textures.plan.json'
$reportPath = Join-Path $outputPath 'gui_textures.batch.json'
$textureLibraryDirectory = Join-Path $assetRoot 'Workspace\TextureLibraries'
$textureLibraryReportPath = Join-Path $textureLibraryDirectory 'texture_libraries.inventory.json'

if (-not $TMDLPath) {
    $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') -RepositoryRoot $repositoryRoot
}
if (-not $TMDLPath -or -not (Test-Path -LiteralPath $TMDLPath -PathType Leaf)) {
    throw 'TMDL.exe was not found; pass -TMDLPath or download the OpenBarnyard-PSP-host-tools artifact'
}
$TMDLPath = (Resolve-Path -LiteralPath $TMDLPath).Path

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

$texsecPath = Join-Path $installRoot 'Data\GUI\texsec.trb'
if (-not (Test-Path -LiteralPath $texsecPath -PathType Leaf)) {
    throw "Original GUI texture section table was not found: $texsecPath"
}
New-Item -ItemType Directory -Path $outputPath,$dumpPath,$convertedPath -Force | Out-Null

$libraryArguments = @{
    TMDLPath = $TMDLPath
    OutputDirectory = $textureLibraryDirectory
    ReportPath = $textureLibraryReportPath
}
if ($ReuseInventories) { $libraryArguments.ReuseInventories = $true }
$libraryArguments.AllowFailures = $true
& (Join-Path $PSScriptRoot 'InventoryRetailTextureLibraries.ps1') @libraryArguments | Out-Null
$libraries = Get-Content -LiteralPath $textureLibraryReportPath -Raw | ConvertFrom-Json

if (-not $ReuseInventories -or -not (Test-Path -LiteralPath $sectionInventoryPath -PathType Leaf) -or
    (Get-Item -LiteralPath $sectionInventoryPath).LastWriteTimeUtc -lt (Get-Item -LiteralPath $texsecPath).LastWriteTimeUtc) {
    Push-Location (Split-Path -Parent $TMDLPath)
    try {
        & $TMDLPath -psp-gui-sections $texsecPath -o $sectionInventoryPath | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "TMDL GUI section inventory failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'plan_gui_texture_batch.py') `
    $sectionInventoryPath $textureLibraryReportPath $planPath | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Original GUI texture batch planning failed with exit code $LASTEXITCODE"
}
$plan = Get-Content -LiteralPath $planPath -Raw | ConvertFrom-Json

$itemReports = @(foreach ($item in $plan.items) {
    $itemReport = [ordered]@{
        texture = [string] $item.texture
        output = [string] $item.output
        source_sections = [int] $item.section_count
        source_library = $item.source_library
        source_symbol = $item.source_symbol
        source_texture = $item.source_texture
        status = [string] $item.status
        width = 0
        height = 0
        bytes = 0
        alpha_mode = $null
        error = $null
    }

    if ($itemReport.status -eq 'resolved') {
        try {
            $destinationPath = Join-Path $convertedPath $item.output
            $pending = "$destinationPath.tmp"
            $sourceLibraryPath = Join-Path $installRoot $item.source_library
            $ddsPath = Join-Path $dumpPath ([System.IO.Path]::ChangeExtension($item.output, '.dds'))
            New-Item -ItemType Directory -Path (Split-Path -Parent $ddsPath) -Force | Out-Null
            Push-Location (Split-Path -Parent $TMDLPath)
            try {
                & $TMDLPath -psp-texture $sourceLibraryPath -texture-symbol $item.source_symbol -texture $item.source_texture -o $ddsPath | Out-Null
                if ($LASTEXITCODE -ne 0) {
                    throw "TMDL GUI texture extraction failed with exit code $LASTEXITCODE"
                }
            }
            finally {
                Pop-Location
            }

            $conversionOutput = @(
                & $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_dds.py') `
                    --max-width $TextureMaxWidth $ddsPath $pending 2>&1
            )
            if ($LASTEXITCODE -ne 0 -or $conversionOutput.Count -eq 0) {
                throw "PSP GUI conversion failed: $($conversionOutput | Select-Object -Last 1)"
            }
            $summary = $conversionOutput[-1] | ConvertFrom-Json
            Move-Item -LiteralPath $pending -Destination $destinationPath -Force
            if (-not $KeepIntermediate) {
                Remove-Item -LiteralPath $ddsPath
            }
            $itemReport.status = 'converted'
            $itemReport.width = [int] $summary.width
            $itemReport.height = [int] $summary.height
            $itemReport.bytes = [int64] $summary.bytes
            $itemReport.alpha_mode = [string] $summary.alpha_mode
        }
        catch {
            $itemReport.status = 'failed'
            $itemReport.error = $_.Exception.Message
        }
    }
    [pscustomobject] $itemReport
})

$converted = @($itemReports | Where-Object status -eq 'converted')
$failed = @($itemReports | Where-Object status -eq 'failed')
$unresolved = @($itemReports | Where-Object status -eq 'unresolved')
$report = [ordered]@{
    version = 2
    section_count = [int] $plan.section_count
    source_texture_count = [int] $plan.texture_count
    converted_texture_count = @($converted.texture | Sort-Object -Unique).Count
    source_variant_count = [int] $plan.source_variant_count
    converted_variant_count = $converted.Count
    failed_variant_count = $failed.Count
    unresolved_texture_count = $unresolved.Count
    failed_library_count = [int] $libraries.failed_library_count
    failed_asset_pack_count = @($libraries.failed_asset_packs).Count
    converted_bytes = [int64] (($converted | Measure-Object bytes -Sum).Sum)
    items = $itemReports
}
$temporaryReportPath = "$reportPath.tmp"
[System.IO.File]::WriteAllText(
    $temporaryReportPath,
    ($report | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false)
)
Move-Item -LiteralPath $temporaryReportPath -Destination $reportPath -Force

[pscustomobject]@{
    Sections = $report.section_count
    SourceTextures = $report.source_texture_count
    ConvertedTextures = $report.converted_texture_count
    SourceVariants = $report.source_variant_count
    ConvertedVariants = $report.converted_variant_count
    FailedVariants = $report.failed_variant_count
    UnresolvedTextures = $report.unresolved_texture_count
    FailedLibraries = $report.failed_library_count
    FailedAssetPacks = $report.failed_asset_pack_count
    ConvertedBytes = $report.converted_bytes
    Report = $reportPath
}

$blockingCount = $failed.Count + $unresolved.Count + $report.failed_library_count + $report.failed_asset_pack_count
if ($blockingCount -gt 0 -and -not $AllowFailures) {
    throw "$blockingCount original GUI texture batch items failed; see $reportPath"
}
