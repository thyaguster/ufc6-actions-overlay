# UFC 6 Actions Overlay v0.9.10

- Add session-only Protect career longevity to Cheats, using a checked immediate setter guard for the selected career.
- Preserve the current amount, allow increases, and restore normal loss immediately when switched Off.
- Keep adjacent packed fitness bits and unrelated fighters/fields unchanged; disable on reset or invalid/changed career bindings.
- Log intercepted decreases to help verify natural career loss during the live test.

Validated in a live Parsec career fight: longevity stayed at 9,895 and the guard intercepted one natural decrease. Isolated native tests cover immediate Off, allowed increases, neighboring fitness bits, unrelated fighters, career binding changes and reset. Installer, controller/rendering and safe handoff checks passed.

# UFC 6 Actions Overlay v0.9.9

- Add local support-report export from Settings and setup, including component hashes/build comparisons, runtime/module information and bounded, path-redacted overlay logs. No uploads or save collection.
- Add core build markers to logs and keep a desktop support path available when the overlay cannot be seen.
- Add a confirmed one-time career fitness restoration to 100%, using the existing checked selected-user hook and game setter.
- Scroll Cheats as it grows, preserving row spacing and controller/mouse navigation.
- Add a confirmed one-time refill to seven training days, bound to the selected fighter's verified camp reference, row and week.

Validated in a live Parsec career: fitness restores to 100%, training days refill to seven without changing the week, and days decrease normally afterward. Installer, diagnostics privacy/archive tests, 29 Python tests and native controller/rendering/handoff checks passed.

# UFC 6 Actions Overlay v0.9.8

- Recognize PORTHEON 6.2's rebuilt bundled overlay files and upgrade them through the existing verified, backed-up transaction. Unknown DLLs remain protected.
- Install the current startup bridge together with the current menu; keep the player's original threading runtime. No manual DLL deletion is required.
- Detect a missing original runtime during Check folder and explain recovery before enabling installation.
- Preserve overlay preferences and launcher settings.

# UFC 6 Actions Overlay v0.9.7

- Replaces the classic installer and updater windows with a matching dark theme, rounded panels and buttons, a custom title bar, and crisp UFC 6 branding.
- Adds the modern Windows Explorer folder picker for Browse.
- Keeps the existing compatibility checks, installation/rollback behavior, update verification, and normal/Parsec support. Game code is unchanged.

Validated installer/update checks, rendered both windows for visual review, and exercised Browse and cancellation in the actual modern folder picker without modifying game files.

# UFC 6 Actions Overlay v0.9.6

- Replaces the U6 monogram with a full **UFC 6** icon: white UFC lettering and a large red 6.
- Updates the installer, updater, GitHub branding, and editable vector artwork. Windows ICO sizes cover 16–256 px.

Icon-only release. Installer and updater validation checks passed; game behavior is unchanged.

# UFC 6 Actions Overlay v0.9.5

- Adds **Settings > Check for updates...** and an installer update button. The desktop checker finds the latest stable release on GitHub, offers newer versions, verifies the installer against release SHA-256 checksums (and the GitHub asset digest when provided), then opens setup with your game folder filled in. Downloads and installation require user actions; there are no silent startup updates.
- Adds an original U6 icon to the installer and updater, with 16–256 px ICO sizes and editable SVG artwork.
- Puts simple installation steps at the top of the README and clarifies that one install supports normal UFC 6 and the verified Parsec engine without extra overlay steps.
- Smoother text, button symbols, switches and outlines with 2x supersampling at the selected UI scale. Unchanged frames reuse a cached image to keep the extra rendering work off normal frames. Settings scroll markers are drawn as shapes, and text bounds are checked from 50% to 300%.
- **Cheats > Savegame backups...** creates separate, timestamped ZIPs in **Documents / UFC6 Actions Overlay / SaveBackups**, with an action to open the folder. Save in the game and wait for autosaving to finish first; unsaved progress is not included.
- Backups include default-user and guest-user UFC 6 saves, preserve their relative `_SaveData` paths, and contain a SHA-256 manifest. Files and archive contents are verified; detected source changes or failures discard the incomplete ZIP while leaving saves and older backups untouched.
- Backup work runs outside the game-rendering thread using the bundled helper. No separate Python installation is needed, and removing the overlay keeps backups.

