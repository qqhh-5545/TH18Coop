$ErrorActionPreference = 'Stop'

$taskRoot = $PSScriptRoot

$version = '0.20.0-experimental'

$dist = Join-Path $taskRoot 'dist'

$runtime = Join-Path $dist "TH18LocalCoop-$version"

$source = Join-Path $dist "TH18LocalCoop-$version-source"

New-Item -ItemType Directory -Force -Path $runtime,$source | Out-Null

function Copy-ListedFiles($destination, $paths) {

    foreach ($relative in $paths) {

        $target = Join-Path $destination $relative

        New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null

        Copy-Item -LiteralPath (Join-Path $taskRoot $relative) -Destination $target -Force

    }

}

# Explicit file lists prevent accidentally distributing game data or private dumps.

$common = @('README.md','LICENSE','THIRD_PARTY_NOTICES.md','coop.ini','install.ps1','docs/KEYBINDINGS_VALIDATION.json','docs/REPLAY.md','docs/REPLAY_VALIDATION.json','docs/ROLLBACK.md','docs/DELAY_VALIDATION.json','docs/INPUT_DELAY.md','docs/IMPLEMENTATION.md','docs/VALIDATION.md','docs/CHANGES.md','docs/NETWORK_PLAN.md','docs/CONNECTION_TEST.md','docs/INPUT_PROTOCOL.md','docs/ONLINE_PLAY.md','docs/COOPERATIVE_RULES.md',

    'licenses/eagler-common-LICENSE.txt','licenses/th07-CC0.txt','licenses/thprac-MIT.txt','licenses/zig-MIT.txt','licenses/mingw-COPYING.txt','licenses/libcxx-LICENSE.txt','licenses/libcxxabi-LICENSE.txt','licenses/libunwind-LICENSE.txt')

Copy-ListedFiles $runtime $common

$sourceFiles = $common + @('AGENTS.md','build.ps1','package.ps1','package-portable.ps1','docs/PORTABLE.txt','test.ps1','.gitignore','src/coop.c','src/bridges.S',

    'src/key_bindings.h','src/KeyBindingsForm.cs','src/native_replay.h','src/ReplayViewer.cs','src/HelpForm.cs','tests/native_replay_test.h','src/native_supply_scenario.h','src/native_delay_runtime.h','src/native_graphics_replay.h','src/native_present_history.h','src/native_rollback_runtime.h','src/native_rollback_oracle.h','src/native_rollback_scenarios.h','src/native_file_fence.h','src/native_asset_fence.h','src/native_world_digest.h','src/rollback_ipc.h','src/eagler_core.h','src/eagler_core.cpp','src/RollbackTransport.cs','src/netplay/NetplayProtocolConfig.hpp','tests/eagler_core_test.c','tools/build_eagler_core.ps1',

    'src/vendor/eagler/LICENSE','src/vendor/eagler/UPSTREAM.txt','src/vendor/eagler/include/eagler/netplay/NetplayCore.hpp','src/vendor/eagler/include/eagler/netplay/NetplayProtocol.hpp','src/vendor/eagler/src/netplay/NetplayCore.cpp','src/vendor/eagler/src/netplay/NetplayProtocol.cpp',

    'src/LocalDuo.cs','src/online_ipc.h','src/online_runtime.h','src/OnlineBridge.cs','src/OnlineProfile.cs','src/OnlineGame.cs','src/OnlineSession.cs','tools/online_probe.cs','tools/build_native_fixture.ps1',

    'src/native_heap_audit.h','src/native_write_watch.h','src/native_audio_audit.h','src/native_step_history.h','src/rollback_journal.h','src/RollbackTimeline.cs','tests/rollback_test.c','tests/rollback_test.cs','src/frame_input.h','src/InputTape.cs','src/NetworkLog.cs','src/NetSession.cs','src/NetworkForm.cs','tools/net_probe.cs','src/CoopData.cs','src/data_profile.h','src/hud.c','src/coop_api.h','src/loader.c','src/Launcher.cs','src/card_catalog.tsv','tools/play.py','tools/live.py','tools/inspect_pe.py',

    'tools/check_profile.py','tests/data_test.cs','tests/context_test.c','tests/trio_test.h','tests/native_heap_test.h','tests/hud_test.c','tests/draw_bench.c','tests/rollback_transport_test.cs')

Copy-ListedFiles $source $sourceFiles

$runtimeFiles = $common + @('TH18LocalCoop.exe','TH18LocalCoop.Console.exe','TH18Coop.NetTest.exe','TH18Coop.NativeTest.exe','th18_coop.dll')

foreach ($binary in @('TH18LocalCoop.exe','TH18LocalCoop.Console.exe','TH18Coop.NetTest.exe','TH18Coop.NativeTest.exe','th18_coop.dll')) {

    Copy-Item -LiteralPath (Join-Path $taskRoot "release/$binary") -Destination (Join-Path $runtime $binary) -Force

}

foreach ($folder in @($runtime,$source)) {

    $allowed = if ($folder -eq $runtime) { $runtimeFiles } else { $sourceFiles }

    foreach ($file in Get-ChildItem -LiteralPath $folder -Recurse -File -Force) {

        $relative = $file.FullName.Substring($folder.Length+1).Replace('\','/')

        if ($relative -ne 'SHA256SUMS.txt' -and $relative -notin $allowed) {

            throw "Unexpected staged file; refusing to package: $relative"

        }

    }

    $manifest = Get-ChildItem -LiteralPath $folder -Recurse -File | Where-Object Name -ne 'SHA256SUMS.txt' |

        Sort-Object FullName | ForEach-Object {

            $relative = $_.FullName.Substring($folder.Length+1).Replace('\','/')

            "{0}  {1}" -f (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant(),$relative

        }

    $manifest | Set-Content -Encoding ASCII -LiteralPath (Join-Path $folder 'SHA256SUMS.txt')

    Compress-Archive -LiteralPath $folder -DestinationPath ($folder+'.zip') -Force

    Get-Item -LiteralPath ($folder+'.zip') | Select-Object FullName,Length

}

& (Join-Path $taskRoot 'package-portable.ps1')

