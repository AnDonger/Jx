param(
    [string]$OpenSslRoot = "$PSScriptRoot\..\..\..\Test\Dependencies\OpenSSL",
    [string]$VcVars = 'D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat'
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath("$PSScriptRoot\..\..\..")
$outputRoot = Join-Path $projectRoot 'Test\Client20261030\Tools'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
foreach ($name in 'CreateLocalIdentity', 'PatchClientTrust') {
    $commands = @(
        '@echo off'
        "call `"$VcVars`" >nul"
        "cl /nologo /EHsc /MT /W4 /I `"$OpenSslRoot\include`" `"$PSScriptRoot\$name.cpp`" /Fo`"$outputRoot\$name.obj`" /link /out:`"$outputRoot\$name.exe`" /MANIFEST:EMBED /MANIFESTUAC:`"level='asInvoker' uiAccess='false'`" /libpath:`"$OpenSslRoot\lib`" libssl.lib libcrypto.lib ws2_32.lib crypt32.lib advapi32.lib user32.lib"
    )
    $commandPath = Join-Path $outputRoot "Build-$name.cmd"
    [IO.File]::WriteAllLines($commandPath, $commands, [Text.Encoding]::Default)
    & $commandPath
    if ($LASTEXITCODE -ne 0) {
        throw "$name compilation failed."
    }
}
