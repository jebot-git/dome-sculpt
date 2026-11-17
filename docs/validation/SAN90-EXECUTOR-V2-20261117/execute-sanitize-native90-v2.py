"""PREPARE-ONLY template for a reviewed mixed-current strict sanitizer owning build.
Default is read-only plan inspection. Never executes built products or tests.
Original prepare HOLD is preserved and requires a separately pinned root review.
"""
import argparse,hashlib,json,os,re,resource,shutil,signal,stat,subprocess,time
from pathlib import Path
from ninja_audit import archive_members
P=Path(__file__).resolve().parent
LIMITS={'independent_owning_seconds':18000,'max_active_tasks':2,'aggregate_descendant_RSS_bytes':8*2**30,'initial_input_audit_seconds':600,'final_input_audit_seconds':600}
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

def validate_instrumented_owners(plan):
 """Retain exact CMake optimized CPU/FMA fixtures without relaxing SAN flags."""
 optimized={
  'CMakeFiles/dome-sculpt-resample-native-contract-test.dir/tests/resample-native-contract.cpp.o':'tests/resample-native-contract.cpp',
  'CMakeFiles/dome-sculpt-resample-native-contract-test.dir/src/sparse-resample-cpu.cpp.o':'src/sparse-resample-cpu.cpp',
 }
 seen=set();source=Path(plan['source_path'])
 for key,owner in plan['objects'].items():
  argv=owner['command'];name=owner['source']
  assert all(argv.count(flag)==1 for flag in ['-O1','-g1','-DNDEBUG','-fsanitize=address,undefined','-fno-omit-frame-pointer']),('Strict C/C++ owner flags differ',key,name)
  options=[x for x in argv if x.startswith('-O')]
  if key in optimized:
   assert Path(name)==source/optimized[key]and options==['-O1','-O3'],('Optimized contract owner/source/options differ',key,name,options)
   assert argv.count('-march=native')==1 and argv.index('-O1')<argv.index('-O3')<argv.index('-march=native'),('Optimized CPU/FMA fixture flag order differs',key)
   seen.add(key)
  else:assert options==['-O1'],('Unexpected optimized sanitizer owner',key,name,options)
 assert len(plan['objects'])==1036 and seen==set(optimized)
 return {'owners':len(plan['objects']),'optimized_contract_owners':len(seen),'all_other_owners_only_O1':len(plan['objects'])-len(seen),'sanitizer_and_frame_pointer_flags_retained_for_all':True}

