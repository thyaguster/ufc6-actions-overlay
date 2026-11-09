# UFC 6 Actions Overlay v0.8.7

Adds a controller ownership indicator and **Settings > Controller access**.

- Shows Player 1/2 for assigned navigation input, or a waiting indicator before an eligible controller claims the menu.
- Choose **Either** (default) or **Player 1 only**. The restriction applies to navigation, opening combinations and Guide; F10 remains available.
- Native input uses the verified engine's player assignment rather than treating SDL instance IDs or arrival order as player numbers. Direct XInput uses Windows controller slots, with slot 1 as Player 1. Unassigned native input displays Controller.
- Switching access releases the old owner and waits for eligible held navigation controls to be released. A newly eligible held Guide is ignored until release and a fresh press. Other controllers cannot take over an already-owned menu, and owner disconnection closes the menu safely.
- Settings scrolls to keep the selected item visible, with direction indicators.
- F10-opened menus accept fresh direct XInput navigation as well as native engine input.

Tests cover arbitrary native IDs and reversed arrival order, both owner labels, held buttons during access changes, saved preferences in a fresh process, restricted Guide/chord/navigation, owner disconnect and keyboard recovery. Existing launcher, Parsec routing, power-stub and installer checks remain included.

Downloads: **UFC6-Actions-Overlay-Setup-0.8.7.exe** (recommended) or **UFC6-Actions-Overlay-0.8.7.zip** (installer, prebuilt files, complete source and licenses).

# UFC 6 Actions Overlay v0.8.6

Adds **Settings > Launcher setup...** for Restart game and post-reboot auto-launch.

- Browse to a launch executable (another launcher or the supported game engine), set arguments and an optional start folder, and choose whether to wait for the previous launcher to exit.
- The current PORTHEON executable, arguments and cleanup wait remain the default. Custom launchers that stay open can restart without waiting for their exit.
- The desktop setup window supports browsing and typing; the game minimizes while it is open and restores on close.
- Arguments retain quotes and Unicode characters. Settings save atomically while preserving unrelated overlay preferences.
- Both immediate restart and one-time post-reboot launch use the same saved configuration. Missing/invalid launch files are rejected before the game is closed.
- Exact supported-engine process checks, normal game closure and non-forcing PC power behavior are retained. Choosing a launcher does not enable unsupported engine revisions.

Validation includes native dialog controls, configuration persistence/Unicode, preserved unrelated settings, a rejected missing launcher with the game left running, custom launcher paths and working folders, persistent launcher restart, and simulated custom post-reboot launch. All PC power tests use the stub helper; no real reboot or shutdown is performed.

Downloads: **UFC6-Actions-Overlay-Setup-0.8.6.exe** (recommended) or **UFC6-Actions-Overlay-0.8.6.zip** (installer, prebuilt files, complete source and licenses).

# UFC 6 Actions Overlay v0.8.5

Adds a persistent warning when Game Bar takes controller input while the overlay is open, including when Game Bar itself is invisible.

- Shows the configured fallback combination using Xbox or PlayStation labels, plus F10.
- Explains how to restore input and directs users to turn **Windows Game Bar shortcut** Off in Settings.
- Clears automatically when input returns; no repeated notifications or automatic Windows preference changes.
- Cancels PC shutdown/restart holds and countdowns when input is redirected, and rejects queued power actions while input is unavailable.
- Reads the Windows Game Bar input-redirection API on a background thread, so service delays cannot block the overlay or game renderer. If the API is unavailable, normal overlay behavior remains available.

Validation: real-DLL tests cover warning/clear behavior, redirected-input suppression, a recorded custom PlayStation shortcut, keyboard closing, fallback reopening, and power-countdown cancellation. Existing controller, Parsec routing, restart/power stub, and installer checks remain included. The native background monitor was smoke-tested against Windows without changing Game Bar settings.

Downloads: **UFC6-Actions-Overlay-Setup-0.8.5.exe** (recommended) or **UFC6-Actions-Overlay-0.8.5.zip** (installer, prebuilt files, complete source and licenses).

# UFC 6 Actions Overlay v0.8.4

Adds a Windows Game Bar shortcut setting to resolve Guide navigation conflicts, plus compatibility with PORTHEON's verified Parsec/two-player engine.

## Fixed

- Guide navigation accepts both direct XInput and engine controller events, so a bridge that only reports Guide through XInput can still navigate through SDL/HID. A fresh navigation press claims ownership. Xbox combinations use the matching XInput pad when available. Release events and buttons held on a different pad do not claim or block it.
- Settings now includes **Windows Game Bar shortcut**, showing the actual Windows preference. Turn it Off to keep Game Bar from consuming Guide navigation. It changes the current Windows user's preference only after an explicit action; Win+G remains available. Setup/startup never change it automatically, and uninstall leaves this user choice in place. Unknown registry values are preserved.
- LB + RB + Menu (or the configured combination) and F10 remain available independently of Guide.
- D-pad and left-stick navigation are supported through both input paths.
- Production rendering is in-frame only; the old desktop popup fallback cannot appear behind the game.
- Menu input, confirmation holds, and shortcut capture follow the controlling pad. Disconnect and reconnect clear stale button state.
- Verified `UFC 6 Parsec.exe` support preserves its existing routing wrappers with five-byte relay hooks in checked unused code padding. The original engine file and Parsec patch are not changed.
- Engine file fingerprints are verified before hooks are installed. Unknown engines and occupied relay padding are rejected.
- The Vulkan layer, process checks, and restart/power handoffs recognize the verified Parsec filename.

