[CmdletBinding()]
param(
    [string] $InstallDirectory = '',
    [string] $OutputPath = '',
    [string] $PythonPath = ''
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
if (-not $InstallDirectory) {
    $InstallDirectory = Join-Path $assetRoot 'Install'
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $assetRoot 'Workspace\Inventory\retail_install.inventory.json'
}

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

$installPath = [System.IO.Path]::GetFullPath($InstallDirectory)
$reportPath = [System.IO.Path]::GetFullPath($OutputPath)
if (-not (Test-Path -LiteralPath $installPath -PathType Container)) {
    throw "Original Barnyard install was not found: $installPath"
}

& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'inventory_retail_install.py') $installPath $reportPath
if ($LASTEXITCODE -ne 0) {
    throw "Original Barnyard install inventory failed with exit code $LASTEXITCODE"
}
