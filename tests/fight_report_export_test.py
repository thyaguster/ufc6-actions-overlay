"""Regression check against the captured submission Fight Statistics screen."""
import argparse
import importlib.util
from pathlib import Path

source = Path(__file__).resolve().parents[1] / "scripts" / "fight_report_export.py"
spec = importlib.util.spec_from_file_location("fight_report_export", source)
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)

fields = {
    "Global_MatchState": (3, 0),
    "Global_RoundNum": (1, 0),
    "Global_RoundLength": (1133903872, 0),
    "Global_MatchResultType": (2, 0),
    "Online_MatchResultType": (2, 7),
    "Online_MatchResult": (2, 1),
    "Online_MatchTimeSecs": (397, 0),
    "Online_KnockdownsLanded": (0, 5),
    "Online_StunsLanded": (0, 2),
    "Online_AdvanceStrikesLanded": (13, 25),
    "Online_SignificantStrikesThrown": (2, 9),
    "Online_StrikesLanded": (24, 34),
    "Online_StrikesThrown": (5, 12),
    "Online_TakedownsAttempted": (0, 3),
    "Online_TakedownsLanded": (0, 1),
    "Online_Control_Duration": (0, 1124402339),
}
args = argparse.Namespace(game_version="01.006.000", red_name="Makhachev", blue_name="McGregor")
report = exporter.build_report(fields, "ps5-pro", args)
assert report["result"]["winnerCorner"] == "red"
assert report["result"]["method"] == "Submission"
assert report["result"]["round"] == 2
assert report["result"]["clockSeconds"] == 203
red, blue = report["fighters"]
assert red["stats"]["totalStrikesAttempted"] == 34
assert red["stats"]["totalStrikesLanded"] == 12
assert red["stats"]["significantStrikesAttempted"] == 25
assert red["stats"]["significantStrikesLanded"] == 9
assert red["stats"]["knockdowns"] == 5
assert red["stats"]["controlTimeSeconds"] == 133
assert blue["stats"]["totalStrikesAttempted"] == 24
assert blue["stats"]["totalStrikesLanded"] == 5
for changed in (
    {"Global_MatchState": (2, 0)},
    {"Online_MatchResultType": (1, 7)},
    {"Online_MatchResult": (0, 0)},
    {"Online_StrikesThrown": (5, 99)},
):
    bad = dict(fields, **changed)
    try:
        exporter.build_report(bad, "ps5-pro", args)
    except RuntimeError:
        pass
    else:
        raise AssertionError(f"Invalid report accepted: {changed}")
print("Fight report field mapping: passed")
