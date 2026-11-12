# Photo Mode

Photo Mode is available in the overlay's main menu and through F9 in a running fight or practice. It is enabled only for the verified engine and matching game-code signatures. Unsupported code leaves the normal game camera intact.

## Controls and tools

Use the sticks to move/look, bumpers for height and triggers for field of view. A/Cross freezes, X/Square takes a clean screenshot, Y/Triangle resets and B/Circle exits. D-pad Up toggles the game HUD, Right toggles native cage fading versus a fully visible cage, Down toggles the controls panel and Left takes an overlay-inclusive screenshot. Controller camera controls apply with the tools menu closed. F10 opens the tools.

Keyboard equivalents are I/J/K/L, arrow keys, U/O, Page Up/Page Down, P, Home, H, C, Tab and Esc. Hold Shift for faster movement. Ctrl+Shift+F12 captures the overlay by default. Photo Mode and screenshot action keys can be changed in Keybinds; existing assignments are preserved.

![Photo Mode options](screenshots/photo-mode-options.png)

## Screenshots

Take screenshot preserves the existing clean capture. Screenshot + overlay includes the exact same-frame overlay raster, with matching alpha and color-space handling. Startup recognition always sees the clean frame first. Both actions save PNGs in Windows Pictures\Screenshots, with independent keyboard/controller bindings. The overlay-inclusive option can capture either the translucent controls panel or the full tools menu. Capture feedback appears after the requested image has been taken.

![Controls panel in a fight](screenshots/photo-controls-in-game.png)

## Restoration and limitations

- Exiting or switching away from the game restores normal camera, field of view, game speed and HUD behavior. The original native cage-fade values continue to update and are restored when the override ends.
- Opening the tools stops camera movement and temporarily shows the game HUD. Closing the tools returns to the selected HUD setting.
- Freezing also stops the game's cage-update graph. Briefly unfreeze after changing the cage option to process the change.
- The controls panel uses the current theme, controller artwork, font and scale. It can be hidden independently without leaving Photo Mode.
- Photo Mode is intended for a running fight or practice. It does not add an instant-replay editor or suppress the game's real network/offline state.

## Validation

Live testing confirmed camera movement/look, field-of-view zoom, freezing while the camera moves, HUD hiding, cage visibility, controller navigation, smooth panel rendering and exit restoration. The shipped screenshots were captured through the installed overlay. Native tests cover identity and ownership guards, current-state restoration, page-memory checks, screenshot modes and input routing. GPU tests cover BGRA/RGBA linear and sRGB formats, alpha composition and clean startup-recognition frames.
