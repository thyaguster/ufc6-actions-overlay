# Fight report export

The read-only exporter captures the finished fight's result and Overall Fight
Statistics into a JSON file for manual Parsectagon import. It does not submit
the result or change Elo. Both lobby fighters still need to report the result.

On a finished fight's **Fight Statistics** screen, open **F10 → Parsectagon → Export fight report**. The overlay saves a JSON file in `Documents/Parsectagon/Fight Reports` and shows the result in its status line. Sign in on the Parsectagon website, open the correct match in **My fighter**, and choose **Attach game capture JSON**.

The export only reads game data and writes a local file. It does not submit a result or award ranked points; both players still report the match. Fighter names remain empty in the native export until their in-game fields are verified.

The standalone diagnostic exporter remains available from this source folder:

```powershell
python scripts\fight_report_export.py --red-name "Red fighter" --blue-name "Blue fighter" --game-version 01.006.000
```

The command prints the saved file path and accepts optional fighter-name labels.

The exporter was checked against an actual round-two submission: winner,
round, finish time, knockdowns, significant and total strikes, takedowns, and
control time matched the game screen. Other finish method codes and draws
still need a separate validation case, and Original/Parsec has not yet been
checked live. The exporter refuses results it cannot verify instead of guessing.
