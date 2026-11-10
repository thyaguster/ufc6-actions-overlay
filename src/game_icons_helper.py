"""Controller-driven handoff for the vendored, asset-free Xbox icons patcher."""
from __future__ import annotations
import argparse
import configparser
import ctypes as c
from ctypes import wintypes as w
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[1] / 'vendor' / 'xbox-icons'))
sys.path.insert(0, str(ROOT))
import patcher_core as core


def report(path, state, message, success=True):
    p = configparser.ConfigParser(interpolation=None)
    p['Icons'] = {'State': str(state), 'Success': str(int(success)),
                  'Message': str(message).replace('\r', ' ').replace('\n', ' ')[:1000]}
    temp = Path(str(path) + '.new')
    with temp.open('w', encoding='utf-16', newline='') as f:
        p.write(f)
        f.flush()
        os.fsync(f.fileno())
    os.replace(temp, path)


def state(root, profile):
    root = core.validate_game(root, profile)
    backup = root / core.BACKUP_NAME
    if not backup.exists():
        core.run('status', root, profile)  # Also rejects legacy installs without a restore backup.
        return 1
    saved = core.read_backup(backup, profile)
    with core.GameFiles(root, core.paths_for(profile), False) as files:
        spans = core.verify_backup_state(files, saved)
        if all(core.sha256(actual) == e['original_sha256'] for e, old, actual, installed in spans):
            return 1
        if all(installed for e, old, actual, installed in spans):
            return 2
        return 3


def set_theme(theme, root, profile):
    before = state(root, profile)
    desired = 2 if theme == 'xbox' else 1
    if before == desired:
        return 'The selected game icons are already installed.'
    core.run('install' if theme == 'xbox' else 'restore', root, profile)
    core.check(state(root, profile) == desired, 'Icon read-back verification failed.')
    return 'Xbox game icons installed.' if desired == 2 else 'Original PlayStation game icons restored.'


class Processes:
    def __init__(self):
        self.k = c.WinDLL('kernel32', use_last_error=True)
        self.k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
        self.k.OpenProcess.restype = w.HANDLE
        self.k.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
        self.k.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
        self.k.OpenEventW.argtypes = [w.DWORD, w.BOOL, w.LPCWSTR]
        self.k.OpenEventW.restype = w.HANDLE
        self.k.SetEvent.argtypes = [w.HANDLE]
        self.k.CloseHandle.argtypes = [w.HANDLE]

    def open(self, pid, expected):
        core.check(pid > 0, 'Invalid process ID.')
        h = self.k.OpenProcess(0x101000, False, pid)
        core.check(h, 'The previous session cannot be verified.')
        name = c.create_unicode_buffer(32768)
        length = w.DWORD(len(name))
        if not self.k.QueryFullProcessImageNameW(h, 0, name, c.byref(length)) or os.path.normcase(name.value) != os.path.normcase(str(expected)):
            self.k.CloseHandle(h)
            raise core.PatchError('The process identity changed; the game was left running.')
        return h

    def ready(self, name):
        core.check(name.startswith('Local\\UFC6GameIcons_'), 'Invalid readiness event.')
        event = self.k.OpenEventW(2, False, name)
        core.check(event, 'The overlay handoff is no longer available.')
        try:
            core.check(self.k.SetEvent(event), 'Cannot acknowledge the overlay handoff.')
        finally:
            self.k.CloseHandle(event)

    def wait(self, handle, timeout):
        core.check(self.k.WaitForSingleObject(handle, timeout) == 0,
                   'The previous session is still running; no files were changed and no second game was launched.')

    def close(self, handle):
        if handle:
            self.k.CloseHandle(handle)


