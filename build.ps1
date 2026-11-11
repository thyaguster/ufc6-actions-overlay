param([switch]$Test)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
Set-Location -LiteralPath $root
$compiler = Join-Path $root '.tools\ziglang\zig.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'Install the local compiler: python -m pip install --target .tools ziglang==0.17.1' }
New-Item -ItemType Directory -Force -Path 'dist', '.build' | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root '.build\zig-global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $root '.build\zig-local'
# The committed manifest records the last shipped payload fingerprints. Carry
# them forward so future GitHub builds can upgrade existing helper EXEs too.
$previousRelease = $null
if (-not $Test -and (Test-Path -LiteralPath 'dist/release.json')) {
    $previousRelease = Get-Content -LiteralPath 'dist/release.json' -Raw | ConvertFrom-Json
}
$sources = @('src/quit_menu.c','src/version_proxy.c','src/exact_hooks.c','src/restart_client.c','src/gamebar_shortcut.c','src/gamebar_monitor.c','src/launch_config.c','src/launcher_dialog.c','src/screenshot_folder.c','src/live_cheats.c','src/cheat_bindings.c','src/career_cheats.c','src/game_icons.c','src/save_backups.c')
$flags = @('cc','-target','x86_64-windows-gnu','-O2','-Wall','-Wextra','-Werror','-shared','-Isrc','-luser32','-lgdi32','-lkernel32','-ladvapi32','-lcomdlg32','-lshell32','-lole32','-luuid')
if ($Test) { $flags += '-DQUIT_MENU_TEST'; $output = 'dist/quit_menu_test.dll' }
else { $output = 'dist/version.dll' }
& $compiler @flags @sources 'src/version.def' '-o' $output
if ($LASTEXITCODE) { throw "Compiler failed: $LASTEXITCODE" }
& $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror -municode '-Wl,--subsystem,windows' src/restart_helper.c src/launch_config.c -lshell32 -lkernel32 -luser32 -ladvapi32 -o dist/ufc6_menu_restart.exe
if ($LASTEXITCODE) { throw 'Restart helper compilation failed' }
& $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror -shared -Isrc src/pthread_bridge.c src/pthread_bridge.def -lkernel32 -o dist/libwinpthread-1.dll
if ($LASTEXITCODE) { throw 'Startup bridge compilation failed' }
if (-not $Test) {
    if (-not (Test-Path -LiteralPath 'dist/ufc6_menu_icons.exe')) { & (Join-Path $root 'build-icons.ps1'); if ($LASTEXITCODE) { throw 'Icons helper build failed' } }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror -shared -Ivendor/vulkan-headers/include src/vulkan_menu_layer.c src/screenshot_png.c src/screenshot_folder.c src/vulkan_layer.def -lkernel32 -lole32 -lwindowscodecs -luuid -lshell32 -o dist/ufc6_menu_layer.dll
    if ($LASTEXITCODE) { throw 'Vulkan layer compilation failed' }
    Copy-Item -LiteralPath 'src/ufc6_menu_layer.json' -Destination 'dist/ufc6_menu_layer.json'
    & (Join-Path $root 'build-updater.ps1'); if ($LASTEXITCODE) { throw 'Updater build failed' }
    $manifest = [ordered]@{
        version = '0.9.11'
        parsec_engine_sha256 = 'ad94dc7e30ad12c8078d14d96520c1d48d4b0c658604cc3dcc1d68a3e228773d'
        engine_revision = '3309ab23f'
        engine_sha256 = 'f3c42126bd9df5c12cb69c1c57233a94ba024c50929ba0dfe20e715521f691a2'
        dll_sha256 = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
        previous_dll_sha256 = @('dccc774294f60e3dbec5792dcb2f8e42de0cec7c4b8ec1e1b3fd1743541ee575','2c261b3743af1db6f605c95a8f1ccd03bd48cf34c8dcab378f5feb4751ddd0de','59ec6578fddf0fef57a16f5b3381b13ed6ea724dafc88923ee0c098b8fca6622','5e02d201152bb4943aeb961352e2023ed4a1eb31ce8ad0b34eb1b54b285e9a3e','d37aa589dbbe4bebba91c20f65fa0599fb9bebdbd8eda8ad9affdf1978e2e34e','5f1c88a6c0fce0a298c1d28eee5388766e708d54f65b75458ef1d98c531cb6de','615bbe844166ee81a7e731ebd2d26c3b2c383800d59fa201f99135296296b43a','e02127b75b064f70d94100743a8fdbdf99941bdd24a5659b354d14c40016a5bb','2ee03b23285889c6160b94a59d30bc57466b5ef9310e21f23e0ad4592c39d8d0','7f664dddaa22f4d7be3b1a7fd3eb41783f1a76a6d1b4073faade454568eb66e0','0533ad86d7350a67868d51f057dd5bef037a996ad5f451ecc19d91ca10e185ef','3a2f4e9d7c2086bef666f1b3946ae845cb6d1b7c93157ad033cd7613768921f1','e913d77d00dac1ca9367c4b371f032418caaf7118278ad14ed955aeb67cecc6b','a5c7eccf2333ccfdf85a91a751a5afa6758be3488e11314cc8b5175841fd5e63','943bdcae82e5260e7fe4011937c92d3d014c676b3426f5961e157ca8ba3aaa0c','e70939f0d86c17e2324a5ad799f6b67ebd745af6cd6539048506fd66608efa92','f1f76f54bdd00c8e1412d983c8c02cfd40588f2a39775a876858b0d5de8d8f2c','80bae003123512eb868045a282a128fe4000e1a19de802ce5987cb51666eedfb','84504fd97f7eac21209b7ea8a1a6f005c591edac762efee5d10dc4a67c3a91a6','0ffacad46cc88cd6861e4fc940e44277a0145af5003fb9d2be55786318bb56e8','29450785d6e50686e1cec9e81f9bada4240772da58b1667ab8ecec682a961d75','69d6b4a0223702e3896cc02f370b6505b542472fb28163fe123b53f5bd30ea2f','a331d902a47023b261e0d696fe64ae65c80aea65bc850a72a9d9f32c4fca55a0','5bc45aabf9607a228c0c2d7544855fe678b5ff3f720d86f9bfa0ea57d90bfc6a','62b6b7970f3235125c913728997b9f44cf751d832a2ba6c364da78f48a5b8d3c','c64da30447e65a1e6628492a78ba733bf26b9eb1c1bd2e60983fcf94f79b3980','9a6bb5b6d4a1d3a77a116ac1b84ab103f5e07b75ef1d4ae87cd21d08963c493f','75221511636af4f553ef7a86aedf0606485d9f93e83ffe7c6e496cc5f0d390f5','20826c7d547e51f61c3bf1706746f279504e7c8a16afbb3ae0cd7799fe317270','9d7a5429ccf1e79e3c98b9c82248414698e9000daa95ba2f125863dcf09e54fb','cb032e3b275ab2b608839f0d5ff518e3c38d8a13a8333bb65792059121216b36','f418a8844d4ae9097491b4ad7c0bafb4a58476bf83581304e87864b3d6d99f5a','31e8e21276c4ea3156cbf97ebc1258d52fbeb40143e9a5378108bc17063fbc59','2757fe8daf6ff04f97c88ae5ed107800cc7be4a3a3d274228386ee8e4ecc3bee','878bd2d3856b469d74a7eb2655cc6e9b4bca06fafc08527709f258db354fbaf2','77ef6d607be1c1f9a07ff39952aec8091a6256e98f85f5bce99f617624ae3b02','a1bc5af4a887a613f3067e4aa5b2508d384def0dfb152857d9b3c096dcdd2a21','d76dea7a21a6f3327c9944e923f57fd53d2fe67b6c12f51e07ce0c8a94a779ac','6725630f187d1609296b62420ae50041f4be4a61ba794d90b228a058427317c0','38803c637dcccc1a370e351f07c0efd1bd76ccd49e470fd99b3e5e53974eaa15','f482758e1e9b4edc2b7865aa9cbe5a570a077841ef1b7abebfcf3b642f7836c9')
        runtime_bridge = @{target='libwinpthread-1.dll'; original_name='ufc6_pthread_original.dll'; original_sha256='78ebbc1480d62098aa66307db2ff139abc07c137bc188f5b73bf7b131492932c'}
        companion_files = [ordered]@{
            'ufc6_menu_update.exe' = (Get-FileHash -LiteralPath 'dist/ufc6_menu_update.exe').Hash.ToLowerInvariant()
            'ufc6_menu_icons.exe' = (Get-FileHash -LiteralPath 'dist/ufc6_menu_icons.exe').Hash.ToLowerInvariant()
            'libwinpthread-1.dll' = (Get-FileHash -LiteralPath 'dist/libwinpthread-1.dll').Hash.ToLowerInvariant()
            'ufc6_menu_layer.dll' = (Get-FileHash -LiteralPath 'dist/ufc6_menu_layer.dll').Hash.ToLowerInvariant()
            'ufc6_menu_layer.json' = (Get-FileHash -LiteralPath 'dist/ufc6_menu_layer.json').Hash.ToLowerInvariant()
            'ufc6_menu_restart.exe' = (Get-FileHash -LiteralPath 'dist/ufc6_menu_restart.exe').Hash.ToLowerInvariant()
        }
        previous_companion_sha256 = @{'ufc6_menu_icons.exe'=@('c15a1fe33f684fbc73be0bad5da032c4a8cb17ad14301134176bf923584d1d42','e14e61e0576ed9dec6265edb74a94d93c3b89f54c5f244a253e2af5c2fe46c4b');'libwinpthread-1.dll'=@('68a8ba895e4253e42577b1634b4f3421ff6f492c8f4de48d7a212a159b266143','7834ba478b95cc2e680dd7b61cc92fc61f73cd9d3de373121c3448ec0a701ba4');'ufc6_menu_layer.dll' = @('0936ce0405352b439f00dae84ba6dfb8bace20711177723700a1a34dff4dd255','d16d00f5e09687192f9f7243fb25844702a58ff531cf34cb32fb74caef95c9e4','620dc7fe02917c24ddb8476da90cd561030de6ed9e7e7e57b5c3c0a83ca0eebd','d943492814bec60b5b535fc31f33c9a1c2e809239f6e7022a95dfc964cf0e101','0b63b6657e5049609281bda3045c530333fc55c37ce38366bcf582141e036e83','bec28a5cad188a802063c2a0f5f7eac259882a6ff575bb7905f56ca1bbb34e7b','da7b6ca8e9ef149b37ed8986781db071899a843d4e12ab326ba65e7d196a72de','56a5a3dac1d174dc0825d95b0729969967b66ebd538e1263aa06b122d5ce0527');'ufc6_menu_restart.exe'=@('d1c9984d275b12348638d5b3a31c85e75fbf7d742b47680a05137f1f387ad2a7','ab28c2edbc12db244e13c2b2cddc82cbbf827dbf246b216deb87a78982d01b1a','36eaec21972661408381cbebb463c1c71bd57ac544fa3839bd36af3488adcca2','9234b1e02b3dc2e7ad804bdaeb20854898462de159a6a224444aba80325829c3','cc25fdf1e57cb63aa0a0dcb493a77cc5575abf692447713de75e84024bfa0618','92b747b7c5cacd7dfe9c97c085ba436bcd4b53a3272093aa781f991a3a272556','3ab575a182f6f8d899b9a7276c003941d7bd575d2f60738b3931e6db5cae65fd')}
        shortcut = 'Hold LB + RB + Menu for 800 ms'
    }
    # PORTHEON 6.2 rebuilds our older sources with different binary fingerprints.
    # Accept only these verified bundled builds; unknown mods remain blocked.
    $manifest.previous_dll_sha256 += '67e5d77bd4940d9b53936909e0ed1181979bd219e6cfbda75e97b3b7b79ebb2b'
    $manifest.previous_companion_sha256['libwinpthread-1.dll'] += @('24a6575bedb4bc91561a2e31a879613459793fe08cf127ac52b9b253bec53a6d')
    $manifest.previous_companion_sha256['ufc6_menu_layer.dll'] += @('f8b38e46afa08a785bcd35c9a2a32faa9489899a342141b42d2b405a8c278a0b')
    $manifest.previous_companion_sha256['ufc6_menu_restart.exe'] += @('bac16220717756de7141b53e9d14eadc692bdcf633a77f8d9d1d82104697e21e')
    if ($previousRelease -and $previousRelease.engine_sha256 -eq $manifest.engine_sha256 -and [version]$previousRelease.version -le [version]$manifest.version) {
        $oldCore = @($previousRelease.dll_sha256) + @($previousRelease.previous_dll_sha256)
        $manifest.previous_dll_sha256 = @(@($manifest.previous_dll_sha256) + $oldCore | Where-Object { $_ -match '^[a-fA-F0-9]{64}$' -and $_ -ne $manifest.dll_sha256 } | ForEach-Object { $_.ToLowerInvariant() } | Select-Object -Unique)
        foreach ($name in $manifest.companion_files.Keys) {
            $old = @($manifest.previous_companion_sha256[$name])
            $property = $previousRelease.companion_files.PSObject.Properties[$name]
            if ($property) { $old += @($property.Value) }
            if ($previousRelease.previous_companion_sha256) {
                $property = $previousRelease.previous_companion_sha256.PSObject.Properties[$name]
                if ($property) { $old += @($property.Value) }
            }
            $manifest.previous_companion_sha256[$name] = @($old | Where-Object { $_ -match '^[a-fA-F0-9]{64}$' -and $_ -ne $manifest.companion_files[$name] } | ForEach-Object { $_.ToLowerInvariant() } | Select-Object -Unique)
        }
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath 'dist/release.json' -Encoding utf8
}
if ($Test) {
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror tests/icons_process_env_test.c -o .build/icons_process_env_test.exe
    if ($LASTEXITCODE) { throw 'Icons environment fixture compilation failed' }
    & ./.build/icons_process_env_test.exe
    if ($LASTEXITCODE) { throw 'Icons environment fixture failed' }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror tests/career_cheats_test.c src/exact_hooks.c -o .build/career_cheats_test.exe
    if ($LASTEXITCODE) { throw 'Career cheat fixture compilation failed' }
    & ./.build/career_cheats_test.exe
    if ($LASTEXITCODE) { throw 'Career cheat tests failed' }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror tests/live_cheats_test.c src/cheat_bindings.c src/exact_hooks.c -o .build/live_cheats_test.exe
    if ($LASTEXITCODE) { throw 'Live cheat fixture compilation failed' }
    & ./.build/live_cheats_test.exe
    if ($LASTEXITCODE) { throw 'Live cheat tests failed' }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror tests/menu_state_test.c -o .build/menu_state_test.exe
    if ($LASTEXITCODE) { throw 'State test compilation failed' }
    & .\.build\menu_state_test.exe
    if ($LASTEXITCODE) { throw 'State tests failed' }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror tests/exact_hooks_test.c src/exact_hooks.c -o .build/exact_hooks_test.exe
    if ($LASTEXITCODE) { throw 'Hook test compilation failed' }
    & .\.build\exact_hooks_test.exe
    if ($LASTEXITCODE) { throw 'Hook tests failed' }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror tests/gamebar_shortcut_test.c -ladvapi32 -o .build/gamebar_shortcut_test.exe
    if ($LASTEXITCODE) { throw 'Game Bar preference test compilation failed' }
    & ./.build/gamebar_shortcut_test.exe
    if ($LASTEXITCODE) { throw 'Game Bar preference test failed' }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror -municode tests/launch_config_test.c src/launch_config.c -o .build/launch_config_test.exe
    if ($LASTEXITCODE) { throw 'Launch configuration fixture compilation failed' }
    & ./.build/launch_config_test.exe (Join-Path $root ('.build/launch-config-' + [guid]::NewGuid().ToString('N')))
    if ($LASTEXITCODE) { throw 'Launch configuration tests failed' }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror -municode tests/launcher_dialog_test.c src/launch_config.c -luser32 -lgdi32 -lcomdlg32 -o .build/launcher_dialog_test.exe
    if ($LASTEXITCODE) { throw 'Native launcher dialog fixture compilation failed' }
    & ./.build/launcher_dialog_test.exe (Join-Path $root ('.build/launcher-dialog-' + [guid]::NewGuid().ToString('N')))
    if ($LASTEXITCODE) { throw 'Native launcher dialog tests failed' }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror tests/menu_harness.c -luser32 -lgdi32 -o dist/menu_harness.exe
    if ($LASTEXITCODE) { throw 'Harness compilation failed' }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror -municode tests/restart_fixture.c -lkernel32 -o .build/restart_fixture.exe
    if ($LASTEXITCODE) { throw 'Restart fixture compilation failed' }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror -municode '-Wl,--subsystem,windows' -DUFC6_POWER_TEST src/restart_helper.c src/launch_config.c -lshell32 -lkernel32 -luser32 -ladvapi32 -o .build/shutdown_helper_test.exe
    if ($LASTEXITCODE) { throw 'Shutdown test helper compilation failed' }
    New-Item -ItemType Directory -Force -Path '.build/startup-bridge' | Out-Null
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror -shared tests/pthread_original_fixture.c -o .build/startup-bridge/ufc6_pthread_original.dll
    if ($LASTEXITCODE) { throw 'Original-runtime fixture compilation failed' }
    & $compiler cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror tests/startup_bridge_test.c dist/pthread_bridge.lib -o .build/startup-bridge/startup_bridge_test.exe
    if ($LASTEXITCODE) { throw 'Startup bridge fixture compilation failed' }
    Copy-Item -LiteralPath 'dist/libwinpthread-1.dll' -Destination '.build/startup-bridge/libwinpthread-1.dll' -Force
    Copy-Item -LiteralPath 'dist/quit_menu_test.dll' -Destination '.build/startup-bridge/version.dll' -Force
    Copy-Item -LiteralPath 'dist/ufc6_menu_layer.dll','dist/ufc6_menu_layer.json' -Destination '.build/startup-bridge' -Force
    & ./.build/startup-bridge/startup_bridge_test.exe
    if ($LASTEXITCODE) { throw 'Startup bridge mitigation test failed' }
    New-Item -ItemType Directory -Force -Path '.build/controller-ci' | Out-Null
    Copy-Item -LiteralPath 'dist/quit_menu_test.dll' -Destination '.build/controller-ci/quit_menu_test.dll' -Force
    python tests/icon_preview.py .build/controller-ci --reset --controllers
    if ($LASTEXITCODE) { throw 'Two-controller ownership regression failed' }
    python tests/icon_preview.py .build/controller-ci --reset --raw-controllers
    if ($LASTEXITCODE) { throw 'Direct XInput navigation regression failed' }
    python tests/icon_preview.py .build/controller-ci --reset --gamebar --no-render
    if ($LASTEXITCODE) { throw 'Game Bar settings regression failed' }
    python tests/icon_preview.py .build/controller-ci --reset --conflict --no-render
    if ($LASTEXITCODE) { throw 'Game Bar conflict warning regression failed' }
    python tests/icon_preview.py .build/controller-ci --reset --access --no-render
    if ($LASTEXITCODE) { throw 'Native player access regression failed' }
    python tests/icon_preview.py .build/controller-ci --reload-access --no-render
    if ($LASTEXITCODE) { throw 'Player access persistence regression failed' }
    python tests/icon_preview.py .build/controller-ci --reset --raw-access --no-render
    if ($LASTEXITCODE) { throw 'Direct XInput player access regression failed' }
    python tests/icon_preview.py .build/controller-ci --reset --screenshot-reset --no-render
    if ($LASTEXITCODE) { throw 'Screenshot and reset UI regression failed' }
    python tests/icon_preview.py .build/controller-ci --reset --game-icons
    if ($LASTEXITCODE) { throw 'Game icons pages and text metrics regression failed' }
    foreach ($mode in @('--longevity-save','--longevity-reload','--longevity-reload-off')) {
        if ($mode -eq '--longevity-save') { python tests/icon_preview.py .build/controller-ci --reset $mode --no-render }
        else { python tests/icon_preview.py .build/controller-ci $mode --no-render }
        if ($LASTEXITCODE) { throw 'Longevity preference persistence test failed' }
    }
    foreach ($mode in @('--training-save','--training-reload','--training-reload-off')) {
        if ($mode -eq '--training-save') { python tests/icon_preview.py .build/controller-ci --reset $mode --no-render }
        else { python tests/icon_preview.py .build/controller-ci $mode --no-render }
        if ($LASTEXITCODE) { throw 'Training multiplier persistence test failed' }
    }
    python tests/icon_preview.py .build/controller-ci --reset --cheats
    if ($LASTEXITCODE) { throw 'Live cheats UI regression failed' }
    python tests/restart_handoff.py
    if ($LASTEXITCODE) { throw 'Restart handoff tests failed' }
    python tests/restart_handoff.py --custom
    if ($LASTEXITCODE) { throw 'Custom launcher handoff failed' }
    python tests/restart_handoff.py --custom --persistent
    if ($LASTEXITCODE) { throw 'Persistent launcher handoff failed' }
    python tests/shutdown_handoff.py --reboot --custom
    if ($LASTEXITCODE) { throw 'Custom launcher post-reboot handoff failed' }
    python tests/restart_handoff.py --parsec
    if ($LASTEXITCODE) { throw 'Parsec restart handoff failed' }
    python tests/shutdown_handoff.py --parsec --reboot
    if ($LASTEXITCODE) { throw 'Parsec power handoff failed' }
    python tests/shutdown_handoff.py
    if ($LASTEXITCODE) { throw 'Shutdown handoff tests failed' }
    foreach ($scenario in @('', '--fail', '--conflict')) {
        if ($scenario) { python tests/shutdown_handoff.py --reboot $scenario }
        else { python tests/shutdown_handoff.py --reboot }
        if ($LASTEXITCODE) { throw 'Reboot/resume handoff tests failed' }
    }
    python tests/shutdown_handoff.py --reboot --no-auto
    if ($LASTEXITCODE) { throw 'Reboot without auto-launch test failed' }
    python tests/shutdown_handoff.py --reboot --no-auto --conflict
    if ($LASTEXITCODE) { throw 'Reboot without auto-launch registration preservation failed' }
}
Get-FileHash -LiteralPath $output -Algorithm SHA256 | Format-List

