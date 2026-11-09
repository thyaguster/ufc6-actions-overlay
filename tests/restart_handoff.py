"""Real helper/fixture process handoff; never closes or launches the user's game."""
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import sys

root=Path(__file__).resolve().parents[1]
k=c.WinDLL("kernel32",use_last_error=True)
k.CreateEventW.argtypes=[c.c_void_p,w.BOOL,w.BOOL,w.LPCWSTR];k.CreateEventW.restype=w.HANDLE
k.SetEvent.argtypes=[w.HANDLE];k.WaitForSingleObject.argtypes=[w.HANDLE,w.DWORD];k.CloseHandle.argtypes=[w.HANDLE]
with tempfile.TemporaryDirectory(prefix="restart fixture ",dir=root/".build") as temp:
    folder=Path(temp);binary=folder/"bin";binary.mkdir()
    launcher=folder/"UFC6.exe";game_path=binary/("UFC 6 Parsec.exe" if "--parsec" in sys.argv else "UFC 6.exe");helper=binary/"ufc6_menu_restart.exe"
    custom="--custom" in sys.argv
    persistent="--persistent" in sys.argv
    if custom:
        launcher=folder/"external launcher"/"Game Launcher.exe";launcher.parent.mkdir()
        working=folder/"working folder ação";working.mkdir()
        (binary/"ufc6-quit-menu.ini").write_text(f'[Launch]\nMode=Custom\nExecutable={launcher}\nArguments=raw:--custom-test "path with spaces" "ação"\nWorkingDirectory={working}\nWaitForLauncherExit={int(not persistent)}\n',encoding="utf-16")
    marker=launcher.parent/"launcher-started.txt"
    shutil.copy2(root/".build/restart_fixture.exe",launcher)
    shutil.copy2(root/".build/restart_fixture.exe",game_path)
    shutil.copy2(root/"dist/ufc6_menu_restart.exe",helper)
    token=f"{time.time_ns()}"
    names=[f"Local\\UFC6RestartTest_{token}_{n}" for n in ["game","launcher","ready"]]
    events=[k.CreateEventW(None,True,False,n) for n in names]
    assert all(events)
    processes=[]
    try:
        parent=subprocess.Popen([str(launcher),"--wait-event",names[1]],creationflags=0x08000000);processes.append(parent)
        game=subprocess.Popen([str(game_path),"--wait-event",names[0]],creationflags=0x08000000);processes.append(game)
        if custom:
            ini=binary/"ufc6-quit-menu.ini";original=ini.read_text(encoding="utf-16");missing=folder/"missing.exe"
            ini.write_text(original.replace(str(launcher),str(missing)),encoding="utf-16")
            rejected=subprocess.run([str(helper),str(game.pid),str(missing),str(parent.pid),names[2]],creationflags=0x08000000,timeout=5)
            assert rejected.returncode==2 and game.poll() is None and k.WaitForSingleObject(events[2],0)==258,"Missing launcher must be rejected before authorizing game close"
            ini.write_text(original,encoding="utf-16")
        bad=subprocess.run([str(helper),str(game.pid),str(folder/"wrong.exe"),str(parent.pid),names[2]],creationflags=0x08000000,timeout=5)
        assert bad.returncode==2 and not marker.exists()
        restart=subprocess.Popen([str(helper),str(game.pid),str(launcher),str(parent.pid),names[2]],creationflags=0x08000000);processes.append(restart)
        assert k.WaitForSingleObject(events[2],5000)==0,"Helper did not verify and open both process handles"
        assert not marker.exists()
        k.SetEvent(events[0]);assert game.wait(timeout=5)==0
        time.sleep(.2)
        if persistent:
            assert restart.wait(timeout=5)==0 and parent.poll() is None,"Persistent launcher must not block restart"
        else:assert not marker.exists(),"Restart raced the old launcher's mutex/cleanup"
        k.SetEvent(events[1]);assert parent.wait(timeout=5)==0
        assert restart.wait(timeout=5)==0
        deadline=time.monotonic()+3
        while not marker.exists() and time.monotonic()<deadline:time.sleep(.02)
        if custom:
            text=marker.read_text(encoding="utf-8")
            assert f"cwd={working}" in text and "arg2=path with spaces" in text and "arg3=ação" in text
        else:assert marker.read_text()=="--play --headless --ui-lang en"
        print("PASS: rejected wrong launcher, waited for game AND launcher, and relaunched once with correct arguments and spaced paths.")
    finally:
        for event in events:k.SetEvent(event)
        for process in processes:
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=5)
        for event in events:k.CloseHandle(event)
