param(
    [switch] $Force
)

$ErrorActionPreference = 'Stop'

$version = 'v1.20.4-4696-g79e845c4d0fc'
$archiveName = "ppsspp_win_$version.zip"
$expectedSHA256 = '19BAD024F7732A89914186037FC481214ED630DA1A49317E401DA64C15FDEE31'
$downloadURL = "https://builds.ppsspp.org/builds/$version/$archiveName"

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$runtimeRoot = Join-Path $repositoryRoot 'Runtime'
$archivePath = Join-Path $runtimeRoot "Downloads\$archiveName"
$destinationPath = Join-Path $runtimeRoot 'Tools\PPSSPPDev'
$executablePath = Join-Path $destinationPath 'PPSSPPWindows64.exe'

if ((Test-Path -LiteralPath $executablePath -PathType Leaf) -and -not $Force) {
    Write-Output "PPSSPP $version is already installed: $executablePath"
    return
}

New-Item -ItemType Directory -Path (Split-Path -Parent $archivePath) -Force | Out-Null
New-Item -ItemType Directory -Path $destinationPath -Force | Out-Null

if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
    Write-Output "Downloading official PPSSPP development build $version..."
    Invoke-WebRequest -Uri $downloadURL -OutFile $archivePath
}

$actualSHA256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
if ($actualSHA256 -ne $expectedSHA256) {
    throw "PPSSPP archive checksum mismatch. Expected $expectedSHA256, got $actualSHA256. Remove the archive and retry: $archivePath"
}

Expand-Archive -LiteralPath $archivePath -DestinationPath $destinationPath -Force

$memstickPath = Join-Path $destinationPath 'memstick'
if (-not (Test-Path -LiteralPath $memstickPath -PathType Container)) {
    $existingMemstickPath = Join-Path $runtimeRoot 'Tools\PPSSPP\memstick'
    if (Test-Path -LiteralPath $existingMemstickPath -PathType Container) {
        Copy-Item -LiteralPath $existingMemstickPath -Destination $memstickPath -Recurse
    }
    else {
        $systemPath = Join-Path $memstickPath 'PSP\SYSTEM'
        New-Item -ItemType Directory -Path $systemPath -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $systemPath 'ppsspp.ini') -Value @'
[General]
FirstRun = False
RemoteISOPort = 7755
RemoteDebuggerOnStartup = True
PauseOnLostFocus = False
PauseWhenMinimized = False
'@
    }
}

Write-Output "Installed PPSSPP ${version}: $executablePath"
