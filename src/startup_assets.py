"""Asset-free recipes for reversible UFC 6 title/offline-warning bypasses."""
from __future__ import annotations
import json,re,os,zipfile
from pathlib import Path
import patcher_core as core
BACKUP_NAME='.ufc6-startup-screens-backup.zip'
PROFILE_NAME='startup-PPSA23566-01.006.000.json'
def load_profile(directory):
 raw=(Path(directory)/PROFILE_NAME).read_bytes();p=json.loads(raw)
 core.check(p.get('schema')==1 and len(p.get('entries',[]))==4,'Invalid startup patch profile.')
 seen=set()
 for e in p['entries']:
  core.check(e['id'] not in seen and 0<e['length']<128*1024 and e['offset']>=0,'Invalid startup range.');seen.add(e['id'])
  core.check(e['recipe'] in ('title','login','roster','roster-check'),'Unknown startup recipe.')
 p['_digest']=core.sha256(raw);return p
def make(e,old):
 core.check(len(old)==e['length'] and core.sha256(old)==e['original_sha256'],'Unsupported startup script; nothing changed.')
 if e['recipe']=='title':
  core.check(old.count(b'o.mControllerId = -1')==old.count(b'o.mAutoLogin = false')==1,'Title script changed.')
  new=old.replace(b'o.mControllerId = -1',b'o.mControllerId = 0 ').replace(b'o.mAutoLogin = false',b'o.mAutoLogin = true ')
 else:
  match=re.search(rb'\{\s*"name"',old);core.check(match is not None,'Roster script changed.');start=match.start()
  text=re.sub(r'/\*.*?\*/|//[^\n]*','',old[start:].decode('utf-8'),flags=re.S);nav=json.loads(text)
  target='login_failure' if e['recipe']=='login' else 'roster_download_failure' if e['recipe']=='roster' else 'roster_check_failure_popup'
  def find(n):
   if n.get('name')==target:return n
   for child in n.get('states',[]):
    result=find(child)
    if result is not None:return result
  state=find(nav);core.check(state is not None,'Startup failure state changed.')
  if e['recipe']=='login':
   core.check(state['onExit']==[['closePopup',['.popup']],['invoke',['LOGIN_TRANSITION_FAILURE']]],'Login failure exit changed.')
   original_enter=state['onEnter']
   core.check(original_enter==[['set',['popup',{}]],['openPopup',['.event.data','.popup']]],'Login popup creation changed.')
   state['onEnter']=[['set',['overlayKnownOffline',['or',['==','.event.data.body','TXT_UFC_BLAZE_CONNECTION_FAILURE'],['or',['==','.event.data.body','TXT_UNABLE_TO_LOGIN'],['==','.event.data.body','TXT_UNABLE_TO_LOGIN_EA']]]]],['if',['.overlayKnownOffline',[['invoke',['force_accept_login_fail_popup']]],original_enter]]]
   state['onExit']=[['if',['.overlayKnownOffline',[],[['closePopup',['.popup']]]]],['invoke',['LOGIN_TRANSITION_FAILURE']]]
  else:
   core.check(state['onExit']==[['closePopup',['.popup']]],'Roster continuation changed.')
   event='accept_roster_fail_download' if e['recipe']=='roster' else 'dismiss_roster_failure_popup'
   core.check(state['transitions']==[{'event':event,'targets':['exit_failure']}],'Roster failure exit changed.')
   state['onEnter']=[['invoke',[event]]];state['onExit']=[]
  encoded=json.dumps(nav,separators=(',',':'),ensure_ascii=True).encode('ascii');core.check(len(encoded)<=len(old)-start,'Roster patch does not fit.')
  new=old[:start]+encoded+b' '*(len(old)-start-len(encoded))
 core.check(len(new)==len(old),'Startup patch changed the archive length.')
 if 'installed_sha256' in e:core.check(core.sha256(new)==e['installed_sha256'],'Startup recipe verification failed.')
 return new
def read_backup(path,profile):
 core.check(path.stat().st_size<1024*1024,'Startup backup is unexpectedly large.')
 with zipfile.ZipFile(path) as z:
  names=['manifest.json']+[str(e['id'])+'.bin' for e in profile['entries']]
  core.check(sorted(z.namelist())==sorted(names),'Startup backup contents changed.')
  core.check(z.getinfo('manifest.json').file_size<16384,'Startup backup manifest is invalid.')
  m=json.loads(z.read('manifest.json'));core.check(m.get('profile') in [profile['_digest']]+[x['profile_digest'] for x in profile.get('previous_installations',[])],'Startup backup belongs to a different profile.')
  originals=[]
  for e in profile['entries']:
   name=str(e['id'])+'.bin';core.check(z.getinfo(name).file_size==e['length'],'Invalid startup backup range.')
   old=z.read(name);make(e,old);originals.append(old)
  return originals
def create_backup(path,profile,originals):
 temp=Path(str(path)+'.new');core.check(not path.exists() and not temp.exists(),'Startup backup already exists or needs inspection.')
 with temp.open('xb') as stream:
  with zipfile.ZipFile(stream,'w',compression=zipfile.ZIP_STORED) as z:
   z.writestr('manifest.json',json.dumps({'schema':1,'profile':profile['_digest']}))
   for e,old in zip(profile['entries'],originals):z.writestr(str(e['id'])+'.bin',old)
  stream.flush();os.fsync(stream.fileno())
 read_backup(temp,profile);os.replace(temp,path)
def run(action,root,profile):
 core.check(action in ('status','install','restore'),'Unknown startup action.')
 root=core.validate_game(root,profile)
 if action!='status':core.check(not core.game_running(),'Close UFC 6 before changing startup screens.')
 backup=core.inside(root,BACKUP_NAME)
 with core.GameFiles(root,[e['path'] for e in profile['entries']],action!='status') as files:
  current=[files.read(e) for e in profile['entries']]
  if backup.exists():originals=read_backup(backup,profile)
  else:
   core.check(all(core.sha256(b)==e['original_sha256'] for e,b in zip(profile['entries'],current)),'Startup scripts were modified without this patch backup; restore originals first.')
   originals=current
  patched=[make(e,b) for e,b in zip(profile['entries'],originals)]
  core.check(all(b in (old,new) or any(core.sha256(b)==previous.get('installed_hashes',{}).get(str(e['id'])) for previous in profile.get('previous_installations',[])) for e,b,old,new in zip(profile['entries'],current,originals,patched)),'Startup data changed after installation; nothing written.')
  state=1 if current==originals else 2 if current==patched else 3
  if action=='status':return state
  if action=='restore' and not backup.exists():return 1
  if action=='install' and not backup.exists():create_backup(backup,profile,originals)
  desired=patched if action=='install' else originals
  core.transaction(files,list(zip(profile['entries'],current,desired)))
  return 2 if action=='install' else 1
