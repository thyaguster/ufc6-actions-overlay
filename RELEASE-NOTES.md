## v0.9.36 - Decision replay selection timing (decision replay locally validated)

- Wait for a consistent selected-clip count and stable replay slots before applying the decision fallback. Ignore incomplete zero-initialized slots.
- Recover if the native collector overwrites fallback slots later in the same finished fight; do not rewrite slots while playback is active. Preserve the completed native selection for cleanup.
- In v0.9.36, the automatic fallback enabled replay for a unanimous decision with only two selected clips. The user confirmed two replay visits played and exited normally without manual repair; the trace confirms one automatic activation and preserved slots after both exits. A second unanimous-decision test also passed with zero selected highlights: the fallback found a retained clip and both replay visits worked. Rematch cleanup is live-verified. The distinct draw playback path remains unverified in-game.

## v0.9.35 - Decision replay fallback (local build; integrated live test pending)

- Add **Settings > Game & launcher > Replay after decisions**, Off by default. Enable before starting a fresh supported offline fight. Reuse validated native highlight clips when the selected recap is incomplete, including fallback selection for draws.
- Keep fallback slots available across repeated View Replay visits, including idle exits. Preserve new-fight resets. Turning the setting Off affects future fights without invalidating the current replay.
- Keep the Photo Mode Replay page selected through temporary replay unavailability; show its waiting state instead of unexpectedly returning to Camera.
- This does not create missing footage or guarantee a distinct clip per round. An incomplete decision recap may repeat a clip to satisfy the required slot count. Zero-selected-clip draws and repeat entry in the integrated build still need live validation. The earlier session-specific decision prototype successfully enabled playback; its exit cleanup needed manual correction.

## v0.9.34 - Replay speed and controller shortcuts (locally validated)

- Replay Speed offers Native, 0.1x, 0.25x, 0.5x, 1x and 2x. Native preserves the game-selected slow motion; numbered rates compensate it using bounded consecutive update time. Native speed/hold fields remain untouched, and a native zero-speed hold is respected. The selection lasts for the overlay session and defaults to Native on launch.
- On the visible Replay panel, Y/Triangle pauses/resumes and LB/RB or L1/R1 steps backward/forward. D-pad selects and adjusts rows; B/Circle returns to Camera. Shoulder shortcuts do not also move the camera vertically.
- Matching controller artwork appears on the panel. Native speed compensation and controller routing tests pass. User confirms all speeds and controller shortcuts work in-game.

## v0.9.33 - Expanded playback correction (locally validated)

- Correct zero elapsed time supplied by the held finish cinematic during Available recording playback. Use consecutive update ticks only while expanded playback is active, unpaused and focused; discard gaps above 250 ms.
- Keep native replay speed, normal finish-clip timing, pause/step controls and cinematic ownership unchanged.
- v0.9.32 live test confirmed earlier footage and both step directions, but automatic playback failed. v0.9.33 is user-confirmed: automatic playback, pause, bidirectional stepping and finish-clip restoration work. The passive trace also confirms advancing playback while the finish cinematic stays held. Arbitrary-length whole-fight retention remains unverified.

## v0.9.32 — Available recording (local build, live validation pending)

- Add **Range → Finish clip / Available recording** to Photo Mode's Replay panel. Available recording selects the continuous stored section containing the finish clip, pauses at its start, and excludes gaps in either recording service.
- Returning to Finish clip restores its saved position. Exiting Photo Mode or losing focus restores the owned finish range; foreign or changed replay state is preserved.
- Keep the finish cinematic paused while browsing or playing the expanded recording, and pause recorded playback at the available section's end.
- Allocate 256 MiB for fresh auxiliary replay recorders on the verified engine, then restore the original settings asset. Existing recorders and files are not resized or edited.

The private allocation, continuous coverage of one two-round fight, early-frame seeking and a short round-boundary scan are user-confirmed. The integrated controls, uninterrupted playback and longer/five-round retention still require live validation. This is **not a guarantee of whole-fight recording**, and nothing has been published to GitHub.

## v0.9.31 — Next hit KO