Live tracing confirmed that Game Bar redirected controller input while its visibility flag stayed false. Disabling its controller shortcut restored uninterrupted Guide navigation in the user's Parsec session.

Tests cover the isolated Windows registry preference and the actual settings row without changing the host's Game Bar preference, plus real DLL input handling with two simulated pads, Guide release, held secondary input, ownership isolation, disconnect/reconnect, concurrent Parsec relay calls, intact continuations, rejected padding, and standard/Parsec restart handoffs. Windows power APIs remain disabled in test helpers.

The supported normal engine remains `3309ab23f`; its verified Parsec derivative is `ad94dc7e30ad12c8078d14d96520c1d48d4b0c658604cc3dcc1d68a3e228773d`. Both normal and two-player routing remain protected by exact checks. No engine, Parsec binary, or original threading runtime is included in the download.

## Startup repair retained from v0.8.2

- A non-system threading-runtime bridge enables the Vulkan layer inside the game process. The layer explicitly loads the app-local overlay core beside Windows' system `version.dll`.
- All 135 runtime exports forward to the user's verified original. Setup preserves its bytes as `ufc6_pthread_original.dll`; uninstall restores it. The original runtime is never distributed. Keep that copy and use this version's setup to uninstall.
- Large startup buffers stay off GPU-worker thread-attach stacks. Missing overlay components leave normal runtime forwarding active without enabling a broken Vulkan layer.
- Windows' system-DLL preference is preserved. No system-wide Vulkan layer is registered.
- A launcher or other non-game Vulkan probe does not load the overlay core.
- Installer tests cover runtime preservation/restoration, reinstall, unknown or missing originals, and rollback at every commit failure. Startup tests enable PreferSystem32Images at process creation and check explicit core loading, small-stack threads, and the missing-overlay case. Real GPU checks cover startup, scaling, and clean screenshots through the bridge.

The main game-action UI is unchanged from v0.8.1; Settings adds the Guide toggle and Windows Game Bar shortcut row. Restart and post-reboot auto-launch still use the supported UFC6.exe launcher; custom launcher configuration is not part of this update.

## Downloads

- **UFC6-Actions-Overlay-Setup-0.8.4.exe** — recommended. One file with all overlay components built in. No Python or compiler required.
- **UFC6-Actions-Overlay-0.8.4.zip** — setup EXE, prebuilt mod files, complete project source, build scripts, tests, and licenses.
- **Overlay-Screenshots-0.8.1.zip** — gallery from v0.8.1, including the main menu and Settings over the game menu. The current Settings preview is in the README.
- **SHA256SUMS.txt** — checksums for these download files.

## Install

Close UFC 6 and its launcher. Open Setup, choose the UFC 6 game folder or the `bin` folder containing `UFC 6.exe`, and select **Install / Update**. Launch the game normally. Hold **LB + RB + Menu** for about one second, then release, or press **F10**.

Use the same setup's **Remove overlay** button to uninstall. Settings and screenshots are kept. Unknown mods are not overwritten; recognized older files are backed up, and failed changes roll back.

## Features

- Resume, take a clean game screenshot, restart the game, or quit normally.
- Separate Settings page with Xbox/PlayStation prompts, a configurable controller shortcut, and 50–300% UI scaling.
- Quit game directly below Settings, with a gap before the PC restart/shutdown actions.
- Confirmed PC shutdown or restart with a 3-second hold and cancellable 10-second countdown.
- Optional **Launch UFC 6 after PC restart**, On by default. Status is shown beside Restart PC and in its confirmation. Auto-launch happens once at the same user's next Windows sign-in.
- Screenshots save to the Windows Pictures/Screenshots known folder and exclude the overlay.

## Compatibility and verification

**Windows x64, PORTHEON UFC 6 engine `3309ab23f` only.** Setup verifies the engine SHA-256 and bundled file hashes. Other engine builds are rejected.

The in-frame overlay and controller navigation were verified in UFC 6. Automated checks cover menu safeguards, rendering/persistence, Vulkan scaling and clean screenshots, installer conflicts/upgrades/rollback, and restart handoffs. PC power tests use a separate helper with Windows power APIs compiled out; they do not reboot or shut down the test machine.

The startup repair was verified in the updated PORTHEON setup, and the user confirmed the overlay appears and controller navigation works again.

The game and emulator/launcher are not included. This tool does not patch the game's executable or assets. The menu does not pause emulation. A fresh game session is opened after restart; unsaved game progress is not restored.

Source is GPL-2.0-only; bundled Vulkan headers retain their own licenses. See the repository and full ZIP for source and notices.
