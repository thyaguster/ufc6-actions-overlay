"""Check that retained overall stats cannot be matched to another fight."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from export_complete_fight_report import matches_overall_fight

red = {"KNOCKDOWNS_LANDED": 3, "SIGNIFICANT_STRIKES_THROWN": 0,
       "SIGNIFICANT_STRIKES_LANDED": 7}
blue = {"KNOCKDOWNS_LANDED": 0, "SIGNIFICANT_STRIKES_THROWN": 0,
        "SIGNIFICANT_STRIKES_LANDED": 1}
response = {"Success": True, "Output": {"RESULTS": {
    "ROUND_NUMBER": 1, "RESULT_TYPE": 0, "FIGHTER_STATS": [blue, red]}}}
fields = {"Global_RoundNum": (0, 0), "Global_MatchResultType": (0, 0),
          "Online_KnockdownsLanded": (0, 3),
          "Online_SignificantStrikesThrown": (0, 0),
          "Online_AdvanceStrikesLanded": (1, 7)}
assert matches_overall_fight(response, fields)
assert not matches_overall_fight(response, {**fields, "Global_RoundNum": (1, 0)})
assert not matches_overall_fight(response, {**fields, "Online_KnockdownsLanded": (0, 2)})
print("Retained overall-stat fallback: passed")