def execute(plan_file,review_file,review_sha):
 resource.setrlimit(resource.RLIMIT_CORE,(0,0));started=time.monotonic();deadline=started+18000;active={};owned={};ledger={};copied={};completed=set();peak=0;next_sample=0
 def rawsha(path):
  h=hashlib.sha256()
  with Path(path).open('rb')as f:
   while b:=f.read(1<<20):
    if time.monotonic()>deadline:raise TimeoutError('Declared18000s complete materialization bound exceeded')
    h.update(b)
  return h.hexdigest()
 plan=json.loads(plan_file.read_bytes());assert plan['state']=='audited-NOT-compiled'and plan['profile']=='sanitize'
 assert len(plan['objects'])==1036 and len(plan['links'])==737 and plan['initial_objects']==0
 assert set(plan['required_targets'])<=set(plan['links'])and plan['whole_provider_stage_count']==plan['summary']['reused_whole_objects']
 assert plan['summary']['configured_TUs']==1036 and plan['summary']['all_configured_link_archive_nodes']==737 and len(plan['generated_outputs'])==3
 assert plan['summary']['fresh_complete_TUs']+plan['summary']['reused_whole_objects']==1036
 assert plan['summary']['fresh_links_archives']==737 and plan['summary']['retained_whole_products']==0
 assert all(x['route']=='fresh-current-link'for x in plan['links'].values())
 assert plan['resource_bounds']==LIMITS and plan['current_toolchain']['C_FLAGS_RELEASE']==plan['current_toolchain']['flags']=='-O1 -g1 -DNDEBUG -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer'
 assert plan['current_toolchain']['link_flags']=='-Wl,--gc-sections -fsanitize=address,undefined'and len(plan['selected_CTests'])==len(set(plan['selected_CTests']))==plan['summary']['selected_CTests']==90
 assert plan['environment']['ASAN_OPTIONS']=='detect_leaks=1:halt_on_error=1:abort_on_error=1'and plan['environment']['UBSAN_OPTIONS']=='halt_on_error=1:print_stacktrace=1'and plan['environment']['LSAN_OPTIONS']is None and plan['environment']['LD_PRELOAD']is None
 assert plan['provider_authority']['actual_compiler_CWD_routes']=={'fresh':494,'prior_closed':538}and plan['provider_authority']['original_actual_exit']==143 and plan['provider_authority']['original143_not_a_pass']is True
 validate_instrumented_owners(plan)
 hold=plan_file.parent/'EXECUTION-HOLD.json';assert hold.is_file(),'Preparation HOLD must remain unchanged'
 assert rawsha(review_file)==review_sha;review=json.loads(review_file.read_bytes())
 assert review['state']=='reviewed-for-build-only'and review['source']==plan['source']and review['destination']==plan['destination']
 assert review['plan_sha256']==rawsha(plan_file)and review['prepare_hold_sha256']==rawsha(hold)and review['resource_bounds']==LIMITS
 assert review['executor_sha256']==rawsha(__file__)and review['mixed_audit_sha256']==rawsha(P/'mixed_dependency_audit_v1.py')
 assert review['preserve_prepare_hold']is True and review['no_products_tests_install_HMD']is True
 for path in[Path(__file__).resolve(),P/'mixed_dependency_audit_v1.py',P/'current_provider_routes_v2.py',P/'provider_context_v1.py',P/'ninja_audit.py',Path('/usr/bin/nice'),Path('/bin/bash'),Path('/usr/bin/env'),Path('/usr/bin/python3')]:
  assert str(path)in plan['pins']and rawsha(path)==plan['pins'][str(path)],('Execution helper differs from reviewed plan',str(path))
 assert Path('/proc/self/exe').resolve()==Path('/usr/bin/python3').resolve(),'Executor must use the explicitly pinned system Python interpreter'
 assert not any(os.environ.get(k)for k in['LD_PRELOAD','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS']),'Launcher must clear inherited injection/instrumentation before executor starts'
 metadata_env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))};metadata_env['LD_LIBRARY_PATH']=plan['environment']['LD_LIBRARY_PATH']
 unit=review['systemd_user_service'];assert unit.endswith('.service')and '/'not in unit and unit.startswith('ds-')
 cgroup_lines=Path('/proc/self/cgroup').read_text().splitlines();assert len(cgroup_lines)==1 and cgroup_lines[0].startswith('0::')
 own_cgroup=cgroup_lines[0][3:];assert own_cgroup.endswith('/'+unit)and '/user.slice/'in own_cgroup
 cgroup_root=Path('/sys/fs/cgroup')/own_cgroup.lstrip('/');assert(cgroup_root/'memory.max').read_text().strip()==str(8*2**30)
 systemctl=Path('/usr/bin/systemctl');assert str(systemctl)in plan['pins']and rawsha(systemctl)==plan['pins'][str(systemctl)]
 properties=subprocess.check_output([str(systemctl),'--user','show',unit,'--property=MainPID,ExecMainPID,Type,KillMode,RemainAfterExit,MemoryMax,RuntimeMaxUSec,TimeoutStopUSec,InvocationID'],text=True,env=metadata_env)
 properties=dict(x.split('=',1)for x in properties.splitlines())
 assert properties['MainPID']==properties['ExecMainPID']==str(os.getpid())and properties['Type']=='exec'and properties['KillMode']=='control-group'and properties['RemainAfterExit']=='yes'
 assert properties['MemoryMax']==str(8*2**30)and re.fullmatch('[0-9a-f]{32}',properties['InvocationID'])
 def duration_us(value):
  value=value.replace(' ','');parts=re.findall(r'([0-9]+)(us|ms|min|s|h|d)',value);assert ''.join(a+b for a,b in parts)==value
  return sum(int(a)*{'us':1,'ms':1000,'s':1000000,'min':60000000,'h':3600000000,'d':86400000000}[b]for a,b in parts)
 assert duration_us(properties['RuntimeMaxUSec'])==18600*1000000 and duration_us(properties['TimeoutStopUSec'])==10*1000000
 assert review['service_runtime_max_seconds']==18600 and review['service_memory_max_bytes']==8*2**30
 def service_pids():
  pids=set()
  for path in[cgroup_root,*[x for x in cgroup_root.rglob('*')if x.is_dir()]]:
   try:pids.update(map(int,(path/'cgroup.procs').read_text().split()))
   except FileNotFoundError:continue
  return pids
 assert service_pids()=={os.getpid()},'Independent reviewed service has unowned preexisting processes'
 dest=Path(plan['destination']).resolve();source=Path(plan['source_path']).resolve();head=plan['source'];out=plan_file.parent/'execution-native90-v2';out.mkdir()
 assert dest.is_dir()and not list(dest.rglob('*.o')),'Destination must have no staged/compiled project object before independent execution'
 inputs=dict(plan['pins'])
 for path,value in[(plan_file,rawsha(plan_file)),(review_file,review_sha),(hold,rawsha(hold))]:
  assert str(path)not in inputs or inputs[str(path)]==value;inputs[str(path)]=value
 state={'state':'preparing','exit':1,'source':head,'profile':'sanitize','scope':'Explicit reused complete current objects plus genuinely new complete compiler owners; all737 archive/link actions and3 generators fresh. Reused original MD, command and CWD retained; no newly-compiled claim for reused owners. No product/test execution.','plan':{'path':str(plan_file),'sha256':rawsha(plan_file)},'execution_review':{'path':str(review_file),'sha256':review_sha},'prepare_hold_preserved':{'path':str(hold),'sha256':rawsha(hold)},'resource_bounds':LIMITS,'summary':plan['summary'],'completed_compilations':0,'completed_links':0,'completed_generators':0,'staged_whole_objects':0,'active':{},'external_signal':None,'cleanup':None,'input_drift':None,'initial_input_drift':None,'owned_user_service':{'unit':unit,'cgroup':own_cgroup,'properties':properties}}
 journal=out/'steps.jsonl'
 def save():atomic(out/'status.json',state)
 def record(value):
  with journal.open('a')as f:f.write(json.dumps(value)+'\n')
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
  peak=max(peak,rss);assert peak<=8*2**30,'Owned executor+compiler aggregateRSS exceeds8GiB';return live
 def checkpoint():
  nonlocal next_sample
  now=time.monotonic()
  if now>deadline:raise TimeoutError('Declared18000s complete materialization bound exceeded')
  if now>=next_sample:sample();next_sample=now+.2
 def sha(path):checkpoint();return rawsha(path)
 def audit_inputs(seconds):
  until=time.monotonic()+seconds;changed=[]
  for path,value in inputs.items():
   checkpoint()
   if time.monotonic()>until:raise TimeoutError('Declared600s immutable-input audit bound exceeded')
   if not Path(path).is_file()or sha(path)!=value:changed.append(path)
  return changed
 def assert_source():
  assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==head
  assert not subprocess.check_output(['git','status','--porcelain'],cwd=source,text=True).strip()
 def target_path(key):
  path=dest/key;assert not Path(key).is_absolute()and path.resolve().is_relative_to(dest)
  path.parent.mkdir(parents=True,exist_ok=True);assert not path.exists()and not path.is_symlink(),('Refuse preexisting owned output',key);return path
 def independent_copy(provider,target,digest):
  checkpoint();provider=Path(provider);target=Path(target);assert provider.is_file()and not target.exists()and not target.is_symlink()
  before=file_identity(provider);assert sha(provider)==digest
  target.parent.mkdir(parents=True,exist_ok=True)
  with provider.open('rb')as src,target.open('xb')as dst:
   while b:=src.read(1<<20):checkpoint();dst.write(b)
  target.chmod(0o444);after=file_identity(provider);target_id=file_identity(target)
  assert before==after and sha(provider)==sha(target)==digest
  assert(target_id['dev'],target_id['ino'])!=(before['dev'],before['ino'])and target.stat().st_nlink==1 and not(target_id['mode']&0o222)
  return {'provider_identity_before':before,'provider_identity_after':after,'copy_identity':target_id,'independent_inode':True,'readonly':True,'whole_file_byteexact':True}
 def stage_object(key,row):
  assert row['route']=='reuse-authenticated-whole-object';target=target_path(key)
  assert not(dest/row['depfile']).exists(),'Do not populate new compiler .d for reused owner'
  object_copy=independent_copy(row['provider'],target,row['provider_sha256'])
  snapshot=out/'provider-depfiles'/(key+'.original.d')
  dep_copy=independent_copy(row['provider_depfile'],snapshot,row['provider_depfile_sha256'])
  entry={'route':row['route'],'newly_compiled':False,'provider':row['provider'],'provider_sha256':row['provider_sha256'],'destination':str(target),'object_copy':object_copy,'original_depfile':row['provider_depfile'],'original_depfile_sha256':row['provider_depfile_sha256'],'unchanged_depfile_snapshot':str(snapshot),'depfile_copy':dep_copy,'original_actual_compile_command':row['provider_actual_compile_command'],'original_actual_compile_argv':row['provider_actual_compile_argv'],'original_actual_compile_cwd':row['provider_actual_compile_cwd'],'original_actual_journal':row['provider_actual_journal']}
  copied[key]=entry;completed.add(key);state['staged_whole_objects']=len(copied);record({'event':'stage-current-whole-object','key':key,**entry});atomic(out/'staged-whole-providers.json',copied);save()
 env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))}
 for key in['LD_PRELOAD','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS']:env.pop(key,None)
 for key,value in plan['compiler_environment'].items():
  if value is None:env.pop(key,None)
  else:env[key]=value
 assert shutil.which('nice',path=env.get('PATH'))=='/usr/bin/nice'
 env.update(PKG_CONFIG_PATH=plan['environment']['PKG_CONFIG_PATH'],LD_LIBRARY_PATH=plan['environment']['LD_LIBRARY_PATH'],LP_NUM_THREADS='2',OMP_NUM_THREADS='2',OPENBLAS_NUM_THREADS='2')
 assert env['LD_LIBRARY_PATH']==str(P.parents[1]/'artifacts/sdk-20261111/toolkit-sanitize/lib')+':/opt/fltk-vr/bdf-runtime/lib:/usr/lib64'
 env.update(ASAN_OPTIONS=plan['environment']['ASAN_OPTIONS'],UBSAN_OPTIONS=plan['environment']['UBSAN_OPTIONS'])
 assert not env.get('LSAN_OPTIONS')and not env.get('LD_PRELOAD'),'No suppressions or injected libraries in strict cohort'
 state['environment']={k:env.get(k)for k in['LD_LIBRARY_PATH','LD_PRELOAD','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','LP_NUM_THREADS','OMP_NUM_THREADS','OPENBLAS_NUM_THREADS']}
 def launch(key,row,kind):
  target_path(key)
  if kind=='compile':assert not(dest/row['depfile']).exists(),('Fresh compiler MD already exists',key)
  argv=['nice','-n','10',*row['command']]if kind=='compile'else['nice','-n','10','/bin/bash','-c',row['command']]
  log=out/(kind+'-'+key.replace('/','_')+'.log');stream=log.open('x')
  def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
  try:child=subprocess.Popen(argv,cwd=dest,env=env,stdout=stream,stderr=subprocess.STDOUT,start_new_session=True,preexec_fn=no_core)
  except BaseException:stream.close();raise
  identity=proc_identity(child.pid)
  active[child.pid]={'process':child,'identity':identity,'key':key,'kind':kind,'stream':stream,'log':log,'began':time.monotonic()}
  assert identity and identity['pgrp']==identity['session']==child.pid
  observe();record({'event':'launch','key':key,'kind':kind,'pid':child.pid,'identity':identity,'argv':argv,'actual_command':row['command'],'cwd':str(dest),'log':str(log),'epoch':time.time()})
 def schedule():
  pending={k:(v,'generate')for k,v in plan['generated_outputs'].items()}
  pending.update({k:(v,'compile')for k,v in plan['objects'].items()if v['route']=='compile-complete-TU'})
  pending.update({k:(v,'link')for k,v in plan['links'].items()})
  while pending or active:
   checkpoint()
   for key,(row,kind)in list(pending.items()):
    if len(active)>=2:break
    if not set(row['dag_dependencies'])<=completed:continue
    launch(key,row,kind);del pending[key]
   assert active or not pending,('Actual owning DAG cannot close',list(pending)[:8])
   for pid,task in list(active.items()):
    code=task['process'].poll()
    if code is None:continue
    task['stream'].close();kind=task['kind'];key=task['key'];record({'event':'complete','key':key,'kind':kind,'pid':pid,'exit':code,'seconds':time.monotonic()-task['began'],'log_sha256':sha(task['log'])});del active[pid]
    assert code==0,('Actual complete owning action failed',kind,key,code,str(task['log']))
    target=dest/key;assert target.is_file()and target.stat().st_size>0
    if kind=='compile':assert(dest/plan['objects'][key]['depfile']).is_file(),('Compiler omitted actual MD',key)
    if kind=='generate':assert sha(target)==plan['generated_outputs'][key]['expected_sha256']
    completed.add(key);state[{'compile':'completed_compilations','link':'completed_links','generate':'completed_generators'}[kind]]+=1
   state.update(state='running-owning-DAG',active={str(pid):{'key':x['key'],'kind':x['kind'],'identity':x['identity']}for pid,x in active.items()},pending_actions=len(pending),seconds=time.monotonic()-started,peak_aggregate_RSS_bytes=peak);save()
   if active:time.sleep(.1)
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
 try:
  save();assert_source();state['initial_input_drift']=audit_inputs(600);assert not state['initial_input_drift'],'Immutable inputs changed before staging';atomic(out/'inputs.json',inputs);state['inputs']={'path':str(out/'inputs.json'),'sha256':sha(out/'inputs.json')}
  atomic(out/'staged-whole-providers.json',copied)
  for key,row in plan['objects'].items():
   checkpoint()
   if row['route']=='reuse-authenticated-whole-object':stage_object(key,row)
   else:assert row['route']=='compile-complete-TU'
  assert len(copied)==plan['summary']['reused_whole_objects'];schedule()
  assert not sample(),'Successful action roots left owned live descendants'
  from mixed_dependency_audit_v1 import audit
  state['actual_current_dependency_audit']=audit(plan,dest,out,inputs,copied,journal,checkpoint)
  for key,row in plan['links'].items():
   checkpoint()
   if row['archive']:
    expected=[{'name':Path(x).name,'bytes':(dest/x).stat().st_size,'sha256':sha(dest/x)}for x in row['object_inputs']]
    assert archive_members(dest/key)==expected,('Current final archive payload/order mismatch',key)
  outputs={}
  for key in set(plan['objects'])|set(plan['links'])|set(plan['generated_outputs']):checkpoint();outputs[key]=sha(dest/key)
  assert len(outputs)==1776 and state['completed_compilations']==plan['summary']['fresh_complete_TUs']and state['completed_links']==737 and state['completed_generators']==3
  atomic(out/'owned-outputs.json',outputs);assert_source();state['input_drift']=audit_inputs(600);assert not state['input_drift'],'Immutable inputs changed at final seal'
  assert rawsha(hold)==review['prepare_hold_sha256'];state.update(exit=0,owned_outputs={'path':str(out/'owned-outputs.json'),'sha256':sha(out/'owned-outputs.json'),'count':len(outputs)},staged_whole_providers={'path':str(out/'staged-whole-providers.json'),'sha256':sha(out/'staged-whole-providers.json'),'count':len(copied)})
 except BaseException as exc:
  state.update(exit=128+exc.signum if isinstance(exc,OwnedTermination)else 1,error=repr(exc))
  if isinstance(exc,OwnedTermination):state['external_signal']=exc.signum
  cleanup()
 finally:
  # No build/test restart, output deletion, provider mutation or signal reclassification.
  if active and state['exit']==0:state.update(exit=1,error='Unexpected active handles after seal');cleanup()
  state.update(state='terminal',seconds=time.monotonic()-started,active={},peak_aggregate_RSS_bytes=peak,remaining_owned_same_start=observe())
  if state['remaining_owned_same_start']and state['exit']==0:state['exit']=1
  atomic(out/'owned-process-ledger.json',ledger);save()
 print(json.dumps({k:state.get(k)for k in['state','exit','completed_compilations','completed_links','completed_generators','staged_whole_objects','seconds','external_signal','error','remaining_owned_same_start']}),flush=True);return state['exit']
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('plan',type=Path);parser.add_argument('--execute',action='store_true');parser.add_argument('--execution-review',type=Path);parser.add_argument('--review-sha256');args=parser.parse_args()
 signal.signal(signal.SIGTERM,received_signal);signal.signal(signal.SIGINT,received_signal)
 try:
  if args.execute:
   assert args.execution_review and args.review_sha256;raise SystemExit(execute(args.plan.resolve(),args.execution_review.resolve(),args.review_sha256))
  plan=json.loads(args.plan.read_bytes());print(json.dumps({'state':'inspect-only-NOT-executed','source':plan['source'],'summary':plan['summary'],'prepare_hold_present':(args.plan.parent/'EXECUTION-HOLD.json').is_file(),'execution_review_required':True,'resource_bounds':LIMITS},indent=2))
 except OwnedTermination as exc:raise SystemExit(128+exc.signum)
