"""Isolated real process controls using extracted mixed-executor primitives."""
from pathlib import Path
import argparse,os,signal,subprocess,time,json,sys
from ownership_primitives_v1 import proc_identity,atomic,make_primitives,OwnedTermination,received_signal
P=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--mode',choices=['healthy','orphan','term','deadline','rss'],required=True);parser.add_argument('--unit',required=True);parser.add_argument('--out',type=Path,required=True);a=parser.parse_args();assert a.unit.startswith('ds-owner-control-')and a.unit.endswith('.service')and '/'not in a.unit;a.out.mkdir(exist_ok=False,parents=True)
props=subprocess.check_output(['systemctl','--user','show',a.unit,'--property=MainPID,ExecMainPID,InvocationID,KillMode,Type,MemoryMax,RuntimeMaxUSec,TimeoutStopUSec'],text=True);props=dict(x.split('=',1)for x in props.splitlines());assert props['MainPID']==props['ExecMainPID']==str(os.getpid())and props['Type']=='exec'and props['KillMode']=='control-group'and props['MemoryMax']==str(256*2**20)
cg=Path('/proc/self/cgroup').read_text().strip();assert cg.startswith('0::/user.slice/')and cg.endswith('/'+a.unit);cgroup_root=Path('/sys/fs/cgroup')/cg[3:].lstrip('/');assert(cgroup_root/'memory.max').read_text().strip()==str(256*2**20)
active={};owned={};ledger={};state={'mode':a.mode,'unit':a.unit,'properties':props,'scope':'Real CPU ownership/resource primitives ONLY; not project executor/build/runtime qualification','state':'running','exit':None,'error':None,'actual_signal':None,'cleanup':None};events=[]
def record(value):events.append(value);atomic(a.out/'process-events.json',events)
started=time.monotonic();deadline=started+(1.6 if a.mode=='deadline'else 15);rss_limit=64*2**20 if a.mode=='rss'else 192*2**20
pr=make_primitives(cgroup_root=cgroup_root,active=active,owned=owned,ledger=ledger,state=state,record=record,deadline=deadline,rss_limit=rss_limit)
assert pr['service_pids']()=={os.getpid()};signal.signal(signal.SIGTERM,received_signal);signal.signal(signal.SIGINT,received_signal)
stream=(a.out/'child-output.log').open('w');child=subprocess.Popen(['/usr/bin/python3',str(P/'cpu_tree_v1.py'),'--role','leader','--out',str(a.out/'tree'),'--mode',a.mode],stdout=stream,stderr=subprocess.STDOUT,start_new_session=True);identity=proc_identity(child.pid);assert identity and identity['pgrp']==identity['session']==child.pid;active[child.pid]={'process':child,'identity':identity,'key':'REAL_CPU_TREE_CONTROL','kind':'CPU_CONTROL','stream':stream,'log':a.out/'child-output.log','began':started}
try:
 ready=False
 while True:
  pr['checkpoint']()
  if(a.out/'tree/grandchild.json').exists()and not ready:
   pr['sample']();ready=True;state['ready_identity']=proc_identity(os.getpid());atomic(a.out/'ready.json',{'unit':a.unit,'invocation':props['InvocationID'],'main':state['ready_identity'],'owned_live':pr['observe'](),'cgroup':cg})
  code=child.poll()
  if code is not None:
   state['leader_returncode']=code;stream.close();del active[child.pid]
   live=pr['observe']()
   if a.mode=='healthy':
    if not live:state['exit']=0;break
   elif a.mode=='orphan':
    assert ready and code==0 and live,'Orphan control needs actual surviving grandchild after actual leader exit'
    state['orphan_before_cleanup']=live;assert not pr['cleanup']();state['exit']=0;break
  if time.monotonic()-started>10:raise RuntimeError('Control setup/run exceeded independent bounded expectation')
  time.sleep(.02)
except OwnedTermination as error:
 state.update(exit=128+error.signum,actual_signal=error.signum,error=repr(error));pr['cleanup']()
except TimeoutError as error:
 state.update(exit=124,error=repr(error),control_outcome='Expected real monotonic expiry;124 is control mapping, not a whole-executor result');pr['cleanup']()
except AssertionError as error:
 state.update(exit=1,error=repr(error),control_outcome='Expected RSS primitive assertion only if real peak exceeds limit');pr['cleanup']()
except BaseException as error:
 state.update(exit=70,error=repr(error));pr['cleanup']()
finally:
 if active and state['exit']==0:state.update(exit=70,error='Unexpected active root at successful control');pr['cleanup']()
 state.update(state='terminal',seconds=time.monotonic()-started,peak_aggregate_RSS_bytes=pr['peak'](),declared_RSS_limit_bytes=rss_limit,remaining_owned_same_start=pr['observe'](),process_ledger=ledger,cgroup_memory_events=(cgroup_root/'memory.events').read_text())
 if state['remaining_owned_same_start']:state['exit']=71
 atomic(a.out/'status.json',state)
raise SystemExit(state['exit'])
