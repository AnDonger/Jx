param(
    [Parameter(Mandatory=$true)][string]$PerlPath,
    [string]$VisualStudioRoot
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$workRoot = Join-Path $projectRoot 'Test\Dependencies'
$outputRoot = Join-Path $workRoot 'OpenSSL'
$version = '3.5.9'
$expectedHash = '796831EC533BF41998EAAEE9D632D921B8A69F57394FA4086FF01A941113D26D'
if (-not (Test-Path -LiteralPath $PerlPath -PathType Leaf)) {
    throw 'Specify an installed native Windows Perl executable. MSYS Perl is not suitable for VC-WIN32.'
}
if (-not $VisualStudioRoot) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VisualStudioRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
$environmentScript = Join-Path $VisualStudioRoot 'VC\Auxiliary\Build\vcvars32.bat'
if (-not (Test-Path -LiteralPath $environmentScript)) { throw 'Visual C++ x86 build tools are missing.' }
New-Item -ItemType Directory -Path $workRoot -Force | Out-Null
$archive = Join-Path $workRoot "openssl-$version.tar.gz"
if (-not (Test-Path -LiteralPath $archive)) {
    & curl.exe --ssl-revoke-best-effort --fail --location --silent --show-error `
        "https://codeload.github.com/openssl/openssl/tar.gz/refs/tags/openssl-$version" --output $archive
    if ($LASTEXITCODE -ne 0) { throw 'OpenSSL source download failed.' }
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'OpenSSL source hash mismatch. The archive was not executed.'
}
$sourceRoot = Join-Path $workRoot "openssl-openssl-$version"
if (-not (Test-Path -LiteralPath $sourceRoot)) {
    & tar.exe -xf $archive -C $workRoot
    if ($LASTEXITCODE -ne 0) { throw 'OpenSSL extraction failed.' }
}
Push-Location $sourceRoot
try {
    & $PerlPath Configure VC-WIN32 no-shared no-asm no-tests no-docs no-apps no-module no-legacy "--prefix=$outputRoot"
    if ($LASTEXITCODE -ne 0) { throw 'OpenSSL configuration failed.' }
    $perlDirectory = Split-Path -Parent $PerlPath
    $commandFile = Join-Path $workRoot 'BuildOpenSsl.cmd'
    $commands = @"
@echo off
call "$environmentScript"
if errorlevel 1 exit /b %errorlevel%
set "PATH=$perlDirectory;%PATH%"
cd /d "$sourceRoot"
nmake /nologo build_libs > "$workRoot\OpenSSL-build.log" 2>&1
exit /b %errorlevel%
"@
    [IO.File]::WriteAllText($commandFile, $commands, [Text.Encoding]::Default)
    & $commandFile
    if ($LASTEXITCODE -ne 0) { throw "OpenSSL build failed. See $workRoot\OpenSSL-build.log" }
    New-Item -ItemType Directory -Path "$outputRoot\include\openssl","$outputRoot\lib" -Force | Out-Null
    Copy-Item -Path "$sourceRoot\include\openssl\*.h" -Destination "$outputRoot\include\openssl" -Force
    foreach ($library in 'libssl.lib','libcrypto.lib') {
        Copy-Item -LiteralPath "$sourceRoot\$library" -Destination "$outputRoot\lib\$library" -Force
    }
    Copy-Item -LiteralPath "$sourceRoot\LICENSE.txt" -Destination "$outputRoot\LICENSE.txt" -Force
    Write-Output "OpenSSL $version VC-WIN32 /MT is ready: $outputRoot"
} finally {
    Pop-Location
}