- Added **Next hit KO** to Cheats, beneath No damage. Confirm to arm for 25 seconds; confirm again to cancel. A clean head strike can knock out Player 1's opponent and generate the normal finish replay.
- Resolves the opponent from controller ownership, automatically disarms after a KO, and cancels on timeout, focus loss, Photo Mode/replay entry or changed fighter bindings. Starts unarmed each session.
- Preserves existing nonzero game overrides and rejects ambiguous controller mappings or fighters already finished.
- The integrated Parsec action is user-confirmed: arm/cancel, opponent-only KO, automatic disarm and normal finish replay all work.

# UFC 6 Actions Overlay v0.9.30 — local replay steps and background audio

- Add Step size to the Photo Mode Replay panel: 10, 25, 50, 100, 250, 500 or 1000 milliseconds, with a finer 10 ms default and a millisecond position display.
- Save and reload the selected step size. Reset settings restores the default.
- Add Game & launcher → Mute when unfocused, Off by default. Silence only the verified game PCM feed while minimized or in the background, then forward normal samples on focus or when disabled. Windows mixer preferences and other applications are untouched.
- Verify both supported engine profiles, the PCM submission call site and dispatch target before enabling the audio hook.

The background-audio prototype is confirmed in Parsec mode. Native fixtures and UI rendering/persistence checks pass; the integrated installation still needs its live check. This build does not add slow replay playback, full-fight recording or new unlock patches. Those are separate investigations. It remains local and unpublished.

# UFC 6 Actions Overlay v0.9.29 — local replay presentation correction

- Pause the native cinematic presentation together with the recorded replay clock, preventing the delayed black screen observed during paused Photo Mode replays.
- Use the existing P / A / Cross and Replay page controls automatically; no diagnostic function keys are required.
- Restore the owned native cinematic and script pauses on replay resume, Photo Mode exit, focus loss or stale UI input. Preserve pauses that were already active and reject changed cinematic/entity contexts.
- Verify the cinematic timer and paired native pause/resume signatures before installing replay hooks.

The full native-pause prototype is visually confirmed in Parsec mode: the picture stays visible and playback resumes. Coupled replay/cinematic fixtures cover a sixty-second hold, bidirectional stepping and restoration. The user confirmed the integrated v0.9.29 pause, both step directions and restoration in Parsec mode. Brief flashes still occur during replay, including while paused; those remain under investigation. This build remains local and unpublished.

# UFC 6 Actions Overlay v0.9.28 — local replay controls build

- Add a Replay page to the translucent Photo Mode panel during the game's View Replay: pause playback and adjust position in 0.1-second steps.
- Pause the native playback clock, keeping the selected moment held without looping. Native seek changes both the selected frame and the controller clock so playback resumes there.
- Use the existing P / A / Cross freeze control for replay-clock pause when a replay is active. Camera, effects, watermark and screenshots remain available.
- Release replay pause on Photo Mode exit, focus loss, changed playback ownership/clip or stale UI updates. Production controls do not have the prototype's fifteen-second test limit.
- Offer F4 as an alternative Photo Mode page key when F8 is assigned to screenshots, and display the available key.

The private native pause/seek prototype is visually confirmed in Parsec mode, including resume from the selected moment. The integrated panel build still requires its own in-game check. Controls apply within the current recorded replay clip; this does not add full-fight recording or launch the game's replay viewer. This build is local and unpublished. It carries the v0.9.27 career diagnostics; the affected client's code mismatch is still under investigation.

# UFC 6 Actions Overlay v0.9.27 â€” career initialization test build

- Initialize checked career hooks without waiting for the unrelated fight stamina-flag registration. A loaded career can otherwise remain permanently unbound.
- Recover a temporary unsupported-code state once matching guest functions are available.
- Log bounded career binding reasons and every requested career action, distinguishing missing user lookups, schema/layout failures, read-only records and hook problems. Keep save-data guards intact.
- Replace the misleading "Open the career hub first" failure with a verification/support-report message.

The affected client's v0.9.18 report shows a matching installation without elevation and no verified career binding. The removed startup dependency is reproduced in an isolated native fixture; its exact role on the affected PC remains unconfirmed. This is a local test installer, not a published release.

# UFC 6 Actions Overlay v0.9.26 â€” local offline-identifier correction

