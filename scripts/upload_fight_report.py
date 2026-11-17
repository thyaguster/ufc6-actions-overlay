"""Send one locally saved fight-report stage to Parsectagon off the game thread."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import time
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit
from urllib.request import Request, urlopen


def upload_key() -> str | None:
    token = os.environ.get("PARSECTAGON_OVERLAY_TOKEN", "").strip()
    if not token:
        path = Path.home() / ".parsectagon-overlay-token"
        if path.is_file():
            token = path.read_text(encoding="utf-8-sig").strip()
    return token if re.fullmatch(r"ptg_ovl_[A-Za-z0-9_-]{16,120}", token) else None


def endpoint(base_url: str) -> str:
    parts = urlsplit(base_url)
    if parts.scheme not in ("http", "https") or not parts.hostname or parts.username or parts.password or parts.query or parts.fragment or parts.path not in ("", "/"):
        raise ValueError("Invalid Parsectagon server URL")
    return base_url.rstrip("/") + "/api/bridge/game-capture"


def send_report(path: Path, base_url: str, token: str) -> dict:
    raw = path.read_bytes()
    if len(raw) > 131072:
        raise ValueError("Fight report is too large")
    capture = json.loads(raw)
    if capture.get("schema") != 1 or not capture.get("id") or not isinstance(capture.get("result"), dict):
        raise ValueError("Invalid fight report")
    digest = hashlib.sha256(raw).hexdigest()
    marker = path.with_suffix(path.suffix + ".upload.json")
    if marker.is_file():
        try:
            previous = json.loads(marker.read_text(encoding="utf-8"))
            if previous.get("sha256") == digest and previous.get("status") == "uploaded":
                return previous
        except (OSError, ValueError):
            pass
    request = Request(endpoint(base_url), data=json.dumps({"capture": capture}).encode("utf-8"),
                      headers={"Content-Type": "application/json", "Authorization": "Bearer " + token,
                               "User-Agent": "Parsectagon-UFC6-Overlay/0.9.71"},
                      method="POST")
    try:
        with urlopen(request, timeout=5) as response:
            reply = json.load(response)
            if response.status not in (200, 201) or reply.get("captureId") != capture["id"]:
                raise ValueError("Lobby returned an invalid capture acknowledgement")
    except HTTPError as error:
        detail = error.read(4096)
        try:
            message = json.loads(detail).get("error", "Upload rejected")
        except (ValueError, UnicodeError):
            message = "Upload rejected"
        raise RuntimeError(f"Lobby HTTP {error.code}: {message}") from error
    except URLError as error:
        raise RuntimeError(f"Cannot reach Parsectagon: {error.reason}") from error
    state = {"status": "uploaded", "sha256": digest, "captureId": reply["captureId"],
             "matchId": reply.get("matchId"), "stage": capture.get("reportStage", "complete"),
             "uploadedAt": int(time.time() * 1000)}
    temporary = marker.with_suffix(marker.suffix + ".new")
    temporary.write_text(json.dumps(state, separators=(",", ":")) + "\n", encoding="utf-8")
    os.replace(temporary, marker)
    return state


def run(path: Path, base_url: str) -> int:
    token = upload_key()
    if not token:
        return 2  # Local capture still succeeds; user has not linked a fighter account.
    try:
        send_report(path, base_url, token)
        return 0
    except (OSError, RuntimeError, ValueError, TypeError, json.JSONDecodeError) as error:
        failure = path.with_suffix(path.suffix + ".upload-error.txt")
        failure.write_text(str(error)[:500] + "\n", encoding="utf-8")
        return 1
