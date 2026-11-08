# UFC 6 Actions Overlay v0.8.1

First public release of the controller overlay and standalone Windows installer.

## Downloads

- **UFC6-Actions-Overlay-Setup-0.8.1.exe** — recommended. One file with all overlay components built in. No Python or compiler required.
- **UFC6-Actions-Overlay-0.8.1.zip** — setup EXE, prebuilt mod files, complete project source, build scripts, tests, and licenses.
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

The game and emulator/launcher are not included. This tool does not patch the game's executable or assets. The menu does not pause emulation. A fresh game session is opened after restart; unsaved game progress is not restored.

Source is GPL-2.0-only; bundled Vulkan headers retain their own licenses. See the repository and full ZIP for source and notices.