- Include the native PSN/EA offline warning identifiers TXT_UNABLE_TO_LOGIN and TXT_UNABLE_TO_LOGIN_EA, alongside the separate Blaze connection warning, in the state-entry bypass.
- Preserve unrelated login-error popups and the original failure notification.
- Recognize the verified v0.9.25 recipe during updates without replacing the original script backup; unknown backup profiles and edits are still rejected.

The v0.9.25 live test still displayed the online warning, while the roster warning was absent. Native handler inspection found that the filter omitted the actual offline-login identifiers. The user confirmed automatic main-menu entry with both warnings absent in Parsec mode. Visual auto-confirm stayed Off, the session recorded zero injected confirmations and the original backup was unchanged. Normal-mode live validation and the actual overlay enable/restore handoff remain pending.

# UFC 6 Actions Overlay v0.9.25 â€” local correction candidate

- Move the known-offline warning continuation into the active login-failure navigation state, replacing the too-early callback acknowledgement introduced in v0.9.24.
- Keep the original popup handling for other login errors and retain LOGIN_TRANSITION_FAILURE notification for both paths.

The v0.9.24 refinement failed live verification: the user reported network warnings returning while visual auto-confirm was Off. The corrected state-based version passes recipe/backup/rollback checks; live verification is pending.

# UFC 6 Actions Overlay v0.9.24 â€” local validation build

- Add Game & launcher â†’ Skip title & network warnings, with Enable bypass and Restore startup screens.
- Use normal profile initialization and native failure acknowledgement transitions instead of screen matching or injected inputs. Limit login suppression to the known offline error; retain other login errors.
- Back up original script ranges locally, verify hashes, reject changed/unsupported assets, preserve archive sizes and verify rollback on write failure.
- Reuse the bundled helper and configured launcher for Cancel-first close/patch/relaunch in normal or Parsec mode. Keep the visual auto-confirm fallback separate.

Compatibility: PPSA23566 01.006.000. The broad prototype reached the main menu in Parsec mode with visual confirmation Off and no injected confirmations. The integrated, error-specific login refinement still needs in-game verification. Brief startup transitions can remain. Restore game-script patches before uninstalling the overlay.

# UFC 6 Actions Overlay v0.9.23

- Add Settings â†’ Performance with PORTHEON's existing crowd/animation, particles/sparks, depth-of-field, dynamic-resolution and render-scale options. Display next-launch settings, with no claim of immediate live application.
- Add Save for next launch, Cancel-first Save & restart game and unsaved-edit discard confirmation. Keep normal and Parsec mode on the same configuration and existing restart handoff.
- Preserve unrelated launcher values and Unicode data; save via checked temporary replacement, reject detected concurrent changes and avoid writing to a different custom launcher.

Validation: isolated native settings/write-conflict/persistence tests, controller/UI page and Cancel-first restart tests, existing regression checks. The user confirmed the integrated Performance page and discard behavior; PORTHEON crowd-off patches were applied and visually working.

# UFC 6 Actions Overlay v0.9.22 â€” local stability candidate

- Preserve the selected Photo Mode HUD visibility while the overlay tools/settings are open. Remove the automatic HUD show/hide transition previously caused by opening/closing the tools. Explicit HUD toggles, focus-loss and Photo Mode exit still restore visibility normally.
- Add bounded UTC lifecycle records for capture requests/results, screenshot-policy changes and Photo Mode/tools transitions, without recording player names or image contents.

The repeated PORTHEON texture-cache CPU-dirty assertion is still under investigation. A prior screenshot completed successfully before a crash with Photo Mode active and the tools opening. This candidate removes one unintended visibility transition; it is not yet a confirmed root-cause fix.

# UFC 6 Actions Overlay v0.9.21

- Add watermark text colors: White, Black, Silver, Gold, Coral, Sky, Mint and Violet, selectable live on the translucent Watermark page and in the full menu. Adapt contrast shadow/Glass backdrop for dark lettering and save the color preference.
- Replace the separate Screenshot + overlay action with an Off-by-default **Settings â†’ Files & support â†’ Include overlay in screenshots** switch. Normal screenshot controls follow it; remove D-pad Left, the default Ctrl+Shift+F12 action, the extra Keybinds entry and Camera-panel hint. Ignore retired overlay-shot bindings without changing normal screenshot assignments.

Validation: native settings/input/persistence, Xbox/PlayStation UI layout, GPU color/screenshot capture and installer checks. In-game verification pending.

