param([string]$ZigPath = '')
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
if (-not $ZigPath) { $ZigPath = Join-Path $taskRoot 'tools/python/ziglang/zig.exe' }
if (-not (Test-Path -LiteralPath $ZigPath)) { $ZigPath = (Get-Command zig.exe -ErrorAction Stop).Source }
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $taskRoot '.zig-cache/global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $taskRoot '.zig-cache/local'
$rollbackCsc=Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
New-Item -ItemType Directory -Force -Path (Join-Path $taskRoot 'tests/bin') | Out-Null
& $rollbackCsc /nologo /target:exe /platform:x86 /optimize+ /r:System.Core.dll ("/out:"+(Join-Path $taskRoot 'tests/bin/rollback_transport_test.exe')) (Join-Path $taskRoot 'tests/rollback_transport_test.cs') (Join-Path $taskRoot 'src/RollbackTransport.cs')
if($LASTEXITCODE -ne 0){throw 'Rollback TCP/IPC transport test build failed'}
& (Join-Path $taskRoot 'tests/bin/rollback_transport_test.exe')
if($LASTEXITCODE -ne 0){throw 'Rollback TCP/IPC transport test failed'}
& (Join-Path $taskRoot 'tools/build_eagler_core.ps1') -ZigPath $ZigPath
& $ZigPath cc -target x86-windows-gnu -O1 (Join-Path $taskRoot 'tests/eagler_core_test.c') (Join-Path $taskRoot 'tests/bin/eagler/eagler_core.a') -lc++ -o (Join-Path $taskRoot 'tests/bin/eagler_core_test.exe')
if($LASTEXITCODE -ne 0){throw 'Upstream rollback core test build failed'}
& (Join-Path $taskRoot 'tests/bin/eagler_core_test.exe')
if($LASTEXITCODE -ne 0){throw 'Upstream rollback core test failed'}
& $rollbackCsc /nologo /target:exe /platform:x86 /optimize+ ("/out:"+(Join-Path $taskRoot 'tests/bin/rollback_test.exe')) (Join-Path $taskRoot 'src/RollbackTimeline.cs') (Join-Path $taskRoot 'tests/rollback_test.cs')
if($LASTEXITCODE -ne 0){throw 'Rollback ledger build failed'}
& (Join-Path $taskRoot 'tests/bin/rollback_test.exe')
if($LASTEXITCODE -ne 0){throw 'Rollback ledger failed'}
& $ZigPath cc -target x86-windows-gnu -O2 (Join-Path $taskRoot 'tests/rollback_test.c') -o (Join-Path $taskRoot 'tests/bin/rollback_journal_test.exe')
if($LASTEXITCODE -ne 0){throw 'Rollback journal build failed'}
& (Join-Path $taskRoot 'tests/bin/rollback_journal_test.exe')
if($LASTEXITCODE -ne 0){throw 'Rollback journal failed'}
$testOutput = Join-Path $taskRoot 'tests/bin'
New-Item -ItemType Directory -Force -Path $testOutput | Out-Null
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
& $testExe (Join-Path $testOutput 'input-fixture.th18input')
if ($LASTEXITCODE -ne 0) { throw "Context tests failed ($LASTEXITCODE)" }
$fixtureHud = [regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'src/hud.c')),'0x[0-9a-fA-F]+',$relocateAddress)
$fixtureHudTest = [IO.File]::ReadAllText((Join-Path $taskRoot 'tests/hud_test.c')).Replace('../src/hud.c','hud_fixture.c')
$fixtureHudTest = [regex]::Replace($fixtureHudTest,'0x[0-9a-fA-F]+',$relocateAddress)
[IO.File]::WriteAllText((Join-Path $testOutput 'hud_fixture.c'),$fixtureHud)
[IO.File]::WriteAllText((Join-Path $testOutput 'hud_test_fixture.c'),$fixtureHudTest)
$hudExe = Join-Path $testOutput 'hud_test.exe'
& $ZigPath cc -target x86-windows-gnu -O2 '-Wl,--image-base,0x30000000' '-I' (Join-Path $taskRoot 'src') (Join-Path $testOutput 'hud_test_fixture.c') -o $hudExe -luser32 -lkernel32
if ($LASTEXITCODE -ne 0) { throw 'HUD test build failed' }
$hudBytes = [IO.File]::ReadAllBytes($hudExe)
$hudPe = [BitConverter]::ToInt32($hudBytes,0x3c)
$hudBytes[$hudPe+94] = $hudBytes[$hudPe+94] -band 0xbf
[IO.File]::WriteAllBytes($hudExe,$hudBytes)
& $hudExe (Join-Path $testOutput 'hud-trace.txt')
if ($LASTEXITCODE -ne 0) { throw "HUD tests failed ($LASTEXITCODE)" }

$csc = Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
$dataExe = Join-Path $testOutput 'data_test.exe'
& $csc /nologo /target:exe /platform:x86 /optimize+ /utf8output ("/out:"+$dataExe) (Join-Path $taskRoot 'tests/data_test.cs') (Join-Path $taskRoot 'src/CoopData.cs')
if ($LASTEXITCODE -ne 0) { throw 'Data isolation test build failed' }
& $dataExe
if ($LASTEXITCODE -ne 0) { throw "Data isolation tests failed ($LASTEXITCODE)" }
$netExe = Join-Path $testOutput 'net_test.exe'
& $csc /nologo /target:exe /platform:x86 /optimize+ /utf8output /r:System.Core.dll /r:System.Drawing.dll ("/out:"+$netExe) (Join-Path $taskRoot 'tools/net_probe.cs') (Join-Path $taskRoot 'tools/online_probe.cs') (Join-Path $taskRoot 'src/NetSession.cs') (Join-Path $taskRoot 'src/InputTape.cs') (Join-Path $taskRoot 'src/NetworkLog.cs') (Join-Path $taskRoot 'src/OnlineBridge.cs') (Join-Path $taskRoot 'src/OnlineProfile.cs') (Join-Path $taskRoot 'src/OnlineGame.cs') (Join-Path $taskRoot 'src/OnlineSession.cs') (Join-Path $taskRoot 'src/GameCatalog.cs') (Join-Path $taskRoot 'src/Th14Launcher.cs') (Join-Path $taskRoot 'src/Th14OnlineGame.cs') (Join-Path $taskRoot 'src/RollbackTransport.cs') (Join-Path $taskRoot 'src/LocalDuo.cs') (Join-Path $taskRoot 'src/CoopData.cs')
if ($LASTEXITCODE -ne 0) { throw 'Network test build failed' }
& $netExe --self-test (Join-Path $testOutput 'input-fixture.th18input')
if ($LASTEXITCODE -ne 0) { throw "Network tests failed ($LASTEXITCODE)" }
& $netExe --online-self-test $testExe (Join-Path $testOutput 'online-root')
if ($LASTEXITCODE -ne 0) { throw 'Online end-to-end checks failed' }
