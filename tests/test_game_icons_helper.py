"""Handoff orchestration tests; fake processes and patch calls, no real game writes."""
import configparser
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('game_icons_helper',Path(__file__).resolve().parents[1]/'src/game_icons_helper.py')
helper=importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)


class FakeProcesses:
    def __init__(self,events,fail=None):self.events=events;self.fail=fail
    def open(self,pid,path):
        self.events.append(('open',pid))
        if self.fail=='identity':raise helper.core.PatchError('wrong process')
        return pid
    def ready(self,name):self.events.append(('ready',name))
    def wait(self,handle,timeout):
        self.events.append(('wait',handle))
        if self.fail=='timeout':raise helper.core.PatchError('still running')
    def close(self,handle):self.events.append(('close',handle))


class EnvironmentTests(unittest.TestCase):
    def test_launcher_is_independent_of_deleted_helper_runtime(self):
        inherited={'_PYI_APPLICATION_HOME_DIR':'missing runtime','_PYI_ARCHIVE_FILE':'icons.exe','_PYI_PARENT_PROCESS_LEVEL':'1','PYINSTALLER_RESET_ENVIRONMENT':'0','VK_INSTANCE_LAYERS':'overlay','CUSTOM_SETTING':'100% value'}
        with patch.dict(helper.os.environ,inherited,clear=True),patch.object(helper.subprocess,'Popen') as start:
            helper.launch_game({'launchexe':'C:/launcher.exe','launchargs':'--play --headless','workdir':'C:/game'})
            env=start.call_args.kwargs['env']
            self.assertEqual(env['PYINSTALLER_RESET_ENVIRONMENT'],'1')
            self.assertFalse(any(k.startswith('_PYI_') for k in env))
            self.assertEqual(env['VK_INSTANCE_LAYERS'],'overlay')
            self.assertEqual(env['CUSTOM_SETTING'],'100% value')
            self.assertEqual(helper.os.environ['_PYI_APPLICATION_HOME_DIR'],'missing runtime')


class HandoffTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.exe=self.root/'launcher with spaces.exe';self.exe.write_bytes(b'fixture')
        self.job=self.root/'job.ini';self.report=self.root/'result.ini';self.events=[]
        self.config=configparser.ConfigParser(interpolation=None)
        self.config['Job']={'pid':'10','parent':'11','gameexe':str(self.root/'UFC 6.exe'),'launchexe':str(self.exe),'launchargs':'--play --headless --name "100% Player"','workdir':str(self.root),'gameroot':str(self.root),'report':str(self.report),'theme':'xbox','event':'Local\\UFC6GameIcons_test'}
        self.write_job()
    def write_job(self):
        with self.job.open('w',encoding='utf-16') as f:self.config.write(f)
    def run_job(self,fail=None,patch_fail=None):
        def apply(theme,root,profile):
            self.events.append(('patch',theme))
            self.assertIn(('wait',11),self.events)
            if patch_fail:raise helper.core.PatchError(patch_fail)
            return 'verified'
        def launch(job):
            self.events.append(('launch',job['launchargs']))
            self.assertEqual(job['launchexe'],str(self.exe))
        with patch.object(helper.core,'load_profile',return_value={}),patch.object(helper.core,'validate_game',return_value=self.root):
            helper.handoff(self.job,FakeProcesses(self.events,fail),apply,launch)
    def test_waits_for_both_processes_then_patches_and_launches_once(self):
        self.run_job()
        self.assertLess(self.events.index(('wait',11)),self.events.index(('patch','xbox')))
        self.assertEqual(sum(e[0]=='patch' for e in self.events),1)
        self.assertEqual(sum(e[0]=='launch' for e in self.events),1)
        ini=configparser.ConfigParser();ini.read(self.report,encoding='utf-16');self.assertEqual(ini['Icons']['State'],'2')
        self.assertFalse(self.job.exists())
    def test_identity_failure_and_timeout_never_patch_or_relaunch(self):
        for fail in ['identity','timeout']:
            self.write_job();self.events.clear()
            with self.assertRaises(helper.core.PatchError):self.run_job(fail)
            self.assertFalse(any(e[0] in ('patch','launch') for e in self.events))
    def test_verified_patch_rejection_keeps_report_and_relaunches_old_icons(self):
        self.run_job(patch_fail='Unsupported version; nothing was written.')
        self.assertTrue(any(e[0]=='launch' for e in self.events))
        ini=configparser.ConfigParser();ini.read(self.report,encoding='utf-16');self.assertEqual(ini['Icons']['Success'],'0')
    def test_incomplete_rollback_does_not_launch(self):
        with self.assertRaises(helper.core.PatchError):self.run_job(patch_fail='Write failed and rollback was incomplete.')
        self.assertFalse(any(e[0]=='launch' for e in self.events))
    def test_invalid_theme_and_engine_filename_rejected(self):
        for key,value in [('theme','other'),('gameexe',str(self.root/'unrelated.exe'))]:
            before=self.config['Job'][key];self.config['Job'][key]=value;self.write_job()
            with self.assertRaises(helper.core.PatchError):helper.load_job(self.job)
            self.config['Job'][key]=before
    def test_cancelled_job_never_applies(self):
        original_wait=FakeProcesses.wait
        def wait(processes,handle,timeout):
            original_wait(processes,handle,timeout)
            if handle==11:self.job.unlink()
        with patch.object(FakeProcesses,'wait',wait),self.assertRaises(helper.core.PatchError):self.run_job()
        self.assertFalse(any(e[0] in ('patch','launch') for e in self.events))


if __name__=='__main__':unittest.main()
