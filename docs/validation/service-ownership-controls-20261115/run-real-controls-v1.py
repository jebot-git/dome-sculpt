"""Launch ONLY new bounded user units and real CPU processes for primitive proof."""
from pathlib import Path
import json,subprocess,sys,time,hashlib,uuid
from ownership_primitives_v1 import proc_identity
P=Path(__file__).resolve().parent;RUN=P/('real-cpu-v1-'+uuid.uuid4().hex[:8]);RUN.mkdir();nonce=RUN.name.rsplit('-',1)[1];units={};results=[]
def properties(unit):
 r=subprocess.run(['systemctl','--user','show',unit,'--property=Id,LoadState,ActiveState,SubState,MainPID,ExecMainPID,ExecMainStatus,ExecMainCode,InvocationID,Result,ControlGroup,MemoryMax,RuntimeMaxUSec,Type,KillMode,RemainAfterExit'],capture_output=True,text=True)
 return dict(line.split('=',1)for line in r.stdout.splitlines()if '='in line)
def own(unit):
 assert unit in units and unit.startswith('ds-owner-control-')and unit.endswith('.service')
 p=properties(unit);assert p['InvocationID']==units[unit]['InvocationID'];return p
def start(unit,argv,memory):
 assert properties(unit).get('LoadState','not-found')=='not-found'
 cmd=['systemd-run','--user','--unit='+unit,'--service-type=exec','--property=RuntimeMaxSec=45','--property=TimeoutStopSec=10','--property=MemoryMax='+str(memory),'--property=KillMode=control-group','--property=Nice=15','--property=RemainAfterExit=yes','--working-directory='+str(P),'--property=StandardOutput=append:'+str(RUN/(unit+'.log')),'--property=StandardError=append:'+str(RUN/(unit+'.log')),*argv]
 r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,(unit,r.stderr);p=properties(unit);assert p['Type']=='exec'and p['KillMode']=='control-group'and p['MemoryMax']==str(memory);units[unit]={'InvocationID':p['InvocationID'],'launch_properties':p,'command':cmd};return p
def wait_file(path,until):
 while not path.is_file():
  assert time.monotonic()<until,('Missing real CPU readiness',str(path));time.sleep(.02)
def sentinel_alive():
 p=own(sentinel);assert p['SubState']=='running'and p['MainPID']==str(sentinel_pid);now=proc_identity(sentinel_pid);assert now and now['start']==sentinel_id['start'];hb=json.loads((RUN/'sentinel/sentinel-heartbeat.json').read_text());assert hb['start']==now['start']and hb['cgroup'].endswith('/'+sentinel);assert time.time()-hb['epoch']<3;return {'unit':sentinel,'properties':p,'identity':now,'heartbeat':hb}
def stop(unit):
 p=own(unit);subprocess.run(['systemctl','--user','stop',unit],check=True);return p
sentinel='ds-owner-control-sentinel-'+nonce+'.service';p=start(sentinel,['/usr/bin/python3',str(P/'cpu_tree_v1.py'),'--role','sentinel','--out',str(RUN/'sentinel'),'--mode','healthy'],64*2**20);sentinel_pid=int(p['MainPID']);sentinel_id=proc_identity(sentinel_pid);assert sentinel_id;wait_file(RUN/'sentinel/sentinel-heartbeat.json',time.monotonic()+5);initial_sentinel=sentinel_alive()
try:
 for mode,expected in [('healthy',0),('orphan',0),('term',143),('deadline',124),('rss',1)]:
  unit='ds-owner-control-'+mode+'-'+nonce+'.service';out=RUN/mode;p=start(unit,['/usr/bin/python3',str(P/'primitive_control_v1.py'),'--mode',mode,'--unit',unit,'--out',str(out)],256*2**20)
  mainpid=int(p['MainPID']);main_identity=proc_identity(mainpid);assert main_identity
  if mode=='term':
   wait_file(out/'ready.json',time.monotonic()+5);ready=json.loads((out/'ready.json').read_text());before=own(unit);now=proc_identity(mainpid);assert now and now['start']==main_identity['start']==ready['main']['start']and before['MainPID']==str(mainpid)
   subprocess.run(['systemctl','--user','kill','--kill-whom=main','--signal=TERM',unit],check=True)
  until=time.monotonic()+18
  while True:
   p=own(unit)
   if p['MainPID']=='0'and p['SubState']in['exited','failed','dead']:break
   assert time.monotonic()<until,('Owned control did not terminate',mode,p);sentinel_alive();time.sleep(.04)
  assert int(p['ExecMainStatus'])==expected,(mode,p)
  state=json.loads((out/'status.json').read_text());assert state['state']=='terminal'and state['exit']==expected and not state['remaining_owned_same_start'];assert state['cgroup_memory_events'].split('oom_kill ')[1].splitlines()[0]=='0'
  if mode=='rss':assert state['peak_aggregate_RSS_bytes']>state['declared_RSS_limit_bytes']
  if mode=='term':assert state['actual_signal']==15
  if mode=='deadline':assert 'monotonic deadline'in state['error']
  if mode=='orphan':assert state['leader_returncode']==0 and state['orphan_before_cleanup']
  if mode!='healthy':assert state['cleanup']and not state['cleanup']['remaining_owned_same_start']
  journal=subprocess.check_output(['journalctl','--user','-u',unit,'--no-pager','-o','short-iso'],text=True);(out/'unit-journal.log').write_text(journal);(out/'terminal-properties.json').write_text(json.dumps(p,indent=2)+'\n')
  sentinelproof=sentinel_alive();assert sentinelproof['heartbeat']['cgroup']!=json.loads((out/'tree/leader.json').read_text())['cgroup'];terminal=stop(unit)
  survivors=[];deadline=time.monotonic()+2
  tracked=[int(k.split(':')[0])for k in state['process_ledger']]+[mainpid]
  while True:
   survivors=[pid for pid in tracked if proc_identity(pid)is not None]
   if not survivors or time.monotonic()>=deadline:break
   time.sleep(.03)
  assert not survivors,('Owned process IDs remain after unit stop',mode,survivors)
  results.append({'mode':mode,'expected_actual_exit':expected,'actual_properties':terminal,'state':state,'sentinel_survived':sentinelproof,'all_owned_PIDs_gone':tracked,'success_as_deliberate_control':True})
 final_sentinel=sentinel_alive();outcome={'state':'terminal','exit':0,'scope':'Real isolated CPU ownership/resource primitives only. Deliberate deadline124/TERM143/RSS1 remain actual expected negative control outcomes. NOT compiler/project/mixed-executor qualification. No .o/.d or Ninja/build journal created.','extraction_receipt':json.loads((P/'extraction-receipt-v1.json').read_text()),'initial_sentinel':initial_sentinel,'final_sentinel':final_sentinel,'controls':results};(RUN/'status.json').write_text(json.dumps(outcome,indent=2)+'\n');print(json.dumps({'state':'terminal','exit':0,'run':str(RUN),'controls':[(r['mode'],r['expected_actual_exit'])for r in results]},indent=2))
finally:
 for unit in reversed(list(units)):
  p=properties(unit)
  if p.get('LoadState')=='loaded'and p.get('InvocationID')==units[unit]['InvocationID']:
   subprocess.run(['systemctl','--user','stop',unit],check=True)
   if p.get('SubState')=='failed':subprocess.run(['systemctl','--user','reset-failed',unit],check=True)
 (RUN/'launches.json').write_text(json.dumps(units,indent=2)+'\n')
