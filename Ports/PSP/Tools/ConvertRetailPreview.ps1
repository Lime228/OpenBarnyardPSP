param(
    [string] $ManifestPath = '',
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string] $RuntimeDirectory = '',
    [switch] $KeepIntermediate
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
if (-not $ManifestPath) {
    $ManifestPath = Join-Path $assetRoot 'Manifests\first_skin.txt'
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot 'Workspace\Converted'
}

function Resolve-AssetPath([string] $Value) {
    if ([System.IO.Path]::IsPathRooted($Value)) {
        return [System.IO.Path]::GetFullPath($Value)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $assetRoot $Value))
}

if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Retail preview manifest was not found: $ManifestPath"
}

$manifest = @{}
foreach ($rawLine in Get-Content -LiteralPath $ManifestPath) {
    $line = $rawLine.Trim()
    if (-not $line -or $line.StartsWith('#')) {
        continue
    }
    $pair = $line.Split('=', 2)
    if ($pair.Count -ne 2 -or -not $pair[0].Trim()) {
        throw "Invalid manifest line: $rawLine"
    }
    $manifest[$pair[0].Trim()] = $pair[1].Trim()
}

foreach ($required in @('model', 'keylib', 'sequence')) {
    if (-not $manifest.ContainsKey($required) -or -not $manifest[$required]) {
        throw "Retail preview manifest requires '$required'"
    }
}

$model = Resolve-AssetPath $manifest.model
$keyLibrary = Resolve-AssetPath $manifest.keylib
if (-not (Test-Path -LiteralPath $model -PathType Leaf)) {
    throw "Retail model was not found: $model"
}
if (-not (Test-Path -LiteralPath $keyLibrary -PathType Leaf)) {
    throw "Retail key library was not found: $keyLibrary"
}

if (-not $TMDLPath) {
    $TMDLPath = Get-ChildItem (Join-Path $assetRoot 'Tools') -Filter 'TMDL.exe' -Recurse -File |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $TMDLPath -or -not (Test-Path -LiteralPath $TMDLPath -PathType Leaf)) {
    throw 'TMDL.exe was not found; pass -TMDLPath or download the OpenBarnyard-PSP-host-tools artifact'
}
$TMDLPath = [System.IO.Path]::GetFullPath($TMDLPath)

$pythonArguments = @()
if (-not $PythonPath) {
    $launcher = Get-Command py -ErrorAction SilentlyContinue
    if ($launcher) {
        $PythonPath = $launcher.Source
        $pythonArguments = @('-3')
    }
    foreach ($name in @('python3', 'python')) {
        if ($PythonPath) {
            break
        }
        $candidate = Get-Command $name -ErrorAction SilentlyContinue
        if ($candidate -and $candidate.Source -notlike '*\WindowsApps\python*.exe') {
            $PythonPath = $candidate.Source
        }
    }
    if (-not $PythonPath) {
        $codexPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
        if (Test-Path -LiteralPath $codexPython -PathType Leaf) {
            $PythonPath = $codexPython
        }
    }
}
if (-not $PythonPath -or -not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
    throw 'Python 3 was not found; pass -PythonPath explicitly'
}

$sampleRate = 20
if ($manifest.ContainsKey('sample_rate') -and $manifest.sample_rate) {
    $sampleRate = [int] $manifest.sample_rate
}
if ($sampleRate -lt 1 -or $sampleRate -gt 60) {
    throw "sample_rate must be between 1 and 60, got $sampleRate"
}

$arguments = @(
    (Join-Path $PSScriptRoot 'convert_barnyard_skin.py'),
    '--tmdl', $TMDLPath,
    '--model', $model,
    '--keylib', $keyLibrary,
    '--sequence', $manifest.sequence,
    '--sample-rate', $sampleRate,
    '--output', ([System.IO.Path]::GetFullPath($OutputDirectory)),
    '--runtime-preview'
)
if ($manifest.ContainsKey('material') -and $manifest.material) {
    $arguments += @('--material', $manifest.material)
}
if ($manifest.ContainsKey('texture') -and $manifest.texture) {
    $arguments += @('--texture', (Resolve-AssetPath $manifest.texture))
}
if ($KeepIntermediate) {
    $arguments += '--keep-intermediate'
}

& $PythonPath @pythonArguments @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Retail preview conversion failed with exit code $LASTEXITCODE"
}

if ($RuntimeDirectory) {
    $runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
    New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null
    Get-ChildItem ([System.IO.Path]::GetFullPath($OutputDirectory)) -Filter 'barnyard_preview.*' -File |
        Copy-Item -Destination $runtimePath -Force
    Write-Output "Published retail preview to $runtimePath"
}
