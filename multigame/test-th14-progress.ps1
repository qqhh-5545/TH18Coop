param([string]$ZigPath='')
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
if(!$ZigPath){$ZigPath=Join-Path $taskRoot 'tools/python/ziglang/zig.exe'}
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $taskRoot '.zig-cache/global'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $taskRoot '.zig-cache/local'
$testExe=Join-Path $taskRoot 'tests/bin/th14_progress_test.exe'
& $ZigPath cc -target x86-windows-gnu -O2 (Join-Path $taskRoot 'tests/th14_progress_test.c') -o $testExe
if($LASTEXITCODE -ne 0){throw 'TH14 progress test compilation failed'}
& $testExe
if($LASTEXITCODE -ne 0){throw 'TH14 progress test failed'}
$scaleAssembly=Join-Path $taskRoot 'tests/bin/th14_scale_bridge_fixture.S'
$production=[IO.File]::ReadAllText((Join-Path $taskRoot 'src/th14/bridges14.S'))
# Compile the exact production bridge section, excluding unrelated adapters.
$scaleStart=$production.IndexOf('.globl _scale14_bridge')
[IO.File]::WriteAllText($scaleAssembly,".text`n"+$production.Substring($scaleStart,$production.IndexOf('.macro DAMAGE_WRAP')-$scaleStart))
$scaleExe=Join-Path $taskRoot 'tests/bin/th14_scale_bridge_test.exe'
& $ZigPath cc -target x86-windows-gnu -O2 (Join-Path $taskRoot 'tests/th14_scale_bridge_test.c') $scaleAssembly -o $scaleExe
if($LASTEXITCODE -ne 0){throw 'TH14 scale bridge test compilation failed'}
& $scaleExe
if($LASTEXITCODE -ne 0){throw 'TH14 scale bridge test failed'}
$menuAssembly=Join-Path $taskRoot 'tests/bin/th14_menu_bridge_fixture.S'
$menu=$production.Substring(0,$scaleStart).Replace('pushl $0x445840','pushl $_test_native_fade')
[IO.File]::WriteAllText($menuAssembly,$menu)
$menuExe=Join-Path $taskRoot 'tests/bin/th14_menu_bridge_test.exe'
& $ZigPath cc -target x86-windows-gnu -O2 (Join-Path $taskRoot 'tests/th14_menu_bridge_test.c') $menuAssembly -o $menuExe
if($LASTEXITCODE-ne 0){throw 'TH14 menu bridge compilation failed'}
& $menuExe
if($LASTEXITCODE-ne 0){throw 'TH14 menu BGM bridge test failed'}
