"""Browser-approved, upload-only Parsectagon account linking for the overlay."""
from __future__ import annotations

import json
import msvcrt
import os
from pathlib import Path
import time
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen
import webbrowser

from upload_fight_report import endpoint, upload_key


KEY_FILE = Path.home() / ".parsectagon-overlay-token"
STATE_FILE = Path.home() / ".parsectagon-overlay-link.json"
LOCK_FILE = Path.home() / ".parsectagon-overlay-link.lock"


def _save_state(**state: object) -> None:
    temporary = STATE_FILE.with_suffix(".new")
    temporary.write_text(json.dumps(state, separators=(",", ":")) + "\n", encoding="utf-8")
    os.replace(temporary, STATE_FILE)


def _request(base: str, route: str, payload: dict, token: str | None = None) -> tuple[int, dict]:
    url = endpoint(base).removesuffix("/api/bridge/game-capture") + route
    headers = {"Content-Type": "application/json", "User-Agent": "Parsectagon-UFC6-Overlay/0.9.71"}
    if token:
        headers["Authorization"] = "Bearer " + token
    request = Request(url, data=json.dumps(payload).encode("utf-8"), headers=headers, method="POST")
    try:
        with urlopen(request, timeout=5) as response:
            return response.status, json.load(response)
    except HTTPError as error:
        try:
            message = json.load(error).get("error", "Request failed")
        except (ValueError, UnicodeError):
            message = "Request failed"
        raise RuntimeError(f"Parsectagon HTTP {error.code}: {message}") from error
    except URLError as error:
        raise RuntimeError(f"Cannot reach Parsectagon: {error.reason}") from error


def check(base: str) -> int:
    # Opening the lobby again must not erase a code awaiting browser approval.
    try:
        current = json.loads(STATE_FILE.read_text(encoding="utf-8"))
        if current.get("status") == "pending" and current.get("expiresAt", 0) > time.time() * 1000:
            return 0
    except (OSError, ValueError, TypeError):
        pass
    token = upload_key()
    if not token:
        _save_state(status="unlinked")
        return 0
    try:
        _, data = _request(base, "/api/overlay/link/me", {}, token)
        fighter = data["fighter"]
        _save_state(status="linked", name=fighter["name"], rank=fighter["rank"], rating=fighter["rating"])
        return 0
    except (RuntimeError, OSError, KeyError, ValueError) as error:
        _save_state(status="error", message=str(error)[:160])
        return 1


def _link(base: str) -> int:
    try:
        _, data = _request(base, "/api/overlay/link/start", {})
        secret, code = data["deviceSecret"], data["code"]
        _save_state(status="pending", code=code, expiresAt=data["expiresAt"])
        browser_url = endpoint(base).removesuffix("/api/bridge/game-capture") + "/link-overlay.html?code=" + quote(code)
        webbrowser.open(browser_url)
        while time.time() * 1000 < data["expiresAt"]:
            time.sleep(max(3, min(10, int(data.get("pollSeconds", 3)))))
            status, result = _request(base, "/api/overlay/link/poll", {"deviceSecret": secret})
            if status == 202:
                continue
            if status != 200 or result.get("status") != "linked":
                raise RuntimeError("Parsectagon did not confirm the link")
            token, fighter = result["overlayToken"], result["fighter"]
            temporary = KEY_FILE.with_suffix(".new")
            temporary.write_text(token + "\n", encoding="utf-8")
            os.replace(temporary, KEY_FILE)
            _save_state(status="linked", name=fighter["name"], rank=fighter["rank"], rating=fighter["rating"])
            return 0
        raise RuntimeError("Link code expired. Select Link account to try again.")
    except (RuntimeError, OSError, KeyError, ValueError, TypeError) as error:
        _save_state(status="error", message=str(error)[:160])
        return 1


def link(base: str) -> int:
    # Repeated clicks should keep one active code and one polling helper.
    with LOCK_FILE.open("a+b") as lock:
        if lock.tell() == 0:
            lock.write(b"\0")
            lock.flush()
        lock.seek(0)
        try:
            msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError:
            return 0
        try:
            return _link(base)
        finally:
            lock.seek(0)
            msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)


def unlink(base: str) -> int:
    token = upload_key()
    if not token:
        _save_state(status="unlinked")
        return 0
    try:
        _request(base, "/api/overlay/link/revoke", {}, token)
        KEY_FILE.unlink(missing_ok=True)
        _save_state(status="unlinked")
        return 0
    except (RuntimeError, OSError, ValueError) as error:
        _save_state(status="error", message=str(error)[:160])
        return 1