def load_job(path):
    core.check(path.stat().st_size < 64 * 1024, 'Invalid game-icons job.')
    ini = configparser.ConfigParser(interpolation=None)
    ini.read(path, encoding='utf-16')
    job = dict(ini['Job'])
    for key in ('gameexe', 'launchexe', 'workdir', 'gameroot', 'report'):
        core.check(key in job and Path(job[key]).is_absolute() and '\n' not in job[key] and '\r' not in job[key], 'Invalid job path.')
    core.check(Path(job['gameexe']).name.casefold() in ('ufc 6.exe', 'ufc 6 parsec.exe'), 'Unsupported engine filename.')
    core.check(Path(job['launchexe']).is_file() and Path(job['workdir']).is_dir(), 'The configured launcher is unavailable.')
    core.check(job['theme'] in ('xbox', 'playstation'), 'Invalid game icon theme.')
    core.check('\n' not in job['launchargs'] and '\r' not in job['launchargs'], 'Invalid launcher arguments.')
    return job


def launch_environment():
    # A fresh launcher/game must outlive this helper's temporary extraction.
    # Preserve game/launcher settings while dropping only PyInstaller state.
    env = {key: value for key, value in os.environ.items()
           if not key.upper().startswith('_PYI_')
           and key.upper() != 'PYINSTALLER_RESET_ENVIRONMENT'}
    env['PYINSTALLER_RESET_ENVIRONMENT'] = '1'
    return env


def launch_game(job):
    command = subprocess.list2cmdline([job['launchexe']]) + (' ' + job['launchargs'] if job['launchargs'] else '')
    kernel = old_directory = None
    if os.name == 'nt' and getattr(sys, 'frozen', False):
        kernel = c.WinDLL('kernel32', use_last_error=True)
        kernel.GetDllDirectoryW.argtypes = [w.DWORD, w.LPWSTR]
        kernel.SetDllDirectoryW.argtypes = [w.LPCWSTR]
        buffer = c.create_unicode_buffer(32768)
        kernel.GetDllDirectoryW(len(buffer), buffer)
        old_directory = buffer.value or None
        kernel.SetDllDirectoryW(None)
    try:
        subprocess.Popen(command, executable=job['launchexe'], cwd=job['workdir'],
                         creationflags=0x08000000, env=launch_environment())
    finally:
        if kernel:
            kernel.SetDllDirectoryW(old_directory)


def handoff(path, processes=None, patch=set_theme, launch=None):
    job = load_job(path)
    profile = core.load_profile(ROOT)
    core.validate_game(job['gameroot'], profile)
    # Resolve the launcher exactly as saved by the overlay; no shell is involved.
    if launch is None:
        launch = launch_game
    processes = processes or Processes()
    game = parent = None
    try:
        game = processes.open(int(job['pid']), job['gameexe'])
        if int(job.get('parent', '0')):
            parent = processes.open(int(job['parent']), job['launchexe'])
        processes.ready(job['event'])
        processes.wait(game, 60000)
        if parent:
            processes.wait(parent, 30000)
        core.check(path.exists(), "The overlay cancelled this icons request; no game files were changed.")
        try:
            message = patch(job['theme'], Path(job['gameroot']), profile)
            report(job['report'], 2 if job['theme'] == 'xbox' else 1, message)
        except Exception as exc:
            report(job['report'], 4, exc, False)
            if 'rollback was incomplete' in str(exc).lower():
                raise
            # Preflight rejection / verified rollback keeps the previous artwork.
            # Relaunch it normally and leave the actionable result for the overlay.
        launch(job)
    finally:
        processes.close(game)
        processes.close(parent)
        path.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--status', action='store_true')
    parser.add_argument('--game-dir', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--apply-job', type=Path)
    args = parser.parse_args()
    try:
        if args.apply_job:
            handoff(args.apply_job)
        elif args.status and args.game_dir and args.report:
            current = state(args.game_dir, core.load_profile(ROOT))
            report(args.report, current, {1: 'PlayStation game icons', 2: 'Xbox game icons', 3: 'Mixed verified icons; choose a theme to repair.'}[current])
        else:
            parser.error('Use --status with --game-dir/--report, or --apply-job.')
        return 0
    except Exception as exc:
        if args.report:
            report(args.report, 4, exc, False)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