Validated with native controller/cheat/power-stub checks, renderer/text bounds at 50–300%, backup round trips and failure cases, two consecutive launches of the packaged helper, a verified backup of the user's 25 save files, installer upgrade/removal/rollback tests, normal/Parsec shared-install coverage, updater rejection fixtures, and an actual verified GitHub download without launching it.

# UFC 6 Actions Overlay v0.9.3

Adds Player 1 cheats, career additions, integrated controller-icon patching, and refreshed controls.

- **Cheats** is on the main menu. Infinite stamina and No damage use labeled On/Off pill switches and follow Player 1's controller assignment in normal and Parsec mode. Choices can be set before a fight and remain session-only.
- **Career cheats** adds selectable amounts of money or skill points to the loaded offline career, once per confirmation. Current balances are shown. Keep a save backup before experimenting; changes use the game's normal saving.
- **The [UFC 6 Xbox Icons mod](https://github.com/thyaguster/ufc6-xbox-icons) is already included; no separate download or installation is needed.** Open **Settings > Patch in-game controller icons...**, choose Xbox or PlayStation, then confirm **Apply and restart game**. This changes the game artwork separately from **Overlay button icons**. It checks the supported game build and all edited ranges, keeps a verified local restore backup, then applies after normal game/launcher shutdown and relaunches with saved settings. No game artwork is bundled.
- Fixes repeated icon switches after restart by isolating each packaged helper's Python runtime and clearing its temporary state before launching the game.
- Refreshes the UI with pill switches, compact value selectors, rounded rows, subtle focus outlines, and distinct quit/power styling. Fixes clipped text at different UI scales and uses the same two-pixel row gaps on Main, Settings, and Cheats.
- Restart PC's auto-launch indication is now inline text and a status dot, keeping it clear of the row borders. The confirmation also states whether auto-launch is enabled and where to change it.
- Adds updated screenshots, including Cheats and the controller-icon patch page.

Validation: controller ownership and Parsec regressions; native cheat fixtures; confirmed in-game stamina independence and career additions; font bounds at 50–300% scaling; game-icons validation/restore/rollback fixtures; a second packaged-helper launch after temporary-runtime cleanup; installer upgrade/removal/rollback checks. PC power tests use stubs and never power off or reboot the test machine.

Downloads: **UFC6-Actions-Overlay-Setup-0.9.3.exe** (recommended) or **UFC6-Actions-Overlay-0.9.3.zip** (installer, prebuilt components, source and licenses).

# UFC 6 Actions Overlay v0.8.8

Adds screenshot filename feedback, folder access, and confirmed settings reset.

- Successful screenshots show the saved filename and a result page with Open screenshots folder / Back, starting on Back. Captures still exclude all overlay pixels.
- Settings > Screenshots folder opens the destination even before the first capture. Explorer opens with the game minimized.
- Late captures never reopen a hidden overlay or replace another settings/confirmation page.
- Settings > Reset overlay settings... starts on Cancel and restores overlay preferences and launcher defaults only after confirmation.
- Reset keeps saved screenshots, unrelated INI sections, and the Windows Game Bar preference. Settings save atomically; save failures keep previous settings active.

Validated with real-DLL navigation/rendering checks, reset persistence and save-failure tests, a GPU screenshot readback test, and existing controller, launcher, power-stub and installer checks. No real PC shutdown/restart is used in tests.

Downloads: **UFC6-Actions-Overlay-Setup-0.8.8.exe** (recommended) or **UFC6-Actions-Overlay-0.8.8.zip** (installer, prebuilt files, complete source and licenses).

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
