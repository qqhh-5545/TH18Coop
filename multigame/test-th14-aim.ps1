param([string]$ZigPath='')
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
if(!$ZigPath){$ZigPath=Join-Path $taskRoot 'tools/python/ziglang/zig.exe'}
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $taskRoot '.zig-cache/global'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $taskRoot '.zig-cache/local'
$testOutput=Join-Path $taskRoot 'tests/bin'
New-Item -ItemType Directory -Force -Path $testOutput | Out-Null
$testExe=Join-Path $testOutput 'th14_aim_test.exe'
& $ZigPath cc -target x86-windows-gnu -O2 '-Wl,--image-base,0x30000000' (Join-Path $taskRoot 'tests/th14_aim_test.c') -o $testExe -lkernel32
if($LASTEXITCODE -ne 0){throw 'TH14 aim test compilation failed'}
& $testExe
if($LASTEXITCODE -ne 0){throw 'TH14 aim test failed'}
