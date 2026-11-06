param([string]$ZigPath = '')
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$zig = $ZigPath
if (-not $zig) { $zig = Join-Path $taskRoot 'tools/python/ziglang/zig.exe' }
if (-not (Test-Path -LiteralPath $zig)) {
    $found = Get-Command zig.exe -ErrorAction SilentlyContinue
    if ($found) { $zig = $found.Source } else { throw 'Install Zig 0.13.0 and pass -ZigPath C:\path\to\zig.exe.' }
}
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $taskRoot '.zig-cache/global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $taskRoot '.zig-cache/local'
$release = Join-Path $taskRoot 'release'
New-Item -ItemType Directory -Force -Path $release | Out-Null
& (Join-Path $taskRoot 'tools/build_eagler_core.ps1') -ZigPath $zig -OutputDirectory (Join-Path $taskRoot 'tests/bin/eagler')
& $zig cc -target x86-windows-gnu -O2 -g -shared (Join-Path $taskRoot 'src/coop.c') (Join-Path $taskRoot 'src/hud.c') (Join-Path $taskRoot 'src/bridges.S') (Join-Path $taskRoot 'tests/bin/eagler/eagler_core.a') -o (Join-Path $release 'th18_coop.dll') -luser32 -lkernel32 -lc++
if ($LASTEXITCODE -ne 0) { throw 'DLL compilation failed' }
& $zig cc -target x86-windows-gnu -O1 -g -municode (Join-Path $taskRoot 'src/loader.c') -o (Join-Path $release 'TH18LocalCoop.Console.exe') -luser32 -lkernel32 -ladvapi32
if ($LASTEXITCODE -ne 0) { throw 'Loader compilation failed' }
$csc = Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
& $csc /nologo /target:winexe /platform:x86 /optimize+ /utf8output /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Core.dll ("/out:" + (Join-Path $release 'TH18LocalCoop.exe')) (Join-Path $taskRoot 'src/Launcher.cs') (Join-Path $taskRoot 'src/HelpForm.cs') (Join-Path $taskRoot 'src/KeyBindingsForm.cs') (Join-Path $taskRoot 'src/ReplayViewer.cs') (Join-Path $taskRoot 'src/CoopData.cs') (Join-Path $taskRoot 'src/NetworkForm.cs') (Join-Path $taskRoot 'src/NetSession.cs') (Join-Path $taskRoot 'src/InputTape.cs') (Join-Path $taskRoot 'src/NetworkLog.cs') (Join-Path $taskRoot 'src/OnlineBridge.cs') (Join-Path $taskRoot 'src/OnlineProfile.cs') (Join-Path $taskRoot 'src/OnlineGame.cs') (Join-Path $taskRoot 'src/OnlineSession.cs') (Join-Path $taskRoot 'src/RollbackTransport.cs') (Join-Path $taskRoot 'src/LocalDuo.cs')
if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed' }
& $csc /nologo /target:exe /platform:x86 /optimize+ /utf8output /r:System.Core.dll ("/out:" + (Join-Path $release 'TH18Coop.NetTest.exe')) (Join-Path $taskRoot 'tools/net_probe.cs') (Join-Path $taskRoot 'tools/online_probe.cs') (Join-Path $taskRoot 'src/NetSession.cs') (Join-Path $taskRoot 'src/InputTape.cs') (Join-Path $taskRoot 'src/NetworkLog.cs') (Join-Path $taskRoot 'src/OnlineBridge.cs') (Join-Path $taskRoot 'src/OnlineProfile.cs') (Join-Path $taskRoot 'src/OnlineGame.cs') (Join-Path $taskRoot 'src/OnlineSession.cs') (Join-Path $taskRoot 'src/RollbackTransport.cs') (Join-Path $taskRoot 'src/LocalDuo.cs') (Join-Path $taskRoot 'src/CoopData.cs')
if ($LASTEXITCODE -ne 0) { throw 'Network probe compilation failed' }
Copy-Item -LiteralPath (Join-Path $taskRoot 'coop.ini') -Destination (Join-Path $release 'coop.ini') -Force
Get-ChildItem -LiteralPath $release -Filter '*.exe' | Select-Object -ExpandProperty FullName
& (Join-Path $taskRoot 'tools/build_native_fixture.ps1') -ZigPath $zig -OutputPath (Join-Path $release 'TH18Coop.NativeTest.exe')
