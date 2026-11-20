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
    $OutputDirectory = Join-Path $assetRoot 'Workspace\FrontendCamera'
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

$modelPath = Join-Path $assetRoot 'Install\Data\Models\EnvFrontEnd_Cam.trb'
$keyLibraryPath = Join-Path $assetRoot 'Install\Data\Assets\lib_frontend.trb'
foreach ($requiredPath in @($modelPath, $keyLibraryPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Retail frontend camera input was not found: $requiredPath"
    }
}

$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$dumpPath = Join-Path $assetRoot 'Workspace\Dumps\FrontendCamera\frontend_camera_loop.json'
$cameraPath = Join-Path $outputPath 'frontend_camera.pcam'
New-Item -ItemType Directory -Force -Path $outputPath,(Split-Path -Parent $dumpPath),(Join-Path (Split-Path -Parent $TMDLPath) 'Logs') | Out-Null

Push-Location (Split-Path -Parent $TMDLPath)
try {
    & $TMDLPath -psp-animation $modelPath `
        -keylib $keyLibraryPath `
        -keylib-symbol 'frontendchar_keylib' `
        -sequence 'camera_loop' `
        -sample-rate 30 `
        -o $dumpPath
    if ($LASTEXITCODE -ne 0) {
        throw "Retail frontend camera export failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_camera.py') $dumpPath $cameraPath
if ($LASTEXITCODE -ne 0) {
    throw "Retail frontend camera conversion failed with exit code $LASTEXITCODE"
}

if ($RuntimeDirectory) {
    $runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
    New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null
    Copy-Item -LiteralPath $cameraPath -Destination (Join-Path $runtimePath 'frontend_camera.pcam') -Force
    Write-Output "Published retail frontend camera to $runtimePath"
}

if (-not $KeepIntermediate) {
    Remove-Item -LiteralPath $dumpPath -Force -ErrorAction SilentlyContinue
}