# UFC 6 Actions Overlay v0.9.20

- Add Camera, Effects and Watermark pages to the translucent Photo Mode panel. Menu/Options or F8 cycles pages; D-pad or W/S and A/D selects/adjusts effects live while the camera sticks remain available. B/Circle returns to Camera; Esc exits directly. Keep the full tools pages available.
- Add Clean, Glass, Editorial and Signature watermark styles, four font families, existing size/corner/opacity controls, and cached high-resolution lettering with filtered GPU scaling. Read the automatic PORTHEON name as before; no new text-entry box is needed.
- Keep editor-page D-pad input separate from HUD, cage, freeze and overlay-shot shortcuts. Clean screenshots remain available on X/Square; assigned user shortcuts retain priority.

Validation: native input routing/persistence/raster tests, Xbox/PlayStation panel font/layout checks, controller regressions and real-GPU screenshot checks. In-game verification pending.

# UFC 6 Actions Overlay v0.9.19

- Add Photo Mode â†’ Filters & effects: six color presets, strength, exposure, contrast, saturation, warmth, vignette, stable film grain and cinematic bars; neutral defaults and Reset effects.
- Add an optional photo watermark using the PORTHEON player name, with corner, opacity and size controls. Show the detected name before enabling; never fall back to a Windows login. Custom text can be supplied in the overlay INI.
- Preview effects live on the GPU and preserve them in both screenshot modes, while the tools retain their normal colors. Restore the normal image on Photo Mode exit/focus loss and remember the selected settings for the next session.

Validation: native settings/navigation tests, full controller/UI regression and real-GPU pixel tests on BGRA/RGBA linear and sRGB formats. In-game visual/performance verification is pending.

# UFC 6 Actions Overlay v0.9.18

- Add Photo Mode with controller/keyboard free-camera movement, look, field-of-view zoom, reset and restoration on exit or focus loss. Open it from the main menu or F9 during a running fight/practice.
- Add reversible action freeze, full game-HUD hiding, and Keep cage visible to prevent missing panels in free-camera views. Expose the cage toggle as C / D-pad Right.
- Add a translucent controls panel below the fighter HUD, with matching Xbox/PlayStation artwork and live freeze, HUD, cage and FOV information. Tab / D-pad Down hides it. Process raster/conversion work in cached RAM before a bulk GPU upload to avoid the measured uncached-memory FPS drop.
- Add Screenshot + overlay with Ctrl+Shift+F12, a separate configurable Keybinds action, a Photo Mode menu row and D-pad Left in Photo Mode. Preserve clean screenshots and Windows Pictures\Screenshots storage. Include the same-frame raster with matching alpha/color-space handling; keep startup-recognition frames clean.
- Broaden renderer startup discovery with legacy and additive layer paths while preserving inherited path entries and requested layers. Record startup elevation/API information and filtered, path-redacted loader messages in normal support reports. Prevent invisible menu openings from retaining game input. The reported remote invisible-overlay case is not yet confirmed fixed on the affected machine.
- Clarify that setup targets PORTHEON\games\UFC6 or its bin folder, rather than the extracted game-data folder. Keep normal and supported Parsec mode in one installation.
- Refresh screenshots, Photo Mode documentation and the roadmap.

Validated live in Parsec mode: camera/FOV/freeze controls, HUD and cage restoration, smooth controls-panel rendering, cage toggle and both screenshot modes. Native, controller/UI, startup-security, installer rollback/privacy/update and real-GPU tests passed; GPU coverage includes BGRA/RGBA linear and sRGB output. Installer metadata recognizes the affected user's v0.9.15 components for upgrade.

# UFC 6 Actions Overlay v0.9.15

- Add Skip intros under Game & launcher. Apply the verified short-boot route and its supporting movie/legal callbacks early at launch, removing both Zuffa and EA SPORTS splashes. Turning it Off restores all owned callback defaults. Ignore the obsolete standalone shortening preference.
- Add Auto-continue startup for the English Press any button title screen and known PSN/EA and roster-download warnings. Confirm each once during the first five minutes, checking every 100 ms while the game is focused and the overlay is closed. Stop after both warnings.
- Switching Skip intros On also enables Auto-continue startup. Allow auto-continue to be turned Off independently afterward and remember that override across launches. Switching Skip intros Off also disables auto-continue. Both options default Off.
- Recognize the 16:9 startup layouts at 720p or higher. Require a valid primary controller for network warnings; the title screen may use a targeted game-window Enter key when a controller is not assigned. Keep normal offline failure results; do not report fake login/download success.
- Process recognition frames in memory, without saved probe images or uploads. Keep clean screenshot requests independent.

