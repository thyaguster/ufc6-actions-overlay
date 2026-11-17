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
import input_icons
from keyboard_bindings import defaults, load


def bindings_path():
    # Keep the player's editor choices beside the installed helper, outside game archives.
    return (Path(sys.executable).parent if getattr(sys, 'frozen', False)
            else Path(__file__).resolve().parents[1] / 'dist') / 'ufc6-input-bindings.json'


def selected_bindings(root=None):
    saved = bindings_path()
    if saved.is_file():
        return load(saved)
    if root:
        root = Path(root)
        for parent in (root, *list(root.parents)[:4]):
            for candidate in (parent / 'UFC6.ini', parent / 'games' / 'UFC6' / 'UFC6.ini'):
                if candidate.is_file():
                    return load(candidate)
    return defaults()


def report(path, state, message, success=True, section="Icons"):
    p = configparser.ConfigParser(interpolation=None)
    p[section] = {'State': str(state), 'Success': str(int(success)),
                  'Message': str(message).replace('\r', ' ').replace('\n', ' ')[:1000]}
    temp = Path(str(path) + '.new')
    with temp.open('w', encoding='utf-16', newline='') as f:
        p.write(f)
        f.flush()
        os.fsync(f.fileno())
    os.replace(temp, path)


def state(root, profile):
    result = input_icons.run('status', root, profile)
    mode = result.split('Current icons: ', 1)[1].split('. All ', 1)[0]
    return {'playstation': 1, 'xbox': 2, 'xbox / previous install': 2,
            'keyboard': 5}.get(mode, 3)


def set_theme(theme, root, profile):
    if theme in ('startup_on', 'startup_off'):
        import startup_assets
        value = startup_assets.run('install' if theme == 'startup_on' else 'restore', root, startup_assets.load_profile(ROOT / 'profiles'))
        return 'Startup bypass enabled and verified.' if value == 2 else 'Original startup screens restored and verified.'
    desired = {'playstation': 1, 'xbox': 2, 'keyboard': 5}[theme]
    input_icons.run('restore' if theme == 'playstation' else 'install', root, profile,
                    style=theme, bindings=selected_bindings(root))
    core.check(state(root, profile) == desired, 'Icon read-back verification failed.')
    return {'xbox': 'Xbox game icons installed.', 'keyboard': 'Keyboard and mouse icons installed.',
            'playstation': 'Original PlayStation game icons restored.'}[theme]


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
    core.check(Path(job['gameexe']).name.casefold() in ('ufc 6.exe', 'ufc 6 parsec.exe', 'ufc 6 novo.exe'), 'Unsupported engine filename.')
    core.check(Path(job['launchexe']).is_file() and Path(job['workdir']).is_dir(), 'The configured launcher is unavailable.')
    core.check(job['theme'] in ('xbox', 'keyboard', 'playstation', 'startup_on', 'startup_off'), 'Invalid game icon theme.')
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


