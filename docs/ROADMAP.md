# Roadmap

These are investigation targets, not promises of released behavior.

## Replay follow-ups

- **Mid-fight instant replay:** investigate the hidden native pause-menu route and safe restoration of the active fight. Not included in v0.9.36.
- **Brief replay flashes:** investigate camera-cut flashes during playing or paused replay. The long paused black screen has been fixed; brief flashes remain separate.
- **Longer recording coverage:** test continuous playback across additional rounds and longer fights. Available recording is limited to retained continuous footage; do not promise whole-fight capture.
- **Draw testing (lower priority, deferred):** verify unanimous/split draws, including no selected highlights with valid retained clips, repeated entry/exit and rematch cleanup. Decision cases are already verified and this is not a release blocker.
- **Normal-mode live replay validation:** the new controls are live-tested in Parsec mode; broaden live coverage to normal mode.

## Other candidates

- Implement performance options directly in the overlay instead of relying on PORTHEON settings, with live application and restoration where feasible.
- Follow up on the affected user's career-code/binding report. Running PORTHEON without administrator rights resolved that user's invisible-overlay issue; this is a client workaround, not a universal diagnosis.
- Broaden visual startup fallback language/layout coverage. The native title/network-warning bypass does not depend on visual detection.
- Named appearance/keybind presets and import/export.
- Optional automatic save backups before career-changing actions, with retention controls.
- Further inventory/progression unlock investigation; existing PORTHEON fighter/skin unlocks remain launcher features.

## Included in v0.9.36

Photo Mode, HUD/cage controls, filters, styled watermarks, optional overlay-inclusive screenshots, performance settings, native startup bypass, replay pause/steps/speed/available-recording controls, decision replay fallback, Next hit KO and background muting. Decision repeat entry and rematch cleanup are confirmed. See the release notes and Photo Mode guide for limits and controls.
