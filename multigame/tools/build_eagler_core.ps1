param([string]$ZigPath='', [string]$OutputDirectory='')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
if(-not $ZigPath){$ZigPath=Join-Path $taskRoot 'tools/python/ziglang/zig.exe'}
if(-not $OutputDirectory){$OutputDirectory=Join-Path $taskRoot 'tests/bin/eagler'}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$include=Join-Path $taskRoot 'src/vendor/eagler/include'
$sources=@('src/eagler_core.cpp','src/vendor/eagler/src/netplay/NetplayCore.cpp','src/vendor/eagler/src/netplay/NetplayProtocol.cpp')
$objects=@()
foreach($relative in $sources){
    $object=Join-Path $OutputDirectory (([IO.Path]::GetFileNameWithoutExtension($relative))+'.o');$objects+=$object
    & $ZigPath c++ -target x86-windows-gnu -std=c++17 -O2 -g -I $include -I (Join-Path $taskRoot 'src') -c (Join-Path $taskRoot $relative) -o $object
    if($LASTEXITCODE -ne 0){throw "Eagler core compilation failed: $relative"}
}
& $ZigPath ar rcs (Join-Path $OutputDirectory 'eagler_core.a') @objects
if($LASTEXITCODE -ne 0){throw 'Eagler core archive failed'}
