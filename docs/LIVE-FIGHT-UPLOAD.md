# Automatic Parsectagon fight reports

The overlay records a finished fight locally in two stages:

1. `fight-result-<id>.json` contains the winner, method, round and clock. In the measured PS5 Pro test it was written 31 ms after the overlay observed the finish.
2. `fight-complete-<id>.json` adds names, Overall and per-round statistics, and judges' data. It uses the same fight ID. In that test it was written 31.641 seconds later, after Fight Statistics became available.

Both files are saved in `Documents\Parsectagon\Fight Reports`. The `reportStage`, `fightStatisticsComplete`, and `id` fields tell the lobby which stage it received. The complete record upgrades the earlier result; it must not create a second match result.

For automatic upload, generate a scoped overlay upload key in Parsectagon's **My profile** page. On this PC, run `ufc6_menu_icons.exe --link-lobby`, then paste the key into the local dialog. The helper stores it in `%USERPROFILE%\.parsectagon-overlay-token`. Alternatively set `PARSECTAGON_OVERLAY_TOKEN` in the launch environment. Never share the key or place it in the report JSON.

The overlay uses `[Menu] LobbyServerURL` in `ufc6-quit-menu.ini` (default `http://127.0.0.1:3000`). Each stage is uploaded by a child helper to `POST /api/bridge/game-capture` with `Authorization: Bearer <key>`. HTTP work never runs on the Vulkan render thread. A successful upload creates `<report>.upload.json`; a failed attempt creates `<report>.upload-error.txt`. The local JSON remains available even when the lobby is offline or no key is configured.

The lobby may associate a capture with the signed-in fighter's active match. Captures are evidence only: the game cannot prove which lobby account controlled either corner. Uploading a report does not submit the result or change rank; both players' usual result agreement remains required.

The complete-statistics capture currently relies on a locally tested, version-specific game archive patch. It is not yet part of the public installer. The immediate result capture and network uploader are separate from that archive patch. The uploader build was installed locally in the PS5 Pro engine on 2026-11-16 and the game relaunched through PORTHEON; account linking and a running lobby server are still required for network sending.
