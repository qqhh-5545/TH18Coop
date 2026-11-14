param([string]$ZigPath = '',[string]$OutputPath = '')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
if (-not $ZigPath) { $ZigPath = Join-Path $taskRoot 'tools/python/ziglang/zig.exe' }
if (-not (Test-Path -LiteralPath $ZigPath)) { $ZigPath = (Get-Command zig.exe -ErrorAction Stop).Source }
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $taskRoot '.zig-cache/global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $taskRoot '.zig-cache/local'
$testOutput = Join-Path $taskRoot 'tests/bin'
New-Item -ItemType Directory -Force -Path $testOutput | Out-Null
if(-not (Test-Path -LiteralPath (Join-Path $taskRoot 'tests/bin/eagler/eagler_core.a'))){& (Join-Path $taskRoot 'tools/build_eagler_core.ps1') -ZigPath $ZigPath}
$testExe = Join-Path $testOutput 'context_test.exe'
# Windows may reserve the original game's low address range for process heaps.
# Relocate engine address literals in this private fixture, leaving all hook
# logic intact. Production sources and runtime binaries are never rewritten.
$relocateAddress = [Text.RegularExpressions.MatchEvaluator] {
    param($match)
    $value = [Convert]::ToUInt64($match.Value.Substring(2),16)
    if ($value -ge 0x400000 -and $value -lt 0x580000) { return ('0x{0:x}' -f ($value+0x10000000)) }
    return $match.Value
}
$fixtureOnline = [regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'src/online_runtime.h')),'0x[0-9a-fA-F]+',$relocateAddress)
[IO.File]::WriteAllText((Join-Path $testOutput 'online_runtime_fixture.h'),$fixtureOnline)
$fixtureCoop = [regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'src/coop.c')),'0x[0-9a-fA-F]+',$relocateAddress)
$fixtureCoop = $fixtureCoop.Replace('online_runtime.h','online_runtime_fixture.h')
$fixtureReplay = [regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'src/native_replay.h')),'0x[0-9a-fA-F]+',$relocateAddress)
[IO.File]::WriteAllText((Join-Path $testOutput 'native_replay_fixture.h'),$fixtureReplay)
$fixtureCoop = $fixtureCoop.Replace('native_replay.h','native_replay_fixture.h')
$fixtureKeys = [regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'src/key_bindings.h')),'0x[0-9a-fA-F]+',$relocateAddress)
[IO.File]::WriteAllText((Join-Path $testOutput 'key_bindings_fixture.h'),$fixtureKeys)
$fixtureCoop = $fixtureCoop.Replace('key_bindings.h','key_bindings_fixture.h')
$fixtureTest = [IO.File]::ReadAllText((Join-Path $taskRoot 'tests/context_test.c')).Replace('../src/coop.c','coop_fixture.c')
$fixtureTest = [regex]::Replace($fixtureTest,'0x[0-9a-fA-F]+',$relocateAddress)
$fixtureBridges = [regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'src/bridges.S')),'0x[0-9a-fA-F]+',$relocateAddress)
[IO.File]::WriteAllText((Join-Path $testOutput 'coop_fixture.c'),$fixtureCoop)
[IO.File]::WriteAllText((Join-Path $testOutput 'context_fixture.c'),$fixtureTest)
$fixtureTrio = [regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'tests/trio_test.h')),'0x[0-9a-fA-F]+',$relocateAddress)
[IO.File]::WriteAllText((Join-Path $testOutput 'trio_test.h'),$fixtureTrio)
$fixtureHeap = [regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'tests/native_heap_test.h')),'0x[0-9a-fA-F]+',$relocateAddress)
[IO.File]::WriteAllText((Join-Path $testOutput 'native_heap_test.h'),$fixtureHeap)
$fixtureReplayTest = [regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'tests/native_replay_test.h')),'0x[0-9a-fA-F]+',$relocateAddress)
[IO.File]::WriteAllText((Join-Path $testOutput 'native_replay_test.h'),$fixtureReplayTest)
[IO.File]::WriteAllText((Join-Path $testOutput 'bridges_fixture.S'),$fixtureBridges)
& $ZigPath cc -target x86-windows-gnu -O2 -g '-Wl,--image-base,0x30000000' '-I' (Join-Path $taskRoot 'src') (Join-Path $testOutput 'context_fixture.c') (Join-Path $testOutput 'bridges_fixture.S') (Join-Path $taskRoot 'tests/bin/eagler/eagler_core.a') -lc++ -o $testExe -luser32 -lkernel32
if ($LASTEXITCODE -ne 0) { throw 'Context test build failed' }
# This private test maps fixed mock engine addresses. Keep only the generated
# test EXE at its requested base; runtime release binaries retain normal ASLR.
$testBytes = [IO.File]::ReadAllBytes($testExe)
$peOffset = [BitConverter]::ToInt32($testBytes,0x3c)
if ([BitConverter]::ToUInt32($testBytes,$peOffset) -ne 0x4550 -or
    [BitConverter]::ToUInt32($testBytes,$peOffset+52) -ne 0x30000000) { throw 'Unexpected test PE layout' }
$testBytes[$peOffset+94] = $testBytes[$peOffset+94] -band 0xbf
[IO.File]::WriteAllBytes($testExe,$testBytes)
if ($OutputPath) { Copy-Item -LiteralPath $testExe -Destination $OutputPath -Force }
