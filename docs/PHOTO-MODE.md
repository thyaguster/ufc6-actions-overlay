# Photo Mode

> Current v0.9.26 development preview. These screenshots are newer than the published v0.9.18 binary release. [Complete screenshot gallery](SCREENSHOTS.md).

Photo Mode is available in the overlay's main menu and through F9 in a running fight or practice. It is enabled only for the verified engine and matching game-code signatures. Unsupported code leaves the normal game camera intact.

## Controls and tools

Use the sticks to move/look, bumpers for height and triggers for field of view. A/Cross freezes, X/Square takes a screenshot, Y/Triangle resets and B/Circle exits. D-pad Up toggles the game HUD, Right toggles native cage fading versus a fully visible cage, Down toggles the controls panel and Left has no built-in action. Controller camera controls apply with the tools menu closed. F10 opens the tools.

Keyboard equivalents are I/J/K/L, arrow keys, U/O, Page Up/Page Down, P, Home, H, C, Tab and Esc. Hold Shift for faster movement. Overlay-inclusive capture is an opt-in setting, with no separate shortcut. Photo Mode and screenshot action keys can be changed in Keybinds; existing assignments are preserved.

![Photo Mode options](screenshots/photo-mode-options.png)

## Screenshots

**Take screenshot** is clean by default. To photograph the overlay itself, enable **Settings → Files & support → Include overlay in screenshots**. The normal screenshot action/button/key then includes the displayed controls panel or tools menu. Switch it Off to return to clean captures. There is no separate overlay-shot action, default keyboard/controller shortcut or Camera-panel shortcut hint. Existing normal screenshot bindings are preserved; retired `ScreenshotOverlayKey/Pad` entries are ignored. PNGs save in Windows Pictures\Screenshots. Startup recognition always receives the clean pre-overlay game frame; capture feedback appears after the requested image has been taken.

![Controls panel in a fight](screenshots/photo-controls-in-game.png)

## Translucent editor pages

While Photo Mode is active, press **Menu (Xbox) / Options (PlayStation)** or **F8** to cycle **Camera → Effects → Watermark** without opening the full tools. The panel appears if hidden. The Camera page keeps the original photo controls. On the Effects/Watermark pages, use **D-pad Up/Down** to select a row and **Left/Right** to adjust; **A/Cross** also advances the selected value. Keyboard equivalents are **W/S** and **A/D**. The sticks, bumpers and triggers still move/look/zoom the camera. **B/Circle** returns to the Camera page; a second press exits. **Esc** exits directly. **P** still freezes/unfreezes. **X/Square** takes a screenshot on every panel page and follows the optional Include overlay in screenshots setting. Tab hides/shows the panel. While the panel is hidden, controller buttons use the original Camera mapping. Assigned user keybinds retain priority.

## Filters, effects and watermark

Open **Photo Mode → Filters & effects**. Use left/right or confirm to change a value. Presets include Vivid, Cinematic, Warm, Cool, Monochrome and Sepia, with adjustable filter strength. Exposure, contrast, saturation, warmth, vignette, film grain and 2.35:1 / 2.76:1 cinematic bars are independent. **Reset effects** returns the image to neutral without changing your watermark. Settings are saved between launches but only applied while Photo Mode is active. Exit or switch away to restore the normal game image.

**Photo Mode → Watermark** adds your PORTHEON player name, with four corner positions, opacity and Small/Medium/Large sizes. Choose **Clean**, **Glass**, **Editorial** or **Signature** styling and **Modern**, **Condensed**, **Serif** or **Mono** fonts. Lettering is cached at double resolution and filtered on the GPU for sharper scaling. It starts Off; the detected name is shown before enabling it. The overlay reads only `[UFC6] user` from the launcher's `UFC6.ini`, never your Windows login. If no player name is found, it leaves the watermark Off. An optional `[Photo] WatermarkText=Your name` entry in `bin/ufc6-quit-menu.ini` supplies custom text (restart to reload); other launchers can use this too.

Screenshots retain the photo effects and watermark in both capture modes. With Include overlay in screenshots Off, tools/panel are excluded; On adds them in their original colors. Photo grading runs on the GPU, without continuous CPU frame readback. Film grain is spatially stable while the action is frozen. Cinematic bars cover the frame without cropping the saved resolution. These are image effects; they do not add depth-based focus or lens simulation.

Shader sources are in `src/shaders/photo_effects.*`, with committed SPIR-V for normal builds. To regenerate, run `python scripts/generate_photo_effects_shaders.py --compiler path/to/glslang.exe` using [Khronos glslang](https://github.com/KhronosGroup/glslang).

## Restoration and limitations

- Exiting or switching away from the game restores normal camera, field of view, game speed and HUD behavior. The original native cage-fade values continue to update and are restored when the override ends.
- Opening the tools stops camera movement and preserves your selected HUD visibility. It does not automatically show/hide the game HUD when entering or leaving the tools; use the explicit HUD switch, or exit Photo Mode to restore normal behavior.
- Freezing also stops the game's cage-update graph. Briefly unfreeze after changing the cage option to process the change.
- The controls panel uses the current theme, controller artwork, font and scale. It can be hidden independently without leaving Photo Mode.
- Photo Mode is intended for a running fight or practice. It does not add an instant-replay editor or suppress the game's real network/offline state.

## Validation

Live testing confirmed camera movement/look, field-of-view zoom, freezing while the camera moves, HUD hiding, cage visibility, controller navigation, smooth panel rendering and exit restoration. The shipped screenshots were captured through the installed overlay. Native tests cover identity and ownership guards, current-state restoration, page-memory checks, screenshot modes and input routing. GPU tests cover BGRA/RGBA linear and sRGB formats, alpha composition and clean startup-recognition frames.