Both splashes were confirmed completely absent in a live Parsec launch, with hands-free progression through the title and both warnings to the main menu. Native callback/input guards, rendering/persistence, normal/Parsec installer checks, GPU readback/screenshot checks and 29 Python tests passed during development. The final two-switch UI and saved independent override passed targeted state and renderer/reload checks.

# UFC 6 Actions Overlay v0.9.14

- Add configurable keyboard shortcuts and controller combinations for screenshots, menus, save backups and game/PC actions. Assigned shortcuts appear beside actions; F10 remains available. Exit and power actions retain their confirmations.
- Add corner notifications for screenshots and action feedback without taking controller focus. Clean game screenshots exclude the overlay and notifications.
- Organize Settings into Appearance, Controls, Game & launcher, and Files & support.
- Add theme presets and a live-reloaded `ufc6-overlay-theme.cfg` for launcher colors. Seed UI scale, font size, box style and window size from config on first use; preserve later user choices and launcher configuration during updates.
- Keep menu fonts consistent and offer Rounded, Square, Outline, Pill and Octagon box styles.
- Add Compact, Expanded and Tall window sizes independently of text/UI scale. Tall shows all eleven Cheats rows together while retaining viewport fitting and matching input hit areas.
- Replace simplified controller drawings with embedded Kenney Xbox/PlayStation vector artwork in shortcut badges and footer controls, including shoulder, pressed-stick, D-pad and PS5 Options buttons. Include the CC0 license and source artwork.
- Refresh menu screenshots and documentation.

Validated keyboard/controller screenshot shortcuts, themes, notifications, box styles, Tall Cheats layout and both controller icon profiles in the live game. All 28 artwork assets passed rendering checks; native input/configuration/persistence, installer/diagnostics, safe handoff and 29 Python tests passed. Vulkan GPU checks covered tall menus, viewport scaling and clean screenshots excluding notifications.

# UFC 6 Actions Overlay v0.9.11

- Match Cheats row labels to the main menu font size and rename the longevity switch to Never get old.

- Remember the longevity On/Off preference across launches.
- Keep the saved preference separate from the active career binding; reactivate only after validating fresh career data.
- Save Off immediately and clear the remembered choice on Reset.

- Add training skill-point reward choices: 1x (Off), 2x, 5x and 10x, saved across launches. Scale verified training objective rewards from their original amounts without compounding; restore originals at 1x or Reset.
- Add a confirmed All sparring grades: A action for the selected career's sparring challenge packs, including unattempted packs. Leave station locks, training levels and skill-point balances unchanged.

Fresh-process preference reload, reset, deferred career binding, reward scaling/restoration, packed-neighbor preservation and grade eligibility tests passed. Installer, diagnostics, controller/rendering and safe handoff checks passed. Longevity preference reload and A-grade simulation unlock are confirmed in the live career. Validated live at 2x in Parsec heavy-bag training: three 2-point challenges became 4 points each, paid +12, and raised the career balance from 46 to 58.

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
- Updates the installer, updater, GitHub branding, and editable vector artwork. Windows ICO sizes cover 16â€“256 px.

Icon-only release. Installer and updater validation checks passed; game behavior is unchanged.

# UFC 6 Actions Overlay v0.9.5

