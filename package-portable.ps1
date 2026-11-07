param([string]$Name='TH18Coop-1.0.4-recovery-test',[string]$ValidationFile='docs/RECOVERY_VALIDATION.json')
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
$name=$Name
if($name -notmatch '^TH18Coop-[A-Za-z0-9.-]+$'){throw 'Invalid portable package name'}
$destination=Join-Path $taskRoot ('dist/'+$name)
New-Item -ItemType Directory -Path $destination -Force | Out-Null

# Reuse the qualified native build. This export must not silently
# substitute an unqualified native DLL or include per-machine data.
$validation=Get-Content -LiteralPath (Join-Path $taskRoot $ValidationFile) -Raw | ConvertFrom-Json
if((Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskRoot 'release/th18_coop.dll')).Hash.ToLowerInvariant() -ne $validation.dll_sha256) {
    throw 'Native DLL differs from the qualified key-binding build.'
}
$files=[ordered]@{
    '虹龙洞联机.exe'='release/TH18LocalCoop.exe'
    'TH18LocalCoop.Console.exe'='release/TH18LocalCoop.Console.exe'
    'th18_coop.dll'='release/th18_coop.dll'
    'coop.ini'='coop.ini'
    '使用.txt'='docs/PORTABLE.txt'
    'LICENSE'='LICENSE'
    'THIRD_PARTY_NOTICES.md'='THIRD_PARTY_NOTICES.md'
}
foreach($license in @('eagler-common-LICENSE.txt','th07-CC0.txt','thprac-MIT.txt','zig-MIT.txt','mingw-COPYING.txt','libcxx-LICENSE.txt','libcxxabi-LICENSE.txt','libunwind-LICENSE.txt')) {
    $files.Add('licenses/'+$license,'licenses/'+$license)
}
foreach($relative in $files.Keys) {
    $target=Join-Path $destination $relative
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $taskRoot $files[$relative]) -Destination $target -Force
}
foreach($file in Get-ChildItem -LiteralPath $destination -Recurse -File -Force) {
    $relative=$file.FullName.Substring($destination.Length+1).Replace('\','/')
    if($relative -ne 'SHA256SUMS.txt' -and $relative -notin $files.Keys) {throw "Unexpected portable file: $relative"}
}
$manifest=foreach($relative in $files.Keys | Sort-Object) {
    '{0}  {1}' -f (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $destination $relative)).Hash.ToLowerInvariant(),$relative
}
[IO.File]::WriteAllLines((Join-Path $destination 'SHA256SUMS.txt'),$manifest,[Text.UTF8Encoding]::new($false))
Compress-Archive -LiteralPath $destination -DestinationPath ($destination+'.zip') -Force
Get-Item -LiteralPath ($destination+'.zip') | Select-Object FullName,Length
