"""Materialize the audited generated/complete-TU/owning-link DAG. Plan by default.
Never runs built products, CTest, GUI or GPU. Existing provider files are read-only.
"""
import argparse,concurrent.futures,json,os,resource,shutil,signal,subprocess,time
from pathlib import Path
from ninja_audit import sha,archive_members
P=Path(__file__).resolve().parent

def identity(pid):
 try:
  value=(Path('/proc')/str(pid)/'stat').read_text();fields=value[value.rfind(')')+2:].split();return{'start':int(fields[19]),'pgrp':int(fields[2]),'ppid':int(fields[1]),'state':fields[0]}
 except(FileNotFoundError,ProcessLookupError):return None
def drift(pins,seconds=600):
 deadline=time.monotonic()+seconds;out=[]
 for path,value in pins.items():
  if time.monotonic()>deadline:out.append('Declared600s immutable-input audit bound exceeded');break
  if not Path(path).is_file()or sha(path)!=value:out.append(path)
 return out
def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def atomic(path,value):
 temp=path.with_suffix('.writing');temp.write_text(json.dumps(value,indent=2)+'\n');temp.replace(path)
def descendants(pid):
 found=[];todo=[pid];seen=set()
 while todo:
  current=todo.pop()
  if current in seen:continue
  seen.add(current);found.append(current)
  try:todo+=list(map(int,(Path('/proc')/str(current)/'task'/str(current)/'children').read_text().split()))
  except(FileNotFoundError,ProcessLookupError):pass
 return found

