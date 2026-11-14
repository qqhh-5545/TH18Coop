param([switch]$SkipShared,[string]$ZigPath='')
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
if(!$ZigPath){$ZigPath=Join-Path $taskRoot 'tools/python/ziglang/zig.exe'}
if(!$SkipShared){& (Join-Path $taskRoot 'test.ps1') -ZigPath $ZigPath}
$csc=Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
$sources=@('GameCatalog','Th14Launcher','Th14Thcrap','Th14OnlineGame','CoopData','NetSession','InputTape','NetworkLog','OnlineBridge','OnlineProfile','OnlineGame','OnlineSession','RollbackTransport','LocalDuo')|ForEach-Object{Join-Path $taskRoot "src/$_.cs"}
$testExe=Join-Path $taskRoot 'tests/bin/multigame_test.exe'
& $csc /nologo /target:exe /platform:x86 /optimize+ /utf8output /r:System.Core.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll ('/out:'+$testExe) (Join-Path $taskRoot 'tests/multigame_test.cs') @sources
if($LASTEXITCODE-ne 0){throw 'Multi-game test compilation failed'}
& $testExe
if($LASTEXITCODE-ne 0){throw 'Multi-game profile tests failed'}
