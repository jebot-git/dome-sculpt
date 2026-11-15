"""Exact extracted ownership/resource primitives for isolated real CPU controls.
Not imported by any held executor; no whole-mixed-executor qualification claim.
Only RSS threshold and deadline error label are explicitly parameterized below.
"""
import os,signal,subprocess,time,stat,json
from pathlib import Path
class OwnedTermination(BaseException):
 def __init__(self,signum):self.signum=signum;super().__init__('Received owned executor signal '+str(signum))

def received_signal(signum,frame):raise OwnedTermination(signum)

def proc_identity(pid):
 try:
  text=(Path('/proc')/str(pid)/'stat').read_text();a=text[text.rfind(')')+2:].split();return {'start':int(a[19]),'pgrp':int(a[2]),'session':int(a[3]),'ppid':int(a[1]),'state':a[0]}
 except(FileNotFoundError,ProcessLookupError):return None

def file_identity(path):
 s=Path(path).stat();return {'dev':s.st_dev,'ino':s.st_ino,'bytes':s.st_size,'mode':stat.S_IMODE(s.st_mode),'mtime_ns':s.st_mtime_ns,'ctime_ns':s.st_ctime_ns}

def atomic(path,value):
 temp=path.with_suffix(path.suffix+'.writing');temp.write_text(json.dumps(value,indent=2)+'\n');temp.replace(path)

def descendants(pid):
 todo=[pid];found=[];seen=set()
 while todo:
  pid=todo.pop()
  if pid in seen:continue
  seen.add(pid);found.append(pid)
  try:todo+=list(map(int,(Path('/proc')/str(pid)/'task'/str(pid)/'children').read_text().split()))
  except(FileNotFoundError,ProcessLookupError):pass
 return found

def make_primitives(*,cgroup_root,own_cgroup,active,owned,ledger,state,record,deadline,rss_limit):
 assert own_cgroup.startswith('/user.slice/') and own_cgroup.endswith('.service')
 assert Path(cgroup_root).resolve()==(Path('/sys/fs/cgroup')/own_cgroup.lstrip('/')).resolve(), 'Declared ownership binding differs from actual dedicated cgroup root'
 peak=0;next_sample=0
 def service_pids():
  pids=set()
  for path in[cgroup_root,*[x for x in cgroup_root.rglob('*')if x.is_dir()]]:
   try:pids.update(map(int,(path/'cgroup.procs').read_text().split()))
   except FileNotFoundError:continue
  return pids

 def observe():
  live={}
  # The reviewed dedicated cgroup, not the longevity of launch leaders, defines ownership.
  for pid in service_pids()-{os.getpid()}:
   identity=proc_identity(pid)
   try:membership=(Path('/proc')/str(pid)/'cgroup').read_text().splitlines()
   except(FileNotFoundError,ProcessLookupError):continue
   if identity is None or not any(x=='0::'+own_cgroup or x.startswith('0::'+own_cgroup+'/')for x in membership):continue
   if pid not in owned or owned[pid]['identity']['start']!=identity['start']:
    root=identity['session']if identity['session']in active else None
    row={'identity':identity,'owning_job_root':root,'observed_epoch':time.time(),'authority':'dedicated reviewed current service cgroup'}
    owned[pid]=row;ledger[str(pid)+':'+str(identity['start'])]=row;record({'event':'process-observed','pid':pid,**row})
   if identity['state']!='Z':live[pid]=identity
  for pid in list(owned):
   current=proc_identity(pid)
   if current is None or current['start']!=owned[pid]['identity']['start']:del owned[pid]
  return live

 def sample():
  nonlocal peak
  live=observe();rss=0
  for pid in{os.getpid(),*live}:
   try:
    before=proc_identity(pid)
    if before is None:continue
    wanted=live[pid]['start']if pid in live else before['start'];value=0
    if before['start']!=wanted:continue
    for line in(Path('/proc')/str(pid)/'status').read_text().splitlines():
     if line.startswith('VmRSS:'):value=int(line.split()[1])*1024
    after=proc_identity(pid)
    if after and after['start']==wanted:rss+=value
   except(FileNotFoundError,ProcessLookupError):pass
  peak=max(peak,rss);assert peak<=rss_limit,'Owned control aggregateRSS exceeds declared test limit';return live

 def checkpoint():
  nonlocal next_sample
  now=time.monotonic()
  if now>deadline:raise TimeoutError('Declared control monotonic deadline exceeded')
  if now>=next_sample:sample();next_sample=now+.2

 def cleanup():
  signal.signal(signal.SIGTERM,signal.SIG_IGN);signal.signal(signal.SIGINT,signal.SIG_IGN);events=[]
  for sig in[signal.SIGTERM,signal.SIGKILL]:
   live=observe();groups=set()
   for pid,identity in live.items():
    root=owned[pid]['owning_job_root']
    if identity['session']==identity['pgrp']==root:groups.add(root)
   for group in groups:
    try:os.killpg(group,sig);events.append({'pgrp':group,'signal':sig})
    except ProcessLookupError:pass
   for pid,identity in live.items():
    if identity['pgrp']in groups:continue
    current=proc_identity(pid)
    if current and current['start']==owned[pid]['identity']['start']:
     try:os.kill(pid,sig);events.append({'pid':pid,'start':current['start'],'signal':sig})
     except ProcessLookupError:pass
   until=time.monotonic()+2
   while time.monotonic()<until:
    for task in active.values():task['process'].poll()
    if not observe():break
    time.sleep(.05)
  for pid,task in active.items():
   try:task['process'].wait(timeout=.1)
   except subprocess.TimeoutExpired:pass
   task['stream'].close();events.append({'pid':pid,'key':task['key'],'actual_cleanup_returncode':task['process'].returncode})
  remaining=observe();state['cleanup']={'signals':events,'remaining_owned_same_start':remaining};return remaining

 return {'service_pids':service_pids,'observe':observe,'sample':sample,'checkpoint':checkpoint,'cleanup':cleanup,'peak':lambda:peak}
