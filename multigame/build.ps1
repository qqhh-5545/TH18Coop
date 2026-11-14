param([string]$ZigPath='', [string]$RuntimeDirectory='', [string]$IconDirectory='')
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
if(!$ZigPath){$ZigPath=Join-Path $taskRoot 'tools/python/ziglang/zig.exe'}
if(!(Test-Path -LiteralPath $ZigPath)){ $ZigPath=(Get-Command zig.exe -ErrorAction Stop).Source }
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $taskRoot '.zig-cache/global'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $taskRoot '.zig-cache/local'
$release=Join-Path $taskRoot 'release'
New-Item -ItemType Directory -Force -Path $release | Out-Null
$pins=Get-Content -LiteralPath (Join-Path $taskRoot 'docs/TH18_V104_IMPORT.json') -Raw | ConvertFrom-Json
foreach($file in @('th18_coop.dll','TH18LocalCoop.Console.exe')) {
    $candidate=if($RuntimeDirectory){Join-Path $RuntimeDirectory $file}else{Join-Path $release $file}
    if(!(Test-Path -LiteralPath $candidate) -or (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.runtime.$file) {
        throw "Extract the official 1.0.4 portable release and pass -RuntimeDirectory with its directory. Pinned runtime mismatch: $file"
    }
}
if($RuntimeDirectory) {
    foreach($file in @('th18_coop.dll','TH18LocalCoop.Console.exe')) {
        $from=[IO.Path]::GetFullPath((Join-Path $RuntimeDirectory $file))
        $to=[IO.Path]::GetFullPath((Join-Path $release $file))
        if($from -ne $to){Copy-Item -LiteralPath $from -Destination $to -Force}
    }
}
$stable=Join-Path $release 'th18_coop.dll'
if((Get-FileHash -LiteralPath $stable -Algorithm SHA256).Hash.ToLowerInvariant() -ne '4b910582a7b7baa9fd299d056289a2954fb50cf6a4fc402ffc40b0f36e12e680'){throw 'Stable TH18 runtime differs from the pinned official 1.0.4 binary'}
& (Join-Path $taskRoot 'tools/build_eagler_core.ps1') -ZigPath $ZigPath
& $ZigPath cc -target x86-windows-gnu -O2 -g -shared (Join-Path $taskRoot 'src/th14/coop14.c') (Join-Path $taskRoot 'src/th14/bridges14.S') (Join-Path $taskRoot 'tests/bin/eagler/eagler_core.a') -o (Join-Path $release 'th14_coop.dll') -luser32 -lkernel32 -lc++
if($LASTEXITCODE -ne 0){throw 'TH14 native adapter compilation failed'}
& $ZigPath cc -target x86-windows-gnu -O1 -g -municode (Join-Path $taskRoot 'src/th14/loader14.c') -o (Join-Path $release 'TH14Coop.Console.exe') -luser32 -lkernel32 -ladvapi32
if($LASTEXITCODE -ne 0){throw 'TH14 native loader compilation failed'}
$csc=Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
$names=@('Launcher','GameCatalog','Th14Launcher','Th14Thcrap','Th14OnlineGame','HelpForm','KeyBindingsForm','ReplayViewer','CoopData','NetworkForm','NetSession','InputTape','NetworkLog','OnlineBridge','OnlineProfile','OnlineGame','OnlineSession','RollbackTransport','LocalDuo')
$sources=$names|ForEach-Object{Join-Path $taskRoot "src/$_.cs"}
$sources+=Get-ChildItem -LiteralPath (Join-Path $taskRoot "src/th18v104") -Filter "*.cs" | ForEach-Object {$_.FullName}
$iconResources=@()
if($IconDirectory) {
    foreach($game in @('th14','th18')) {
        $icon=Join-Path $IconDirectory ($game+'.ico')
        if(!(Test-Path -LiteralPath $icon)){throw "Missing local icon: $game.ico"}
        $iconResources+=('/resource:'+$icon+',GameIcons.'+$game+'.ico')
    }
}
& $csc /main:TH18LocalCoop.Launcher /nologo /target:winexe /platform:x86 /optimize+ /utf8output /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Core.dll ('/out:'+(Join-Path $release 'TouhouCoop.exe')) @sources @iconResources
if($LASTEXITCODE -ne 0){throw 'Multi-game launcher compilation failed'}
Copy-Item -LiteralPath (Join-Path $taskRoot 'coop.ini') -Destination (Join-Path $release 'coop.ini') -Force
Write-Output 'Built Touhou Co-op 1.0.5 and TH14 adapter with the verified TH18 1.0.4 runtime.'