def execute(plan_file):
 assert not(plan_file.parent/'EXECUTION-HOLD.json').exists(),'Frozen audit has an explicit execution hold; use a newly frozen/recomputed plan'
 plan=json.loads(plan_file.read_text());assert plan['state']=='audited-NOT-compiled'and plan['profile']=='sanitize'
 dest=Path(plan['destination']);source=Path(plan['source_path']);head=plan['source'];out=plan_file.parent/'execution-v1';out.mkdir()
 assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==head
 assert not subprocess.check_output(['git','status','--porcelain'],cwd=source,text=True).strip()
 inputs=dict(plan['pins']);inputs[str(plan_file)]=sha(plan_file);inputs[str(Path(__file__).resolve())]=sha(__file__);inputs[str(P/'ninja_audit.py')]=sha(P/'ninja_audit.py');inputs[str(P/'dependency_audit_v5.py')]=sha(P/'dependency_audit_v5.py');assert not drift(inputs)
 atomic(out/'inputs.json',inputs);inputs_sha=sha(out/'inputs.json')
 env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))}
 for key in ['LD_PRELOAD','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS']:env.pop(key,None)
 sdk=P.parents[1]/'artifacts/sdk-20261111/toolkit-sanitize/lib';env.update(LD_LIBRARY_PATH=str(sdk)+':/opt/fltk-vr/bdf-runtime/lib:/usr/lib64',LP_NUM_THREADS='2',OMP_NUM_THREADS='2',OPENBLAS_NUM_THREADS='2',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 state={'state':'preparing','source':head,'profile':'sanitize','scope':'Independent bounded current strictSAN completion after original actual143 interruption;538 proven complete whole current objects staged,494 TUs fresh,all733 links and3generators fresh. Not original143 pass or fresh single invocation; no product runtime.','resource_bounds':{'independent_owning_seconds':18000,'max_active_tasks':2,'aggregate_descendant_RSS_bytes':8*2**30,'core_bytes':0,'initial_input_audit_seconds':600,'final_input_audit_seconds':600},'strict_environment':{key:env.get(key)for key in ['LD_LIBRARY_PATH','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','LD_PRELOAD','LP_NUM_THREADS','OMP_NUM_THREADS','OPENBLAS_NUM_THREADS']},'plan':{'path':str(plan_file),'sha256':sha(plan_file)},'inputs':{'path':str(out/'inputs.json'),'sha256':inputs_sha},'summary':plan['summary'],'completed_compilations':0,'completed_links':0,'completed_generators':0,'active':{},'steps_journal':str(out/'steps.jsonl'),'exit':1}
 active={};completed=set();copied={};products={};started=time.monotonic();deadline=started+18000;peak_rss=0
 def save():atomic(out/'status.json',state)
 def record(value):
  with(out/'steps.jsonl').open('a')as stream:stream.write(json.dumps(value)+'\n')
 def output_path(key):
  path=dest/key;assert path.resolve().is_relative_to(dest.resolve()),('Output escaped owning tree',key);path.parent.mkdir(parents=True,exist_ok=True);assert not path.is_symlink();return path
 def copy(key,row):
  target=output_path(key);assert not target.exists(),('Configured output already exists before its sole owning step',key)
  provider=Path(row['provider']);assert provider.is_file()and sha(provider)==row['provider_sha256'];shutil.copy2(provider,target);assert sha(target)==row['provider_sha256'];dep_source=Path(row['provider_depfile']);dep_target=dest/row['depfile'];assert sha(dep_source)==row['provider_depfile_sha256'];assert not dep_target.exists();shutil.copy2(dep_source,dep_target);assert sha(dep_target)==row['provider_depfile_sha256'];dep_target.chmod(0o444);target.chmod(0o444)
  copied[key]={'actual_current_depfile_provider':str(dep_source),'actual_current_depfile_snapshot':str(dep_target),'actual_current_depfile_sha256':row['provider_depfile_sha256'],'provider':str(provider),'sha256':row['provider_sha256'],'whole_file_byteexact':True}
 def kill_owned():
  for sig in [signal.SIGTERM,signal.SIGKILL]:
   for pid,value in list(active.items()):
    now=identity(pid)
    if now and now['start']==value['identity']['start']and now['pgrp']==pid:
     try:os.killpg(pid,sig)
     except ProcessLookupError:pass
   for value in active.values():
    try:value['process'].wait(timeout=1)
    except subprocess.TimeoutExpired:pass
 def launch(key,row,kind):
  target=output_path(key);assert not target.exists(),('Refuse overwrite/partial TU reuse',key)
  argv=['nice','-n','10',*row['command']]if kind=='compile'else['nice','-n','10','/bin/bash','-c',row['command']]
  # Commands are the exact actual configured complete-TU and final-link
  # commands. The only extra argv is scheduling priority; no source rewrite.
  log=out/(kind+'-'+key.replace('/','_')+'.log');stream=log.open('w');begin=time.monotonic();child=subprocess.Popen(argv,cwd=dest,env=env,stdout=stream,stderr=subprocess.STDOUT,start_new_session=True,preexec_fn=no_core);ident=identity(child.pid);assert ident and ident['pgrp']==child.pid
  active[child.pid]={'process':child,'identity':ident,'key':key,'kind':kind,'argv':argv,'log':log,'stream':stream,'began':begin};record({'event':'launch','kind':kind,'key':key,'pid':child.pid,'identity':ident,'argv':argv,'cwd':str(dest),'log':str(log),'epoch':time.time()})
 def schedule():
  nonlocal peak_rss
  pending={}
  for key,row in plan['generated_outputs'].items():pending[key]=(row,'generate')
  for key,row in plan['objects'].items():
   if row['route']=='compile-complete-TU':pending[key]=(row,'compile')
  for key,row in plan['links'].items():
   if row['route']=='fresh-current-link':pending[key]=(row,'link')
  while pending or active:
   assert time.monotonic()<deadline,'Independent declared18000s owning materialization deadline exceeded'
   for key,(row,kind) in list(pending.items()):
    if len(active)>=2:break
    if not set(row['dag_dependencies'])<=completed:continue
    launch(key,row,kind);del pending[key]
   assert active or not pending,('Unclosed actual generated/compile/link dependency graph',[(key,sorted(set(row['dag_dependencies'])-completed))for key,(row,kind)in list(pending.items())[:12]])
   rss=0
   for pid,value in list(active.items()):
    for child_pid in descendants(pid):
     try:
      for line in(Path('/proc')/str(child_pid)/'status').read_text().splitlines():
       if line.startswith('VmRSS:'):rss+=int(line.split()[1])*1024
     except(FileNotFoundError,ProcessLookupError):pass
    code=value['process'].poll()
    if code is None:continue
    kind=value['kind'];value['stream'].close();key=value['key'];target=dest/key;record({'event':'complete','kind':kind,'key':key,'pid':pid,'exit':code,'seconds':time.monotonic()-value['began'],'log_sha256':sha(value['log'])});del active[pid]
    assert code==0,('Actual generated/complete TU/link failed',kind,key,code,str(value['log']))
    assert target.is_file()and target.stat().st_size>0,('Successful producer omitted its owned output',kind,key)
    if kind=='compile':assert (dest/plan['objects'][key]['depfile']).is_file(),('Fresh compiler omitted actual declared depfile',key)
    if kind=='generate':assert sha(target)==plan['generated_outputs'][key]['expected_sha256'],('Current generated bytes differ despite equivalent audited source/generator inputs; admission stopped',key)
    completed.add(key);products[key]=sha(target);state[{'compile':'completed_compilations','link':'completed_links','generate':'completed_generators'}[kind]]+=1
   peak_rss=max(peak_rss,rss);assert peak_rss<=8*2**30,'Owning compiler/link descendant RSS8GiB exceeded'
   state.update(state='running-generated-compile-link-DAG',active={str(pid):{'key':v['key'],'kind':v['kind'],'identity':v['identity']}for pid,v in active.items()},peak_descendant_RSS_bytes=peak_rss,seconds=time.monotonic()-started);save()
   if active:time.sleep(.1)
 def received_term(signum,frame):raise SystemExit(143)
 signal.signal(signal.SIGTERM,received_term)
 try:
  save()
  for key,row in plan['objects'].items():
   if row['route']=='reuse-authenticated-whole-object':copy(key,row);completed.add(key)
  for key,row in plan['links'].items():
   if row['route']=='reuse-byteidentical-whole-product':copy(key,row);completed.add(key)
  atomic(out/'staged-whole-providers.json',copied)
  schedule()
  from dependency_audit_v5 import audit as audit_fresh_dependencies
  state['actual_current_dependency_audit']=audit_fresh_dependencies(plan,dest,out,inputs)
  assert time.monotonic()<deadline,'Fresh actual dependency sealing exceeded declared18000s materialization bound'
  for key,row in plan['links'].items():
   assert time.monotonic()<deadline,'Independent18000s archive/output sealing bound exceeded'
   if not row['archive']:continue
   expected=[{'name':Path(name).name,'bytes':(dest/name).stat().st_size,'sha256':sha(dest/name)}for name in row['object_inputs']];assert archive_members(dest/key)==expected,('Final current archive members/order differ',key)
  outputs={}
  for key in set(plan['objects'])|set(plan['links'])|set(plan['generated_outputs']):
   assert time.monotonic()<deadline,'Independent18000s archive/output sealing bound exceeded'
   outputs[key]=sha(dest/key)
  atomic(out/'owned-outputs.json',outputs)
  assert state['completed_compilations']==plan['summary']['fresh_complete_TUs']and state['completed_links']==plan['summary']['fresh_links_archives']
  assert state['completed_generators']==len(plan['generated_outputs'])
  for key,row in plan['generated_outputs'].items():assert sha(dest/key)==row['expected_sha256']
  state.update(exit=0,owned_outputs={'path':str(out/'owned-outputs.json'),'sha256':sha(out/'owned-outputs.json'),'count':len(outputs)},staged_whole_providers={'path':str(out/'staged-whole-providers.json'),'sha256':sha(out/'staged-whole-providers.json'),'count':len(copied)})
 except BaseException as error:
  state.update(exit=1,error=repr(error));kill_owned()
 finally:
  state.update(state='terminal',active={},seconds=time.monotonic()-started,input_drift=drift(inputs),peak_descendant_RSS_bytes=peak_rss);state['exit']=state['exit']or int(bool(state['input_drift']));save()
 print(json.dumps({key:state.get(key)for key in ['state','exit','completed_compilations','completed_links','seconds','error','input_drift']}),flush=True);return state['exit']
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('plan',type=Path);parser.add_argument('--execute',action='store_true');args=parser.parse_args()
 if args.execute:raise SystemExit(execute(args.plan.resolve()))
 plan=json.loads(args.plan.read_text());print(json.dumps({'state':'planned-NOT-executed','source':plan['source'],'summary':plan['summary'],'runner_sha256':sha(__file__),'profile':plan['profile'],'resource_bounds':{'independent_owning_seconds':18000,'max_active_tasks':2,'aggregate_descendant_RSS_bytes':8*2**30,'initial_input_audit_seconds':600,'final_input_audit_seconds':600},'strict_SAN_options':{'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1','LSAN_OPTIONS':None,'LD_PRELOAD':None},'execution_hold':(args.plan.parent/'EXECUTION-HOLD.json').is_file()},indent=2))
