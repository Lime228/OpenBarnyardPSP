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

$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$dumpPath = Join-Path $assetRoot 'Workspace\Dumps\Frontend'
New-Item -ItemType Directory -Force -Path $outputPath,$dumpPath,(Join-Path (Split-Path -Parent $TMDLPath) 'Logs') | Out-Null

$items = @(
    @{ Library = 'Install\Data\Matlibs\gui_FrontEnd.ttl'; Texture = 'Dialogs\barnyard_logo.tga'; Output = 'barnyard_logo' },
    @{ Library = 'Install\Data\Matlibs\gui_Fonts.ttl'; Texture = 'Fonts\Rekord_Black_26.tga'; Output = 'Rekord_Black_26'; MaxWidth = 512 },
    @{ Library = 'Install\Data\Matlibs\gui_HUD.ttl'; Texture = 'HUD\Panel_01.tga'; Output = 'Panel_01' },
    @{ Library = 'Install\Data\Matlibs\gui_HUD.ttl'; Texture = 'HUD\Border_Top_Left.tga'; Output = 'Border_Top_Left' },
    @{ Library = 'Install\Data\Matlibs\gui_HUD.ttl'; Texture = 'HUD\Border_Top_Right.tga'; Output = 'Border_Top_Right' },
    @{ Library = 'Install\Data\Matlibs\gui_HUD.ttl'; Texture = 'HUD\Border_Horizontal.tga'; Output = 'Border_Horizontal' },
    @{ Library = 'Install\Data\Matlibs\gui_HUD.ttl'; Texture = 'HUD\Border_Vertical.tga'; Output = 'Border_Vertical' },
    @{ Library = 'Install\Data\Matlibs\gui_HUD.ttl'; Texture = 'HUD\Border_MIddle.tga'; Output = 'Border_MIddle' },
    @{ Library = 'Install\Data\Matlibs\gui_HUD.ttl'; Texture = 'HUD\Border_Bottom_Left.tga'; Output = 'Border_Bottom_Left' },
    @{ Library = 'Install\Data\Matlibs\gui_HUD.ttl'; Texture = 'HUD\Border_Bottom_Right.tga'; Output = 'Border_Bottom_Right' }
)

foreach ($picture in @('bkg_by_legal1', 'bkg_Bink_Big')) {
    foreach ($tile in 1..6) {
        $items += @{
            Library = "Install\Data\Matlibs\$picture.ttl"
            Texture = "${picture}_${tile}.tga"
            Output = "${picture}_${tile}"
        }
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
    $maxWidth = if ($item.MaxWidth) { $item.MaxWidth } else { 256 }
    & $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_dds.py') --max-width $maxWidth $ddsPath $ptexPath
    if ($LASTEXITCODE -ne 0) { throw "Frontend conversion failed for '$($item.Texture)'" }
}

if ($RuntimeDirectory) {
    $runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
    New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null
    foreach ($item in $items) {
        $fileName = "$($item.Output).ptex"
        Remove-Item -LiteralPath (Join-Path $runtimePath $fileName) -Force -ErrorAction SilentlyContinue
        Copy-Item -LiteralPath (Join-Path $outputPath $fileName) -Destination $runtimePath -Force
    }
    Write-Output "Published retail frontend to $runtimePath"
}

if (-not $KeepIntermediate) {
    Get-ChildItem $dumpPath -Filter '*.dds' -File | Remove-Item -Force
}
