"""Startup patch safety tests with synthetic scripts; no game assets required."""
import importlib.util,json,sys,tempfile,unittest,zipfile
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'vendor/xbox-icons'))
spec=importlib.util.spec_from_file_location('startup_assets',ROOT/'src/startup_assets.py');assets=importlib.util.module_from_spec(spec);spec.loader.exec_module(assets)
class StartupAssetsTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
  (self.root/'sce_sys').mkdir();(self.root/'sce_sys/param.json').write_text(json.dumps({'titleId':'PPSA23566','contentVersion':'01.006.000'}))
  self.profile={'title_id':'PPSA23566','content_version':'01.006.000','_digest':'fixture','entries':[]};self.originals={}
  for recipe in ('title','login','roster','roster-check'):
   if recipe=='title':body=b'o.mControllerId = -1\no.mAutoLogin = false\n'
   elif recipe=='login':body=json.dumps({'name':'login_failure','onEnter':[['set',['popup',{}]],['openPopup',['.event.data','.popup']]],'onExit':[['closePopup',['.popup']],['invoke',['LOGIN_TRANSITION_FAILURE']]],'transitions':[{'event':'force_accept_login_fail_popup','targets':['suspended']}]}).encode()+b' '*1000
   else:
    name='roster_download_failure' if recipe=='roster' else 'roster_check_failure_popup';event='accept_roster_fail_download' if recipe=='roster' else 'dismiss_roster_failure_popup'
    body=json.dumps({'name':name,'onEnter':[['openPopup',['fixture']]],'onExit':[['closePopup',['.popup']]],'transitions':[{'event':event,'targets':['exit_failure']}]}).encode()+b' '*600
   old=b'wrapper'+body;relative=recipe+'.cas';data=b'HEAD'+old+b'TAIL';(self.root/relative).write_bytes(data);self.originals[relative]=data
   entry={'id':len(self.profile['entries']),'recipe':recipe,'path':relative,'offset':4,'length':len(old),'original_sha256':assets.core.sha256(old)}
   entry['installed_sha256']=assets.core.sha256(assets.make(entry,old));self.profile['entries'].append(entry)
  self.running=patch.object(assets.core,'game_running',return_value=False);self.running.start();self.addCleanup(self.running.stop)
 def run_patch(self,action):return assets.run(action,self.root,self.profile)
 def assert_originals(self):
  for p,b in self.originals.items():self.assertEqual((self.root/p).read_bytes(),b)
 def test_install_restore_and_reinstall(self):
  self.assertEqual(self.run_patch('status'),1);self.assertEqual(self.run_patch('install'),2);self.assertEqual(self.run_patch('status'),2)
  backup=self.root/assets.BACKUP_NAME;self.assertTrue(backup.is_file());saved=backup.read_bytes()
  self.assertEqual(self.run_patch('restore'),1);self.assert_originals();self.assertEqual(self.run_patch('install'),2);self.assertEqual(saved,backup.read_bytes())
 def test_unknown_range_rejected_before_backup_or_writes(self):
  p=self.root/'roster-check.cas';p.write_bytes(b'UNKNOWN'+p.read_bytes()[7:]);before={f.name:f.read_bytes() for f in self.root.glob('*.cas')}
  with self.assertRaises(assets.core.PatchError):self.run_patch('install')
  self.assertFalse((self.root/assets.BACKUP_NAME).exists())
  for p,b in before.items():self.assertEqual((self.root/p).read_bytes(),b)
 def test_running_game_refused(self):
  with patch.object(assets.core,'game_running',return_value=True):
   with self.assertRaises(assets.core.PatchError):self.run_patch('install')
  self.assert_originals()
 def test_tampered_backup_refused(self):
  self.run_patch('install');p=self.root/assets.BACKUP_NAME;p.write_bytes(b'bad backup');before={f.name:f.read_bytes() for f in self.root.glob('*.cas')}
  with self.assertRaises((assets.core.PatchError,zipfile.BadZipFile)):self.run_patch('restore')
  for p,b in before.items():self.assertEqual((self.root/p).read_bytes(),b)
 def test_write_failure_rolls_back(self):
  real=assets.core.GameFiles.write;calls=[0]
  def fail(files,entry,data):
   calls[0]+=1
   if calls[0]==2:raise OSError('injected')
   real(files,entry,data)
  with patch.object(assets.core.GameFiles,'write',fail):
   with self.assertRaisesRegex(assets.core.PatchError,'all attempted writes were restored'):self.run_patch('install')
  self.assert_originals();self.assertEqual(self.run_patch('status'),1)
 def test_login_recipe_preserves_other_error_handling(self):
  entry=self.profile['entries'][1];old=self.originals['login.cas'][4:-4];new=assets.make(entry,old)
  nav=json.loads(new[new.index(b'{'):]);self.assertEqual(len(new),len(old))
  self.assertEqual(nav['onEnter'][0],['set',['overlayKnownOffline',['or',['==','.event.data.body','TXT_UFC_BLAZE_CONNECTION_FAILURE'],['or',['==','.event.data.body','TXT_UNABLE_TO_LOGIN'],['==','.event.data.body','TXT_UNABLE_TO_LOGIN_EA']]]]])
  branch=nav['onEnter'][1];self.assertEqual(branch[0],'if');self.assertEqual(branch[1][0],'.overlayKnownOffline')
  self.assertEqual(branch[1][1],[['invoke',['force_accept_login_fail_popup']]])
  self.assertEqual(branch[1][2],[['set',['popup',{}]],['openPopup',['.event.data','.popup']]])
  self.assertEqual(nav['onExit'][0],['if',['.overlayKnownOffline',[],[['closePopup',['.popup']]]]])
  self.assertEqual(nav['onExit'][1],['invoke',['LOGIN_TRANSITION_FAILURE']])
  self.assertNotIn(b'LOGIN_SUCCESS',new)
 def test_known_previous_recipe_updates_without_replacing_original_backup(self):
  self.run_patch('install');backup=self.root/assets.BACKUP_NAME;saved=backup.read_bytes()
  entry=self.profile['entries'][1];p=self.root/entry['path'];current=p.read_bytes();previous=b'X'+current[entry['offset']+1:entry['offset']+entry['length']]
  with p.open('r+b') as f:f.seek(entry['offset']);f.write(previous)
  self.profile['previous_installations']=[{'profile_digest':self.profile['_digest'],'installed_hashes':{str(entry['id']):assets.core.sha256(previous)}}];self.profile['_digest']='next-fixture'
  self.assertEqual(self.run_patch('status'),3);self.assertEqual(self.run_patch('install'),2)
  self.assertEqual(backup.read_bytes(),saved);self.assertEqual(self.run_patch('restore'),1);self.assert_originals()
 def test_unrecognized_backup_profile_refused(self):
  self.run_patch('install');self.profile['_digest']='unknown-new-profile'
  with self.assertRaisesRegex(assets.core.PatchError,'different profile'):self.run_patch('restore')
 def test_backup_kept_on_restore(self):
  self.run_patch('install');self.run_patch('restore');self.assertTrue((self.root/assets.BACKUP_NAME).exists());self.assert_originals()
if __name__=='__main__':unittest.main()
