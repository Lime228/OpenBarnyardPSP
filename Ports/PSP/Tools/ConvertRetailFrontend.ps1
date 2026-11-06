param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string] $RuntimeDirectory = '',
    [switch] $KeepIntermediate
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot 'Workspace\Frontend'
}
if (-not $TMDLPath) {
    $TMDLPath = Get-ChildItem (Join-Path $assetRoot 'Tools') -Filter 'TMDL.exe' -Recurse -File |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $TMDLPath -or -not (Test-Path -LiteralPath $TMDLPath -PathType Leaf)) {
    throw 'TMDL.exe was not found; pass -TMDLPath explicitly'
}
$TMDLPath = (Resolve-Path -LiteralPath $TMDLPath).Path

$pythonArguments = @()
if (-not $PythonPath) {
    $launcher = Get-Command py -ErrorAction SilentlyContinue
    if ($launcher) {
        $PythonPath = $launcher.Source
        $pythonArguments = @('-3')
    }
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
}
if (-not $PythonPath -or -not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
    throw 'Python 3 was not found; pass -PythonPath explicitly'
}
$PythonPath = (Resolve-Path -LiteralPath $PythonPath).Path

$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$dumpPath = Join-Path $assetRoot 'Workspace\Dumps\Frontend'
New-Item -ItemType Directory -Force -Path $outputPath,$dumpPath,(Join-Path (Split-Path -Parent $TMDLPath) 'Logs') | Out-Null

$items = @(
    @{ Library = 'Install\Data\Matlibs\gui_FrontEnd.ttl'; Texture = 'Dialogs\barnyard_logo.tga'; Output = 'barnyard_logo' }
)
foreach ($index in 1..6) {
    $items += @{
        Library = 'Install\Data\Matlibs\bkg_E3_TITLE16.ttl'
        Texture = "bkg_E3_TITLE_$index.tga"
        Output = "barnyard_title_$($index - 1)"
    }
}

foreach ($item in $items) {
    $library = [System.IO.Path]::GetFullPath((Join-Path $assetRoot $item.Library))
    if (-not (Test-Path -LiteralPath $library -PathType Leaf)) {
        throw "Retail frontend library was not found: $library"
    }
    $ddsPath = Join-Path $dumpPath "$($item.Output).dds"
    $ptexPath = Join-Path $outputPath "$($item.Output).ptex"
    Push-Location (Split-Path -Parent $TMDLPath)
    try {
        & $TMDLPath -psp-texture $library -texture $item.Texture -o $ddsPath
        if ($LASTEXITCODE -ne 0) { throw "Frontend extraction failed for '$($item.Texture)'" }
    }
    finally {
        Pop-Location
    }
    & $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_dds.py') $ddsPath $ptexPath
    if ($LASTEXITCODE -ne 0) { throw "Frontend conversion failed for '$($item.Texture)'" }
}

if ($RuntimeDirectory) {
    $runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
    New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null
    Get-ChildItem $runtimePath -Filter 'barnyard_title_*.ptex' -File -ErrorAction SilentlyContinue | Remove-Item -Force
    Remove-Item -LiteralPath (Join-Path $runtimePath 'barnyard_logo.ptex') -Force -ErrorAction SilentlyContinue
    Get-ChildItem $outputPath -Filter 'barnyard_*.ptex' -File | Copy-Item -Destination $runtimePath -Force
    Write-Output "Published retail frontend to $runtimePath"
}

if (-not $KeepIntermediate) {
    Get-ChildItem $dumpPath -Filter '*.dds' -File | Remove-Item -Force
}
