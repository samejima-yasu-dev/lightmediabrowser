[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build'),
    [string]$QtRoot = $env:QT_ROOT_DIR,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\dist')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($QtRoot)) {
    throw 'QT_ROOT_DIR must point to the installed Qt prefix.'
}
$buildPath = (Resolve-Path $BuildDirectory).Path
$qtPath = (Resolve-Path $QtRoot).Path
$executable = Join-Path $buildPath 'lightmediabrowser.exe'
$deployTool = Join-Path $qtPath 'bin\windeployqt.exe'
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)

if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Application executable not found: $executable"
}
if (-not (Test-Path -LiteralPath $deployTool -PathType Leaf)) {
    throw "windeployqt not found: $deployTool"
}

$licenseDirectories = @(
    $env:QT_LICENSE_DIR,
    (Join-Path $qtPath 'licenses'),
    (Join-Path $qtPath 'Licenses'),
    (Join-Path (Split-Path $qtPath -Parent) 'licenses'),
    (Join-Path (Split-Path $qtPath -Parent) 'Licenses'),
    (Join-Path (Split-Path (Split-Path $qtPath -Parent) -Parent) 'licenses'),
    (Join-Path (Split-Path (Split-Path $qtPath -Parent) -Parent) 'Licenses')
)
$qtLicenseFiles = @(
    foreach ($directory in $licenseDirectories) {
        if (-not [string]::IsNullOrWhiteSpace($directory) -and
            (Test-Path -LiteralPath $directory -PathType Container)) {
            Get-ChildItem -LiteralPath $directory -File -Recurse |
                Where-Object { $_.Name -match '(?i)(license|copying|copyright|notice|lgpl|gpl|sqlite)' }
        }
    }
)
if ($qtLicenseFiles.Count -eq 0) {
    throw "Qt/SQLite license texts were not found under the Qt installation: $qtPath"
}

New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
$packageName = 'LightMediaBrowser-windows-x64'
$packagePath = Join-Path $outputPath "$packageName.zip"
$temporaryRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$stagePath = Join-Path $temporaryRoot "lightmediabrowser-package-$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $stagePath | Out-Null

try {
    Copy-Item -LiteralPath $executable -Destination $stagePath
    & $deployTool --release --compiler-runtime --no-translations --include-plugins qsqlite --dir $stagePath $executable
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit code $LASTEXITCODE"
    }

    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'LICENSE') -Destination $stagePath
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'THIRD_PARTY_NOTICES.md') -Destination $stagePath
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'docs\INSTALLATION.md') `
        -Destination (Join-Path $stagePath 'README.md')

    $thirdPartyDirectory = Join-Path $stagePath 'third-party-licenses\qt'
    New-Item -ItemType Directory -Path $thirdPartyDirectory -Force | Out-Null
    $licenseIndex = 0
    foreach ($licenseFile in $qtLicenseFiles) {
        $destinationName = '{0:D3}-{1}-{2}' -f $licenseIndex, $licenseFile.Directory.Name, $licenseFile.Name
        Copy-Item -LiteralPath $licenseFile.FullName -Destination (Join-Path $thirdPartyDirectory $destinationName) -Force
        $licenseIndex++
    }

    foreach ($requiredFile in @('Qt6Core.dll', 'Qt6Widgets.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stagePath $requiredFile) -PathType Leaf)) {
            throw "Qt runtime file was not deployed: $requiredFile"
        }
    }
    if (-not (Get-ChildItem -LiteralPath $stagePath -Filter 'qsqlite.dll' -File -Recurse)) {
        throw 'The QSQLITE plugin was not deployed.'
    }
    if (Get-ChildItem -LiteralPath $stagePath -File -Recurse |
        Where-Object { $_.Name -match '^(ffmpeg|ffprobe)(\.exe)?$' }) {
        throw 'FFmpeg must not be included in the Windows package.'
    }
    if (-not (Get-ChildItem -LiteralPath $thirdPartyDirectory -File)) {
        throw 'Qt license texts were not copied into the Windows package.'
    }

    if (Test-Path -LiteralPath $packagePath) {
        Remove-Item -LiteralPath $packagePath -Force
    }
    Compress-Archive -Path (Join-Path $stagePath '*') -DestinationPath $packagePath -CompressionLevel Optimal
    if (-not (Test-Path -LiteralPath $packagePath -PathType Leaf) -or
        (Get-Item -LiteralPath $packagePath).Length -eq 0) {
        throw "Windows package was not created: $packagePath"
    }
    Write-Output "Created $packagePath"
}
finally {
    Remove-Item -LiteralPath $stagePath -Recurse -Force
}
