# Fight report export

The read-only exporter captures the finished fight's result and both fighters'
detailed Overall statistics into a JSON file for manual Parsectagon import.
When the game's separate complete Fight Statistics response is retained, it
also includes cumulative round records and judges' scores. The file marks
whether those round details were available. It does not submit the result or
change Elo. Both lobby fighters still need to report the result.

On a finished fight's **Fight Statistics** screen, open **F10 → Parsectagon → Export fight report**. The overlay scans the game's read-only records in a separate helper process, then saves a JSON file in `Documents/Parsectagon/Fight Reports` and shows whether complete or overall-only data was saved. Sign in on the Parsectagon website, open the correct match in **My fighter**, and choose **Attach game capture JSON**.

The export only reads game data and writes a local file. It does not submit a result or award ranked points; both players still report the match. Fighter names and game IDs are included when the matching fighter-profile response remains available; they do not identify Parsectagon accounts. The Parsectagon source validator accepts the schema-1 result and retains the extra `fightStatistics` field; a running server must be restarted to load that change.

`clockSeconds` is elapsed time in the finish round. UFC 6 displays time remaining;
the exporter converts it before saving. Reports exported by v0.9.64 or earlier
used the remaining value and should be exported again after updating.

The standalone complete-report exporter remains available from this source folder:

```powershell
python scripts\export_complete_fight_report.py --red-name "Red fighter" --blue-name "Blue fighter" --game-version 01.006.000
```

The command prints the saved file path and accepts optional fighter-name labels.

The exporter was checked against actual Round 1 and Round 2 finishes. It also
matched the active fighter-profile names and IDs in the Round 1 test. The raw
game finish-method code is retained; code 1 appeared as TKO in one test, but
unverified codes remain unlabeled. Draws and Original/Parsec still need live
checks. The exporter refuses results it cannot verify instead of guessing.

## Complete report capture

When present, the `fightStatistics` section contains the game's original `TOTAL` and
`ROUNDS` records for both corners, including all available game-supplied
stat fields and `JUDGES_SCORES`. The round arrays are cumulative for many
stats. Game-supplied field names are preserved exactly; they have not all
been mapped to the visible labels. Fighter names come from a separate profile
response when available; lobby player identities remain unresolved. Parsectagon's source validator now preserves the complete
section on import, but the website does not yet display or use those extra
fields for ranked decisions.

UFC 6's `FightStatsHelper` receives `RED_CORNER_FIGHTER` and
`BLUE_CORNER_FIGHTER`, each with `TOTAL` and cumulative `ROUNDS` arrays. The
game displays a round by subtracting the previous cumulative round. It reads
judges' totals from `TOTAL.JUDGES_SCORES`. The post-fight banner obtains
fighter names from a separate fighter-by-ID service. Changing the visible
Round 1/Round 2 tab does not change the named counters read by the current
exporter; those counters remain fight totals.

`scripts/fight_stats_probe.py` is an exact-byte, reversible diagnostic for
checking access to the complete stats response. Its original archive bytes
are backed up in `.build/fight-stats-probe-backup.bin`. Three probes in the
base `FightStatsHelper` failed to run in a completed exhibition fight: no
engine print, no file, and no visible `REPORT PROBE` tab label. PORTHEON did
not rewrite the patched archive during launch, and the original bytes have
been restored. The active Fight Statistics screen therefore appears to use a
different helper or a compiled/overriding asset. The file probe tells us
nothing about Lua file-I/O availability because the helper did not execute.
Until the active data path is verified, per-round and judge data must not be
inferred from Overall counters. A read-only binding scan found live Overall
counters and several judge bindings, but no simple named bindings for the
round tabs. `scripts/fight_round_trace.py` records low-rate counters for
comparison with a completed fight.

The current export bindings have confusing game-supplied names. Read-only
memory scans found complete serialized `get-fight-stats` responses for both
Round 1 and Round 2 finishes. Their first fields differ, so the exporter checks
the response structure and current-fight totals instead of requiring one field
order. Both have `TOTAL`, cumulative `ROUNDS`, and `JUDGES_SCORES`. The response
matches the game's named Overall bindings, but its strike-field labels do not
follow the usual attempted/landed wording.

The complete response is intermittent: a later Round 1 fight showed the
Statistics page but no complete response remained in process memory, including
after reopening the separate Statistics menu. A retained match-result response
still held 120+ raw Overall fields per fighter. v0.9.70 exports that record as
`overallStatistics` with `fightStatisticsComplete: false`; it does not infer
missing round or judge data. The in-game footer reports this limitation. The
v0.9.69 build was held back after a slow launch; v0.9.70 uses the v0.9.68
menu worker with a distinct helper response for the overall-only result. The
optional full-response search is capped at 15 seconds so an absent record does
not hold the menu on “Capturing” for nearly a minute. A live v0.9.70 test saved
an overall-only report and displayed that limitation in the menu. A full
response appearing outside this search window can be missed; that case remains
to be measured against another fight that exposes its round records.

The separate v0.9.66 post-fight EA progression notice fix is confirmed in a
normal PORTHEON launch: the notice stayed absent, and the hook recorded one
suppression. This does not change the Fight Statistics extraction work.

