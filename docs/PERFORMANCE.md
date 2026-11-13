# Performance settings

Open **Settings → Performance**. These controls edit PORTHEON's existing UFC 6 launch configuration and take effect after a game restart. Normal and Parsec mode use the same saved choices.

| Control | Effect |
| --- | --- |
| Crowd & animation | Off skips crowd rendering jobs and crowd animation. |
| Particles & sparks | Off disables particle emitters, including hit sweat, sparks, smoke and fireworks. |
| Depth of field | Off disables the game's depth-of-field blur. |
| Dynamic resolution | On lets the game vary resolution and returns render scale to Launcher default. |
| 3D render scale | Launcher default or supported fixed percentages. Choosing a fixed scale turns dynamic resolution Off. This affects the game image, not overlay size. |

**Save for next launch** stores the choices without closing the current session. **Save & restart game** opens a Cancel-first confirmation, saves the choices and uses the configured PORTHEON restart flow. Save game progress before confirming a restart. Back asks before discarding unsaved edits.

The page reads the values currently saved in `UFC6.ini` beside PORTHEON's game launcher. They are next-launch settings, not a live GPU-state report. PORTHEON still verifies patch signatures and applies its VRAM limits and automatic repair rules at startup. Fixed percentages use PORTHEON's game rendering scale; 100% is the native game setting, not a promise that 3D rendering equals desktop resolution.

Only the corresponding performance keys are updated; game path, player name, normal/Parsec mode, existing fixes and unrelated settings are preserved. Writes use a temporary copy and atomic replacement with source-change checks. If PORTHEON changes these performance fields while the page is open, the overlay refuses the conflicting save; discard/reopen the page to reload.

The controls require the PORTHEON launcher/settings layout. A different custom launcher is left untouched and the page reports the controls as unavailable. No game assets are deleted, and this page does not install new live rendering hooks.