def start_report_upload(path, url):
    command = ([sys.executable] if getattr(sys, 'frozen', False)
               else [sys.executable, str(Path(__file__).resolve())])
    command += ['--upload-fight-report', str(path), '--lobby-url', url]
    subprocess.Popen(command, close_fds=True, creationflags=0x08000000 if os.name == 'nt' else 0,
                     env=launch_environment())


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
    profile = input_icons.load_profile(ROOT)
    core.validate_game(job['gameroot'], profile['base'])
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
            report(job['report'], {'xbox': 2, 'keyboard': 5, 'startup_on': 2}.get(job['theme'], 1), message, section='Startup' if job['theme'].startswith('startup_') else 'Icons')
        except Exception as exc:
            report(job['report'], 4, exc, False, section='Startup' if job['theme'].startswith('startup_') else 'Icons')
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
    parser.add_argument('--startup-status', action='store_true')
    parser.add_argument('--backup-saves', action='store_true')
    parser.add_argument('--save-root', type=Path)
    parser.add_argument('--backup-dir', type=Path)
    parser.add_argument('--game-dir', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--apply-job', type=Path)
    parser.add_argument('--edit-bindings', action='store_true')
    parser.add_argument('--export-fight-report', action='store_true')
    parser.add_argument('--auto-fight-report', action='store_true')
    parser.add_argument('--preliminary-report', type=Path)
    parser.add_argument('--upload-fight-report', type=Path)
    parser.add_argument('--link-lobby', action='store_true')
    parser.add_argument('--unlink-lobby', action='store_true')
    parser.add_argument('--lobby-account-status', action='store_true')
    parser.add_argument('--lobby-url', default='https://parsectagon.cc')
    parser.add_argument('--game-version', default='unknown')
    args = parser.parse_args()
    try:
        if args.link_lobby:
            from lobby_account import link
            return link(args.lobby_url)
        if args.unlink_lobby:
            from lobby_account import unlink
            return unlink(args.lobby_url)
        if args.lobby_account_status:
            from lobby_account import check
            return check(args.lobby_url)
        if args.upload_fight_report:
            from upload_fight_report import run
            return run(args.upload_fight_report, args.lobby_url)
        if args.export_fight_report:
            if not args.report:
                parser.error('--export-fight-report needs --report.')
            from export_complete_fight_report import capture, save_report
            from fight_report_export import game_process
            from read_postfight_capture import read_capture, build_complete_report
            pid, engine = game_process()
            try:
                # The UI keeps its own complete totals/rounds record while
                # Fight Statistics is open. Validate it against live bindings.
                stats = read_capture(pid, 8)
                fight = build_complete_report(pid, engine, stats, args.game_version)
            except (RuntimeError, OSError, ValueError):
                if args.auto_fight_report:
                    raise
                # Older installations have no UI capture. Preserve their
                # existing read-only result/overall export path.
                fight = capture(None, 15, args.game_version)
            if args.preliminary_report and fight['fightStatisticsComplete']:
                import json
                try:
                    previous = json.loads(args.preliminary_report.read_text(encoding='utf-8'))
                    before = previous.get('result', {})
                    after = fight['result']
                    same_fight = (previous.get('reportStage') == 'result' and
                                  all(before.get(key) == after.get(key) for key in
                                      ('winnerCorner', 'gameMethodCode', 'round')) and
                                  abs(before.get('clockSeconds', -999) -
                                      after.get('clockSeconds', 999)) <= 1)
                    if same_fight:
                        fight['id'] = previous['id']
                        fight['resultCapturedAt'] = previous['capturedAt']
                        fight['preliminaryReport'] = args.preliminary_report.name
                except (OSError, ValueError, KeyError, TypeError):
                    pass
            destination = None
            if args.preliminary_report:
                destination = args.preliminary_report.with_name(
                    f"fight-complete-{fight['id'][:8]}.json")
            saved = save_report(fight, destination)
            if args.auto_fight_report:
                start_report_upload(saved, args.lobby_url)
            if fight['fightStatisticsComplete']:
                report(args.report, 2, f'Complete fight report saved: {saved}', section='FightReport')
            else:
                # The proven v0.9.68 UI reads helper messages for every state
                # except 2. Keep the filename and the missing-round warning
                # visible without changing its native worker.
                report(args.report, 3, 'Overall stats saved; round details unavailable. See Documents\\Parsectagon\\Fight Reports.',
                       section='FightReport')
        elif args.edit_bindings:
            from input_gui import gui
            from xbox_icons import license_text
            gui(ROOT, '1.1.0', license_text, overlay_bindings_path=bindings_path(), initial_game_dir=args.game_dir)
        elif args.backup_saves:
            from save_backups import create_backup
            if not args.save_root or not args.backup_dir or not args.report:
                parser.error('--backup-saves needs --save-root, --backup-dir and --report.')
            path, count = create_backup(args.save_root, args.backup_dir)
            report(args.report, 2, f"Backup saved and verified ({count} files).", section="Backup")
        elif args.apply_job:
            handoff(args.apply_job)
        elif args.startup_status and args.game_dir and args.report:
            import startup_assets
            current = startup_assets.run('status', args.game_dir, startup_assets.load_profile(ROOT / 'profiles'))
            report(args.report, current, {1:'Original startup screens', 2:'Title and offline-warning bypass enabled', 3:'Mixed verified startup patch; enable or restore to repair.'}[current], section='Startup')
        elif args.status and args.game_dir and args.report:
            current = state(args.game_dir, input_icons.load_profile(ROOT))
            report(args.report, current, {1: 'PlayStation game icons', 2: 'Xbox game icons', 3: 'Mixed verified icons; choose a theme to repair.', 5: 'Keyboard and mouse game icons'}[current])
        else:
            parser.error('Use --status with --game-dir/--report, or --apply-job.')
        return 0
    except Exception as exc:
        if args.report:
            report(args.report, 4, exc, False, section="FightReport" if args.export_fight_report else "Backup" if args.backup_saves else "Startup" if args.startup_status else "Icons")
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
