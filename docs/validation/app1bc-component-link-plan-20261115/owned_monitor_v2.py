"""Bound only an explicitly spawned new session; never process-name cleanup."""
import os,signal,time,subprocess,resource
from pathlib import Path
class BudgetExceeded(RuntimeError):pass

def identity(pid):
 try:
  f=Path('/proc',str(pid),'stat').read_text();v=f[f.rfind(')')+2:].split()
  return {'pid':pid,'start':int(v[19]),'pgrp':int(v[2]),'session':int(v[3]),'ppid':int(v[1]),'state':v[0]}
 except(FileNotFoundError,ProcessLookupError):return None

def same(a,b):return bool(a and b and all(a[k]==b[k]for k in ['pid','start','pgrp','session']))
def alive(record):
 now=identity(record['pid']);return same(record,now)and now['state']!='Z'
def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
class Owned:
 def __init__(self,proc):
  self.proc=proc;root=identity(proc.pid)
  if not root or root['pgrp']!=proc.pid or root['session']!=proc.pid or root['ppid']!=os.getpid():raise RuntimeError('New-session owner identity not established')
  self.root=root;self.known={proc.pid:root};self.signals=[];self.peak_rss=0;self.peak_virtual=0;self.samples=0
 def refresh(self):
  todo=list(self.known);seen=set();rss=0
  while todo:
   pid=todo.pop()
   if pid in seen:continue
   seen.add(pid);record=self.known[pid];now=identity(pid)
   if not now:continue
   if not same(record,now):raise RuntimeError('Owned process identity changed')
   try:
    fields={a.split(':',1)[0]:a.split(':',1)[1].strip()for a in Path('/proc',str(pid),'status').read_text().splitlines()if a.startswith(('VmRSS:','VmPeak:'))}
    rss+=int(fields.get('VmRSS','0 kB').split()[0])*1024;self.peak_virtual=max(self.peak_virtual,int(fields.get('VmPeak','0 kB').split()[0])*1024)
    children=set()
    for task in Path('/proc',str(pid),'task').iterdir():
     try:children.update(map(int,(task/'children').read_text().split()))
     except(FileNotFoundError,ProcessLookupError):pass
   except(FileNotFoundError,ProcessLookupError):continue
   # A parent disappearing during enumeration grants no new child identity.
   if not same(record,identity(pid)):continue
   for child in children:
    current=identity(child)
    if not current or current['ppid']!=pid:continue
    if child in self.known:
     if not same(self.known[child],current):raise RuntimeError('Child identity reused')
    else:self.known[child]=current
    todo.append(child)
  self.samples+=1;self.peak_rss=max(self.peak_rss,rss);return rss
 def terminate(self,reason):
  # Only groups containing a currently verified process from our proven tree.
  for sig in [signal.SIGTERM,signal.SIGKILL]:
   groups={}
   for pid,record in self.known.items():
    now=identity(pid)
    if same(record,now)and now['state']!='Z':groups.setdefault(now['pgrp'],record)
   for pgid,witness in groups.items():
    if pgid<=1 or pgid==os.getpgrp():raise RuntimeError('Refuse driver/unowned process group')
    if not same(witness,identity(witness['pid'])):continue
    self.signals.append({'pgid':pgid,'witness':witness,'signal':sig.name,'reason':reason,'epoch':time.time()})
    try:os.killpg(pgid,sig)
    except ProcessLookupError:pass
   end=time.monotonic()+.5
   while time.monotonic()<end:
    if all(not alive(v)for v in self.known.values()):break
    time.sleep(.01)
  self.proc.wait(timeout=2)
 def result(self):return {'root':self.root,'known_processes':list(self.known.values()),'peak_owned_RSS_bytes':self.peak_rss,'peak_observed_virtual_bytes':self.peak_virtual,'monitor_samples':self.samples,'signals':self.signals}

def run(command,*,cwd,env,log,timeout=900,max_rss=8*1024**3,progress=None):
 row={'command':command,'started':time.time(),'timeout_seconds':timeout,'RSS_limit_bytes':max_rss,'virtual_AS_cap':None};owned=None
 with Path(log).open('w')as stream:
  proc=subprocess.Popen(command,cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT,start_new_session=True,preexec_fn=no_core)
  try:
   owned=Owned(proc);row.update(pid=proc.pid,start=owned.root['start'],pgrp=owned.root['pgrp']);deadline=time.monotonic()+timeout;last=0
   while proc.poll()is None:
    rss=owned.refresh();now=time.monotonic();row.update(owned.result())
    if progress and now-last>.1:progress(row.copy());last=now
    if rss>max_rss:raise BudgetExceeded('Owned aggregate RSS exceeded declared budget')
    if now>deadline:raise BudgetExceeded('Owned command exceeded declared deadline')
    time.sleep(.01)
   row['exit']=proc.wait();owned.refresh();row.update(owned.result())
   live=[v for v in owned.known.values()if alive(v)]
   if live:raise RuntimeError('Owned children survived product exit')
  except BaseException as error:
   row['error']=repr(error)
   if owned:
    owned.terminate(str(error));row.update(owned.result());row['exit']=proc.returncode
   else:
    # No identity certificate: never signal a guessed/reused PID or group.
    row['identity_uncertified']=True
  row['seconds']=time.time()-row['started']
 return row
