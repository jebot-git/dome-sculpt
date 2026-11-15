"""CPU-only genuine /proc ENOENT at certification, using the unchanged monitor."""
import hashlib,importlib.util,json,subprocess,time,traceback
from pathlib import Path
R=Path('/home/blux/de-xr/dome-sculpt');P=Path(__file__).resolve().parent;OUT=P/'enoent-counterfactual-v1';OUT.mkdir();C=R/'artifacts/strict-arc-20261111/control';V2=R/'artifacts/baked-emission-source8-public-ad99-20261112/monitor_lifetime_v2.py'
def load(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
g=load('unmodified_monitor',C/'graphics-v3.py');v2=load('unchanged_v2_adapter',V2);original_covering=g.current_identity.__globals__['covering'];rows=[];children=[];pins={str(f):sha(f)for f in [Path(__file__),C/'graphics-v3.py',C/'mount_identity.py',V2,Path('/usr/bin/sleep')]}
try:
 for mode in ['old-ENOENT-counterfactual','v2-ENOENT-counterfactual']:
  child=subprocess.Popen(['/usr/bin/sleep','10'],start_new_session=True);children.append(child);expected=g.identity(child.pid);assert expected
  cert=g.current_identity('/usr/bin/sleep');critical={'consumer':(cert['map_device'],cert['inode'])};stats={'consumer':(cert['stat_device'],cert['inode'])};seen=set();mapped={};owned={};stage=OUT/mode;stage.mkdir();record={'mode':mode,'pid':child.pid,'expected':expected};trigger=[False]
  def exiting_covering(pid,path):
   if str(pid)==str(child.pid)and not trigger[0]:
    trigger[0]=True;record['seen_at_certification']=sorted(seen);record['mapped_at_certification']=list(mapped);child.terminate();record['actual_wait_exit']=child.wait(timeout=1);record['identity_after_reap']=g.identity(child.pid)
   try:return original_covering(pid,path)
   except FileNotFoundError as e:record.update(actual_errno=e.errno,actual_filename=e.filename,actual_stack=traceback.format_exc());raise
  g.current_identity.__globals__['covering']=exiting_covering
  if mode=='old-ENOENT-counterfactual':result=g.monitor(child,owned,critical,stats,seen,mapped,{},stage)
  else:result=v2.checked_monitor(g.monitor,g.identity,child,expected,owned,critical,stats,seen,mapped,{},stage,OUT/'v2-races.jsonl')
  assert trigger[0]and record['actual_errno']==2 and record['actual_filename']==f'/proc/{child.pid}/mountinfo'
  assert record['identity_after_reap']is None and seen=={'consumer'}and mapped=={}
  record.update(return_value=result,seen_after=sorted(seen),mapped_after=mapped,uncertified_critical_witness_promoted=True,exception_escaped=False);rows.append(record)
  g.current_identity.__globals__['covering']=original_covering
 state={'state':'terminal','exit':0,'scope':__doc__,'rows':rows,'input_drift':[p for p,h in pins.items()if sha(p)!=h],'pins':pins,'all_owned_children_reaped':all(p.poll()is not None for p in children)};assert not state['input_drift'];(OUT/'status.json').write_text(json.dumps(state,indent=2)+'\n');print('TERMINAL0 genuine old and v2 swallowed ENOENT both promote uncertified witness; all children reaped')
finally:
 g.current_identity.__globals__['covering']=original_covering
 for child in children:
  if child.poll()is None:child.kill()
  child.wait(timeout=1)
