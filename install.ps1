param(
    [Parameter(Mandatory=$true)][string]$GameExe,
    [string]$PackageRoot = $PSScriptRoot
)
$ErrorActionPreference = 'Stop'
$version = '1.0.1'
$GameExe = (Resolve-Path -LiteralPath $GameExe).Path
$gameRoot = Split-Path $GameExe -Parent
if ((Split-Path $GameExe -Leaf) -notin @('th18.exe','th18c.exe')) { throw 'Choose th18.exe or th18c.exe' }
if ((Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $gameRoot 'th18.exe')).Hash.ToLowerInvariant() -notin @('5aeb74b19939a29a8cf06e4fe7777aaf9139e93478507e039f43318171020e9f','9ed66e6952459e81515c17a671410bee7014a83e3c6cc6a7e360e7b4904c62f4')) { throw 'Unsupported game core' }
if ((Split-Path $GameExe -Leaf) -eq 'th18c.exe' -and (Get-FileHash -Algorithm SHA256 -LiteralPath $GameExe).Hash.ToLowerInvariant() -ne '8c1dc7ebf7aab0c8962b2a71cb848d59ef180e1f0613a3c410ca043767044c5b') { throw 'Unsupported Chinese entry' }
if (-not (Test-Path -LiteralPath (Join-Path $PackageRoot 'th18_coop.dll'))) {
    $PackageRoot = Join-Path $PSScriptRoot "dist/TH18LocalCoop-$version-experimental"
}
$PackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$gameRoot = Split-Path $GameExe -Parent
$destination = [IO.Path]::GetFullPath((Join-Path $gameRoot "mods/TH18LocalCoop/$version"))
if (-not $destination.StartsWith($gameRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid installation destination' }
$files = foreach ($line in Get-Content -LiteralPath (Join-Path $PackageRoot 'SHA256SUMS.txt')) {
    if ($line -notmatch '^([a-fA-F0-9]{64})  (.+)$') { throw 'Invalid manifest' }
    $expected = $Matches[1]; $relative = $Matches[2]
    $source = [IO.Path]::GetFullPath((Join-Path $PackageRoot $relative))
    if (-not $source.StartsWith($PackageRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid package path' }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash -ne $expected) { throw "Hash mismatch: $relative" }
    $relative
}
# Copy only verified package files. Keep older versions for rollback.
New-Item -ItemType Directory -Path $destination -Force | Out-Null
foreach ($relative in $files) {
    $target = Join-Path $destination $relative
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $PackageRoot $relative) -Destination $target -Force
}
Copy-Item -LiteralPath (Join-Path $PackageRoot 'SHA256SUMS.txt') -Destination (Join-Path $destination 'SHA256SUMS.txt') -Force
[IO.File]::WriteAllText((Join-Path $destination 'game-path.txt'),$GameExe,[Text.UTF8Encoding]::new($false))
# Native startup generates fresh saves. The launcher imports only custom.exe's
# th18.cfg before a new game process starts; never import score/replay files.
$profileDirectory=Join-Path $gameRoot 'coop-data/ShanghaiAlice/th18'
New-Item -ItemType Directory -Force -Path (Join-Path $profileDirectory 'replay'),(Join-Path $profileDirectory 'snapshot') | Out-Null
$shell = New-Object -ComObject WScript.Shell
$linkPath = Join-Path $gameRoot '东方虹龙洞 - 本地双人.lnk'
$link = $shell.CreateShortcut($linkPath)
$link.TargetPath = Join-Path $destination 'TH18LocalCoop.exe'
$link.WorkingDirectory = $destination
$link.IconLocation = $GameExe+',0'
$link.Description = "虹龙洞本地双人 / 在线两至三人 $version"
$link.Save()
foreach ($relative in $files) {
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $destination $relative)).Hash -ne
        (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $PackageRoot $relative)).Hash) { throw "Installed hash mismatch: $relative" }
}
Write-Output "Installed: $destination"
Write-Output "Shortcut: $linkPath"
