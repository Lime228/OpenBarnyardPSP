[CmdletBinding()]
param([string] $PythonPath = '')

$arguments = @()
if (-not $PythonPath) {
    foreach ($name in @('python3', 'python')) {
        $candidate = Get-Command $name -ErrorAction SilentlyContinue
        if ($candidate -and $candidate.Source -notlike '*\WindowsApps\python*.exe') {
            $PythonPath = $candidate.Source
            break
        }
    }
    if (-not $PythonPath) {
        $codexPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
        if (Test-Path -LiteralPath $codexPython -PathType Leaf) {
            $PythonPath = $codexPython
        }
    }
    if (-not $PythonPath) {
        $launcher = Get-Command py -ErrorAction SilentlyContinue
        if ($launcher) {
            $PythonPath = $launcher.Source
            $arguments = @('-3')
        }
    }
}
if (-not $PythonPath -or -not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
    throw 'Python 3 was not found; pass -PythonPath explicitly'
}

@{
    Path = (Resolve-Path -LiteralPath $PythonPath).Path
    Arguments = $arguments
}