- Adds **Settings > Check for updates...** and an installer update button. The desktop checker finds the latest stable release on GitHub, offers newer versions, verifies the installer against release SHA-256 checksums (and the GitHub asset digest when provided), then opens setup with your game folder filled in. Downloads and installation require user actions; there are no silent startup updates.
- Adds an original U6 icon to the installer and updater, with 16â€“256 px ICO sizes and editable SVG artwork.
- Puts simple installation steps at the top of the README and clarifies that one install supports normal UFC 6 and the verified Parsec engine without extra overlay steps.
- Smoother text, button symbols, switches and outlines with 2x supersampling at the selected UI scale. Unchanged frames reuse a cached image to keep the extra rendering work off normal frames. Settings scroll markers are drawn as shapes, and text bounds are checked from 50% to 300%.
- **Cheats > Savegame backups...** creates separate, timestamped ZIPs in **Documents / UFC6 Actions Overlay / SaveBackups**, with an action to open the folder. Save in the game and wait for autosaving to finish first; unsaved progress is not included.
- Backups include default-user and guest-user UFC 6 saves, preserve their relative `_SaveData` paths, and contain a SHA-256 manifest. Files and archive contents are verified; detected source changes or failures discard the incomplete ZIP while leaving saves and older backups untouched.
- Backup work runs outside the game-rendering thread using the bundled helper. No separate Python installation is needed, and removing the overlay keeps backups.

Validated with native controller/cheat/power-stub checks, renderer/text bounds at 50â€“300%, backup round trips and failure cases, two consecutive launches of the packaged helper, a verified backup of the user's 25 save files, installer upgrade/removal/rollback tests, normal/Parsec shared-install coverage, updater rejection fixtures, and an actual verified GitHub download without launching it.

# UFC 6 Actions Overlay v0.9.3

Adds Player 1 cheats, career additions, integrated controller-icon patching, and refreshed controls.

- **Cheats** is on the main menu. Infinite stamina and No damage use labeled On/Off pill switches and follow Player 1's controller assignment in normal and Parsec mode. Choices can be set before a fight and remain session-only.
- **Career cheats** adds selectable amounts of money or skill points to the loaded offline career, once per confirmation. Current balances are shown. Keep a save backup before experimenting; changes use the game's normal saving.
- **The [UFC 6 Xbox Icons mod](https://github.com/thyaguster/ufc6-xbox-icons) is already included; no separate download or installation is needed.** Open **Settings > Patch in-game controller icons...**, choose Xbox or PlayStation, then confirm **Apply and restart game**. This changes the game artwork separately from **Overlay button icons**. It checks the supported game build and all edited ranges, keeps a verified local restore backup, then applies after normal game/launcher shutdown and relaunches with saved settings. No game artwork is bundled.
- Fixes repeated icon switches after restart by isolating each packaged helper's Python runtime and clearing its temporary state before launching the game.
- Refreshes the UI with pill switches, compact value selectors, rounded rows, subtle focus outlines, and distinct quit/power styling. Fixes clipped text at different UI scales and uses the same two-pixel row gaps on Main, Settings, and Cheats.
- Restart PC's auto-launch indication is now inline text and a status dot, keeping it clear of the row borders. The confirmation also states whether auto-launch is enabled and where to change it.
- Adds updated screenshots, including Cheats and the controller-icon patch page.

Validation: controller ownership and Parsec regressions; native cheat fixtures; confirmed in-game stamina independence and career additions; font bounds at 50â€“300% scaling; game-icons validation/restore/rollback fixtures; a second packaged-helper launch after temporary-runtime cleanup; installer upgrade/removal/rollback checks. PC power tests use stubs and never power off or reboot the test machine.

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

- **UFC6-Actions-Overlay-Setup-0.8.4.exe** â€” recommended. One file with all overlay components built in. No Python or compiler required.
- **UFC6-Actions-Overlay-0.8.4.zip** â€” setup EXE, prebuilt mod files, complete project source, build scripts, tests, and licenses.
- **Overlay-Screenshots-0.8.1.zip** â€” gallery from v0.8.1, including the main menu and Settings over the game menu. The current Settings preview is in the README.
- **SHA256SUMS.txt** â€” checksums for these download files.

## Install

Close UFC 6 and its launcher. Open Setup, choose the UFC 6 game folder or the `bin` folder containing `UFC 6.exe`, and select **Install / Update**. Launch the game normally. Hold **LB + RB + Menu** for about one second, then release, or press **F10**.

Use the same setup's **Remove overlay** button to uninstall. Settings and screenshots are kept. Unknown mods are not overwritten; recognized older files are backed up, and failed changes roll back.

## Features

- Resume, take a clean game screenshot, restart the game, or quit normally.
- Separate Settings page with Xbox/PlayStation prompts, a configurable controller shortcut, and 50â€“300% UI scaling.
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