## Fight Now callback investigation

The base game asset for `ArcadeHelper` contains the normal Fight Now post-fight
route. `ArcadeHelper:getData()` requests `get-fight-results`,
`fight-now:fighters:get-current`, `get-fight-stats`, and rivalry data as one
batch. Its `_handleFightData` callback passes the complete `statsOutput` to
`ResultHelper:getArcadeResultData()`, which builds the visible `statisticsList`
with `PostFightStatsHelper:getStatsList()`. This is an earlier, specific capture
point for round statistics than a later search of process memory.

`scripts/arcade_stats_probe.py` prepares an exact-byte diagnostic at that
callback in the base `cas_06.cas` asset. It attempts to log the game's original
`statsOutput` table between `UFC6_ARCADE_STATS_BEGIN` and
`UFC6_ARCADE_STATS_END`. The patch retains the asset's exact byte length,
removes only two Lua comments to make room, verifies the original hash, and
backs up/restores the original span. A live PS5 Pro Fight Now test loaded this
exact patch and finished a fight, but neither the Windows debug listener nor
PORTHEON's session log received either marker. This cannot distinguish an
unused base script from `print`/`DebugPrint` output being unavailable in this
build. The diagnostic was restored and verified afterward, and the game was
relaunched through PORTHEON with the original archive. Do not repeat this
probe as evidence of a working capture channel; establish a reliable output
path or confirm the active script asset before another fight test.

The overlay's current native report bridge (`src/fight_report_export.c`) reads
validated named guest bindings for the result and Overall counters. The
separate Python helper reads a retained serialized service response when one
is still present. Neither intercepts the `get-fight-stats` callback, so neither
can guarantee per-round capture. The retained match-result response does
contain `JUDGES_SCORES` arrays and many raw Overall fields, but it has no
round-indexed records. The capture below avoids the intermittent response
search by retaining the complete table at the verified post-fight UI handoff.

### Post-fight UI capture

`scripts/postfight_stats_capture_probe.py` prepares a reversible, exact-size
patch at `PostFightStatsHelper:getStatsList`, the function receiving the
complete `TOTAL` and cumulative `ROUNDS` tables drawn in Fight Statistics.
It retains a tagged copy of those fields and judge arrays on the Lua module
without changing the visible stats. `scripts/read_postfight_capture.py`
decodes that copy, checks it against the live finished fight, and saves a
complete report. The exact-size patch passed archive verification. Round-trip
checks against previously saved one- and two-round game responses preserved
all fields. A live one-round Fight Statistics test saved a 20 KB report with
121 red and 122 blue raw fields per round and both judges arrays. The game
archive was restored byte-for-byte afterward. The rebuilt local helper tries
this capture first and retains the older Overall fallback. A second live
one-round test using **F10 → Parsectagon → Export fight report** saved
`fight-complete-4b54a823.json` with `fightStatisticsComplete: true`, both
fighter names, 121/120 raw round fields, judges arrays, and the Knockout
result. UFC 6 remained running. The capture patch and rebuilt helper are
installed locally in the PS5 Pro engine; this is not yet installer-managed or
published. The patch is gated to UFC 6 01.006.000 and must be restored before
applying a different game version or removing the overlay.

### Automatic local capture (local test build)

The first automatic build polled the finished-match state and launched the
complete-report helper. A live test showed why this is insufficient for a
ranked result: the helper retried while the player stayed on the initial
post-fight page, then succeeded only after Fight Statistics opened. The
measured delay was 112,453 ms from first detecting the finish; the successful
helper run took 8,031 ms. That delay included time spent waiting for the
player to open Statistics.

The current local build uses a two-stage pipeline. It checks the verified
match-state binding every 100 ms. On a finished fight, a native worker reads
only seven result bindings, validates two snapshots 10 ms apart, and flushes
`fight-result-<id>.json` with winner, finish method, round and clock. This
result file is explicitly marked `reportStage: result` and
`fightStatisticsComplete: false`. The worker then starts the full-statistics
helper; it accepts only the complete post-fight UI capture, retries after 15
seconds if that table is not ready, and links the complete report to the first
record by ID when both results match. A new fight rearms the pipeline.

The game log records `native_elapsed_ms` and
`finish_observed_to_result_ms` for the first stage, and
`helper_elapsed_ms` for the full stage. In the live test, the result file
was flushed **31 ms after the overlay observed the finish**. The complete
report followed at **31,641 ms** because Fight Statistics became available
later. Both files have the same fight ID. The first timestamp begins when
the overlay observes the finished state, within about 100 ms of the actual
transition; it is not a server timestamp. The helper now saves the complete
report beside the preliminary result file, including when PORTHEON redirects
Windows Documents. The result-first build is installed locally, but the
archive capture remains a local diagnostic rather than an installer-managed
release. A subsequent local build adds automatic HTTP upload using a scoped
Parsectagon key; see [LIVE-FIGHT-UPLOAD.md](LIVE-FIGHT-UPLOAD.md). That build
was installed locally in the PS5 Pro engine on 2026-11-16. Neither
capture nor upload prevents local file tampering. Parsectagon must corroborate
both players and apply server-side disconnect rules before using reports for rank.
