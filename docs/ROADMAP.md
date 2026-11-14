# Roadmap

These are investigation targets, not promises of released behavior.

## Replay follow-ups

- **Native Instant Replay at results (available; longer-fight coverage pending):** with Experimental → Instant Replay in fights On, the game's post-fight menu already shows both Instant Replay and View Replay. In a short Parsec fight, the user opened the native seek-bar viewer, moved to its start, played and exited. A read-only trace found its clip bounds matched the full retained 16.94-second recording; View Replay had selected a roughly two-second finish clip. Earlier tests showed about 4m49s retained across two rounds with some opening footage trimmed. Test longer fights, repeat entry and normal mode before advertising whole-fight playback; retained recording can be incomplete.
- **Review View Replay fallback:** after confirming post-fight Instant Replay on decisions, draws and longer fights, decide whether to retire the optional Replay after decisions clip-slot fallback and the View Replay-only Available recording route. Keep the native replay pause/step/speed and Photo Mode controls unless their Instant Replay equivalents cover them reliably. Compare retention with and without the auxiliary recorder budget before removing that hook.
- **Instant Replay follow-up:** v0.9.42 fixed the intermittent translucent panel after F9 in a live Parsec fight; repeated quick exits from Replay page 4 cleared it every time without B/Circle. Check normal mode and a relaunch with Instant Replay already On separately. The Replay page still becomes unavailable at the end of the native seek bar; rewinding slightly before Photo Mode makes it available.
- **Brief replay flashes:** investigate camera-cut flashes during playing or paused replay. The long paused black screen has been fixed; brief flashes remain separate.
- **Longer recording coverage:** test continuous playback across additional rounds and longer fights. Available recording is limited to retained continuous footage; do not promise whole-fight capture.
- **Draw testing (lower priority, deferred):** verify unanimous/split draws, including no selected highlights with valid retained clips, repeated entry/exit and rematch cleanup. Decision cases are already verified and this is not a release blocker.
- **Normal-mode live replay validation:** the new controls are live-tested in Parsec mode; broaden live coverage to normal mode.

## Other candidates

- Verify the PS5 Pro / DLSS engine's career bindings in a loaded career before advertising career cheats as live-tested on that host. The checked guest-code signatures match; the save schema still needs a real career check.
- Implement performance options directly in the overlay instead of relying on PORTHEON settings, with live application and restoration where feasible.
- Follow up on the affected user's career-code/binding report. Running PORTHEON without administrator rights resolved that user's invisible-overlay issue; this is a client workaround, not a universal diagnosis.
- Broaden visual startup fallback language/layout coverage. The native title/network-warning bypass does not depend on visual detection.
- Named appearance/keybind presets and import/export.
- Optional automatic save backups before career-changing actions, with retention controls.
- Further inventory/progression unlock investigation; existing PORTHEON fighter/skin unlocks remain launcher features.

## Included in v0.9.36

Photo Mode, HUD/cage controls, filters, styled watermarks, optional overlay-inclusive screenshots, performance settings, native startup bypass, replay pause/steps/speed/available-recording controls, decision replay fallback, Next hit KO and background muting. Decision repeat entry and rematch cleanup are confirmed. See the release notes and Photo Mode guide for limits and controls.
