param([string]$InputFixture='')
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$csc=Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
$bin=Join-Path $root 'tests/bin'
New-Item -ItemType Directory -Force -Path $bin | Out-Null
& $csc /nologo /target:exe /platform:x86 /r:System.Windows.Forms.dll /r:System.Drawing.dll ('/out:'+(Join-Path $bin 'v104_integration.exe')) (Join-Path $root 'tests/th18_v104_integration_test.cs')
if($LASTEXITCODE-ne 0){throw 'UI integration test build failed'}
& (Join-Path $bin 'v104_integration.exe') (Join-Path $root 'release/TouhouCoop.exe')
if($LASTEXITCODE-ne 0){throw 'UI integration test failed'}
$sources=Get-ChildItem (Join-Path $root 'src/th18v104/*.cs') | ForEach-Object {$_.FullName}
& $csc /nologo /target:exe /platform:x86 /main:ReconnectProbe /r:System.Core.dll /r:System.Windows.Forms.dll /r:System.Drawing.dll ('/out:'+(Join-Path $bin 'v104-reconnect.exe')) (Join-Path $root 'tests/th18v104/v104-reconnect_probe.cs') @sources
if($LASTEXITCODE-ne 0){throw 'Reconnect test build failed'}
& (Join-Path $bin 'v104-reconnect.exe')
if($LASTEXITCODE-ne 0){throw 'Reconnect test failed'}
& $csc /nologo /target:exe /platform:x86 /main:NetProbe /r:System.Core.dll /r:System.Windows.Forms.dll /r:System.Drawing.dll ('/out:'+(Join-Path $bin 'v104-net.exe')) (Join-Path $root 'tests/th18v104/v104-net_probe.cs') (Join-Path $root 'tests/th18v104/v104-online_probe.cs') @sources
if($LASTEXITCODE-ne 0){throw 'Network test build failed'}
if($InputFixture){& (Join-Path $bin 'v104-net.exe') --self-test $InputFixture}else{& (Join-Path $bin 'v104-net.exe') --self-test}
if($LASTEXITCODE-ne 0){throw 'Network test failed'}
