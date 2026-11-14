param([string]$ZigPath='')
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
if(!$ZigPath){$ZigPath=Join-Path $taskRoot 'tools/python/ziglang/zig.exe'}
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $taskRoot '.zig-cache/global'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $taskRoot '.zig-cache/local'
$fixture=[IO.File]::ReadAllText((Join-Path $taskRoot 'src/th14/hud14.h')).Replace('((void (TC *)(void *))0x475eb0)(manager);','mock_flush(manager);')
[IO.File]::WriteAllText((Join-Path $taskRoot 'tests/bin/th14_hud_fixture.h'),$fixture)
$testExe=Join-Path $taskRoot 'tests/bin/th14_hud_test.exe'
& $ZigPath cc -target x86-windows-gnu -O2 (Join-Path $taskRoot 'tests/th14_hud_test.c') -I (Join-Path $taskRoot 'tests/bin') -o $testExe -luser32 -lkernel32
if($LASTEXITCODE-ne 0){throw 'TH14 HUD fixture build failed'}
& $testExe
if($LASTEXITCODE-ne 0){throw 'TH14 HUD geometry/state regression failed'}
