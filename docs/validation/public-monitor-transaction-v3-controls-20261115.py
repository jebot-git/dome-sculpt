"""CPU-only unchanged-monitor retirement and expected-ELF witness controls."""
import errno,hashlib,importlib.util,json,subprocess,time,traceback
from pathlib import Path
R=Path('/home/blux/de-xr/dome-sculpt');P=Path(__file__).resolve().parent;OUT=P/'transaction-controls-v1';OUT.mkdir();C=R/'artifacts/strict-arc-20261111/control'
def load(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
g=load('transaction_real_monitor',C/'graphics-v3.py');repair=load('transaction_adapter',P/'monitor_lifetime_v3.py');covering=g.current_identity.__globals__['covering'];rows=[];children=[];started=time.monotonic()
consumer=Path('/usr/bin/sleep').resolve();libc=Path('/usr/lib64/libc.so.6').resolve();critical={};stats={};expectations={}
for name,path in [('consumer',consumer),('libc',libc)]:
 c=g.current_identity(path);critical[name]=(c['map_device'],c['inode']);stats[name]=(c['stat_device'],c['inode']);expectations[name]={'path':str(path),'sha256':sha(path)}
pins={str(f):sha(f)for f in [Path(__file__),P/'monitor_lifetime_v3.py',P/'monitor_lifetime_v2.py',C/'graphics-v3.py',C/'mount_identity.py',consumer,libc]}
def child():
 p=subprocess.Popen([str(consumer),'10'],start_new_session=True);children.append(p);deadline=time.monotonic()+2
 while time.monotonic()<deadline:
  ident=g.identity(p.pid)
  if ident and str(libc)in Path('/proc',str(p.pid),'maps').read_text():return p,ident
  time.sleep(.001)
 raise AssertionError('Actual owned child did not load libc')
def retire(p,reap):
 p.terminate();deadline=time.monotonic()+1
 while time.monotonic()<deadline:
  ident=g.identity(p.pid)
  if ident and ident['state']=='Z':break
 else:raise AssertionError('Owned child did not become zombie')
 if reap:p.wait(timeout=1)
 return g.identity(p.pid)
try:
 for mode in ['live-complete','ENOENT-first','ENOENT-after-consumer','EINVAL-first','EINVAL-after-consumer','EINVAL-first-reaped-at-classifier','wrong-expected-sha','wrong-expected-path','prior-success-then-ENOENT']:
  p,original=child();owned={};seen=set();mapped={};stage=OUT/mode;stage.mkdir();record={'mode':mode,'pid':p.pid,'expected':original};dris={};triggered=[False];expected={k:dict(v)for k,v in expectations.items()}
  if mode=='prior-success-then-ENOENT':
   repair.checked_monitor(g.monitor,g.identity,p,original,owned,critical,stats,seen,mapped,dris,stage,OUT/'observations.jsonl',expected)
   assert seen==set(critical);record['prior_certified']=sorted(seen)
   # Add one uncached real mapping to force another actual certification read.
   for path in list(mapped):
    if Path(path).resolve()not in [consumer,libc]:del mapped[path]
  prior_seen=set(seen)
  if mode=='wrong-expected-sha':expected['consumer']['sha256']='0'*64
  if mode=='wrong-expected-path':expected['consumer']['path']=str(libc)
  def controlled_covering(pid,path):
   target=(mode.endswith('after-consumer')and Path(path).resolve()==libc)or(mode in ['ENOENT-first','EINVAL-first','EINVAL-first-reaped-at-classifier','prior-success-then-ENOENT'])
   if str(pid)==str(p.pid)and target and not triggered[0]:
    triggered[0]=True;record['seen_before_actual_read']=sorted(seen);record['completed_fingerprints_before_actual_read']=list(mapped);record['retired_identity']=retire(p,mode.startswith('ENOENT')or mode=='prior-success-then-ENOENT')
   try:return covering(pid,path)
   except OSError as e:
    record.update(actual_errno=e.errno,actual_filename=e.filename,actual_stack=traceback.format_exc())
    raise
  g.current_identity.__globals__['covering']=controlled_covering
  def actual_monitor(*args):
   try:return g.monitor(*args)
   except OSError:
    if mode=='EINVAL-first-reaped-at-classifier':record['reaped_before_classifier_exit']=p.wait(timeout=1)
    raise
  refused=False
  try:repair.checked_monitor(actual_monitor,g.identity,p,original,owned,critical,stats,seen,mapped,dris,stage,OUT/'observations.jsonl',expected)
  except RuntimeError as e:
   assert mode in ['wrong-expected-sha','wrong-expected-path'];refused=True;record['expected_refusal']=repr(e)
  if mode=='live-complete':assert seen==set(critical)and all(any(fp['sha256']==spec['sha256']for fp in mapped.values())for spec in expectations.values())
  elif mode in ['ENOENT-first','EINVAL-first','EINVAL-first-reaped-at-classifier']:assert triggered[0]and seen==set()and not mapped
  elif mode in ['ENOENT-after-consumer','EINVAL-after-consumer']:assert triggered[0]and seen=={'consumer'}and any(Path(path).resolve()==consumer for path in mapped)and all(Path(path).resolve()!=libc for path in mapped)
  elif mode in ['wrong-expected-sha','wrong-expected-path']:assert refused and seen==prior_seen==set()
  else:assert triggered[0]and seen==prior_seen==set(critical)
  if mode.startswith('ENOENT')or mode=='prior-success-then-ENOENT':assert record['actual_errno']==errno.ENOENT and record['actual_filename']==f'/proc/{p.pid}/mountinfo'
  if mode.startswith('EINVAL'):assert record['actual_errno']==errno.EINVAL and record['actual_filename']==f'/proc/{p.pid}/mountinfo'
  record.update(passed=True,seen_after=sorted(seen),mapped_after={path:fp['sha256']for path,fp in mapped.items()});rows.append(record);g.current_identity.__globals__['covering']=covering
  if p.poll()is None:p.terminate()
  p.wait(timeout=1)
 for mode in ['live-EINVAL','wrong-path','wrong-errno','wrong-original-start','wrong-post-start','unknown-owner']:
  p,original=child();owned={p.pid:dict(original)};expected=dict(original);identity=g.identity;error=OSError(errno.EINVAL,'Invalid argument',f'/proc/{p.pid}/mountinfo');seen={'prior-certified'}
  if mode=='wrong-path':retire(p,False);error=OSError(errno.EINVAL,'Invalid argument',f'/proc/{p.pid}/maps')
  if mode=='wrong-errno':retire(p,False);error=OSError(errno.EIO,'Input/output error',f'/proc/{p.pid}/mountinfo')
  if mode=='wrong-original-start':retire(p,False);expected['start']+=1
  if mode=='wrong-post-start':retire(p,False);identity=lambda n:{**g.identity(n),'start':original['start']+1}
  if mode=='unknown-owner':retire(p,False);owned={}
  def failing(*args):raise error
  refused=False
  try:repair.checked_monitor(failing,identity,p,expected,owned,critical,stats,seen,{}, {},OUT,OUT/'observations.jsonl',expectations)
  except OSError as e:refused=e is error
  assert refused and seen=={'prior-certified'};rows.append({'mode':mode,'passed':True,'original_error_propagated':True,'prior_seen_unchanged':True})
  if p.poll()is None:p.terminate()
  p.wait(timeout=1)
 state={'state':'terminal','exit':0,'scope':__doc__,'controls':rows,'all_owned_children_reaped':all(p.poll()is not None for p in children),'pins':pins,'input_drift':[p for p,h in pins.items()if sha(p)!=h],'seconds':time.monotonic()-started};assert not state['input_drift'];(OUT/'status.json').write_text(json.dumps(state,indent=2)+'\n');print('TERMINAL0',len(rows),'CPU controls; all children reaped',state['seconds'])
finally:
 g.current_identity.__globals__['covering']=covering
 for p in children:
  if p.poll()is None:p.kill()
  p.wait(timeout=1)
