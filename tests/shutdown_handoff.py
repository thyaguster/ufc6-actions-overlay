"""Run ONLY the test-compiled helper; Windows shutdown is replaced by a file marker."""
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import sys
import os
import re

reboot="--reboot" in sys.argv
fail="--fail" in sys.argv
conflict="--conflict" in sys.argv
no_auto="--no-auto" in sys.argv

root=Path(__file__).resolve().parents[1]
test_helper=root/".build/shutdown_helper_test.exe"
assert b"ExitWindowsEx" not in test_helper.read_bytes(),"Test helper must not import the Windows shutdown API"
assert b"AdjustTokenPrivileges" not in test_helper.read_bytes(),"Test helper must not modify token privileges"
k=c.WinDLL("kernel32",use_last_error=True)
k.CreateEventW.argtypes=[c.c_void_p,w.BOOL,w.BOOL,w.LPCWSTR];k.CreateEventW.restype=w.HANDLE
k.SetEvent.argtypes=[w.HANDLE];k.WaitForSingleObject.argtypes=[w.HANDLE,w.DWORD];k.CloseHandle.argtypes=[w.HANDLE]
with tempfile.TemporaryDirectory(prefix="shutdown fixture ",dir=root/".build") as temp:
    folder=Path(temp);binary=folder/"bin";binary.mkdir()
    launcher=folder/"UFC6.exe";game_path=binary/("UFC 6 Parsec.exe" if "--parsec" in sys.argv else "UFC 6.exe");helper=binary/"ufc6_menu_restart.exe"
    shutil.copy2(root/".build/restart_fixture.exe",launcher)
    shutil.copy2(root/".build/restart_fixture.exe",game_path)
    shutil.copy2(test_helper,helper)
    resume=binary/"resume-command.test.txt"
    if conflict:resume.write_text("existing registration")
    names=[f"Local\\UFC6PowerTest_{time.time_ns()}_{n}" for n in ["game","launcher","ready"]]
    events=[k.CreateEventW(None,True,False,n) for n in names];assert all(events)
    processes=[]
    try:
        parent=subprocess.Popen([str(launcher),"--wait-event",names[1]],creationflags=0x08000000);processes.append(parent)
        game=subprocess.Popen([str(game_path),"--wait-event",names[0]],creationflags=0x08000000);processes.append(game)
        env=os.environ.copy()
        if fail:env["UFC6_TEST_POWER_FAIL"]="1"
        power=subprocess.Popen([str(helper),str(game.pid),str(launcher),str(parent.pid),names[2],"--reboot-only" if no_auto else "--reboot-pc" if reboot else "--shutdown-pc"],env=env,creationflags=0x08000000);processes.append(power)
        if conflict and not no_auto:
            assert power.wait(timeout=5)==8
            assert k.WaitForSingleObject(events[2],0)==258,"Failed registration must not signal game close"
            assert game.poll() is None and parent.poll() is None
            assert resume.read_text()=="existing registration"
            print("PASS: existing resume registration preserved; reboot rejected before closing the game.")
            sys.exit(0)
        assert k.WaitForSingleObject(events[2],5000)==0
        marker=binary/"shutdown-requested.test.txt"
        assert not marker.exists()
        if reboot and not no_auto:assert resume.exists(),"Resume must be registered before game closure is authorized"
        if no_auto:assert resume.exists()==conflict,"Auto-launch Off must not register a launch"
        k.SetEvent(events[0]);assert game.wait(timeout=5)==0
        time.sleep(.1);assert not marker.exists(),"Power handoff must also wait for launcher cleanup"
        k.SetEvent(events[1]);assert parent.wait(timeout=5)==0
        assert power.wait(timeout=5)==(7 if fail else 0)
        if fail:
            assert not marker.exists() and not resume.exists(),"Rejected reboot must remove the resume registration"
            print("PASS: rejected power request removed the one-time resume registration.")
            sys.exit(0)
        assert marker.read_text().startswith("flags=0x00000002 " if reboot else "flags=0x00000008 "),"Power request must not use force flags"
        assert not (folder/"launcher-started.txt").exists(),"Shutdown must never relaunch the game"
        if no_auto:
            assert resume.exists()==conflict
            if conflict:assert resume.read_text()=="existing registration"
            print("PASS: auto-launch Off reboots without registering or launching the game; existing registration untouched.")
        elif reboot:
            command=resume.read_bytes().decode("utf-16-le").rstrip("\0")
            match=re.fullmatch(r'"(.+)" --resume-game (\d+)',command);assert match and Path(match[1])==helper
            resume.unlink() # Windows consumes RunOnce before dispatching the command.
            assert subprocess.run([str(helper),"--resume-game",match[2]],timeout=5).returncode==0
            assert not (folder/"launcher-started.txt").exists(),"Logging off in the same boot must not relaunch"
            assert subprocess.run([str(helper),"--resume-game","1"],timeout=5).returncode==0
            deadline=time.monotonic()+5
            while not (folder/"launcher-started.txt").exists() and time.monotonic()<deadline:time.sleep(.02)
            assert (folder/"launcher-started.txt").read_text()=="--play --headless --ui-lang en"
            assert not resume.exists()
            assert subprocess.run([str(helper),"--test-cancel-watcher"],timeout=5).returncode==0
            assert not resume.exists(),"Windows cancelling reboot must remove the registration"
            print("PASS: reboot waits, non-forcing flags, one-time quoted command, same-boot guard and simulated post-reboot launch; OS API absent.")
        else:print("PASS: shutdown handoff waited for both fixture processes; power-off flags recorded without force; OS API absent.")
    finally:
        for event in events:k.SetEvent(event)
        for process in processes:
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=5)
        for event in events:k.CloseHandle(event)
