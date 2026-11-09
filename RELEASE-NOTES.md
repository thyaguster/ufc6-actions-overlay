# UFC 6 Actions Overlay v0.8.3

Adds **Settings → Xbox / Guide button**, a saved On/Off control for opening or closing the overlay with the Xbox/Guide button. It defaults to On, preserving existing behavior. Confirm or left/right changes it. The controller combination and F10 remain available when Off. Re-enabling while Guide is held requires a fresh press. Settings now fit six rows inside the same scalable panel.

Tests cover default On, toggling, press-edge handling, independent F10/chord access, actual rendering, persistence across a fresh process, and preservation during installation/removal. No cheat controls are added in this release.

The v0.8.2 startup fix is retained. The supported engine remains `3309ab23f`; its compatibility checks stay in place.

## Startup repair retained from v0.8.2

- A non-system threading-runtime bridge enables the Vulkan layer inside the game process. The layer explicitly loads the app-local overlay core beside Windows' system `version.dll`.
- All 135 runtime exports forward to the user's verified original. Setup preserves its bytes as `ufc6_pthread_original.dll`; uninstall restores it. The original runtime is never distributed. Keep that copy and use this version's setup to uninstall.
- Large startup buffers stay off GPU-worker thread-attach stacks. Missing overlay components leave normal runtime forwarding active without enabling a broken Vulkan layer.
- Windows' system-DLL preference is preserved. No system-wide Vulkan layer is registered.
- A launcher or other non-game Vulkan probe does not load the overlay core.
- Installer tests cover runtime preservation/restoration, reinstall, unknown or missing originals, and rollback at every commit failure. Startup tests enable PreferSystem32Images at process creation and check explicit core loading, small-stack threads, and the missing-overlay case. Real GPU checks cover startup, scaling, and clean screenshots through the bridge.

The game-action UI is unchanged from v0.8.1. Restart and post-reboot auto-launch still use the supported UFC6.exe launcher; custom launcher configuration is not part of this update.

## Downloads

- **UFC6-Actions-Overlay-Setup-0.8.3.exe** — recommended. One file with all overlay components built in. No Python or compiler required.
- **UFC6-Actions-Overlay-0.8.3.zip** — setup EXE, prebuilt mod files, complete project source, build scripts, tests, and licenses.
- **Overlay-Screenshots-0.8.1.zip** — 18 updated images, including the main menu and Settings over the actual game menu.
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
