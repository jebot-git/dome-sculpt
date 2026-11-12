#!/usr/bin/env python3
"""Execute only the technically reviewed frozen remainder plan, never original Ninja."""
from pathlib import Path
import hashlib,json,os,sys,subprocess,time,signal,threading,concurrent.futures,re,shutil,fcntl
A=Path(__file__).resolve().parent;R=A.parents[1]
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb')as f:
  for block in iter(lambda:f.read(4*1024*1024),b''):h.update(block)
 return h.hexdigest()
assert len(sys.argv)==3 and sys.argv[1]=='--execute-reviewed-plan-sha256'
assert sha(A/'plan.json')==sys.argv[2]
plan=json.load(open(A/'plan.json'));assert sha(__file__)==plan['runner_sha256'];B=Path(plan['original_build']);O=Path(plan['new_namespace']);S=R/'artifacts/authoring-source8-hdr-fit-union-20261112';SDK=R/'artifacts/sdk-20261111';sourcepins=json.load(open(A/'frozen-inputs.json'));success=json.load(open(A/'success-record-outputs.json'));assert sha(A/'frozen-inputs.json')==plan['snapshots']['inputs_sha256']and sha(A/'success-record-outputs.json')==plan['snapshots']['success_outputs_sha256'];assert not(A/'status.json').exists();os.nice(10)
lock=threading.RLock();stop=threading.Event();live={};copies={};state={'state':'running','source':plan['source'],'scope':plan['execution_policy']['claim'],'plan_sha256':sys.argv[2],'runner_sha256':sha(__file__),'pid':os.getpid(),'steps':[],'copy_count':0,'original_failed_build_exit':124,'original_failure_relabeled':False};status=A/'status.json';env=dict(os.environ)
for k in ['ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','LD_PRELOAD','DRI_PRIME','GALLIUM_DRIVER','MESA_LOADER_DRIVER_OVERRIDE','LIBGL_DRIVERS_PATH','__EGL_VENDOR_LIBRARY_FILENAMES']:env.pop(k,None)
env.update(PKG_CONFIG_PATH='/usr/lib64/pkgconfig',LD_LIBRARY_PATH=str(SDK/'toolkit-sanitize/lib')+':/usr/lib64',LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',OPENBLAS_NUM_THREADS='2',OMP_NUM_THREADS='2',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');state['environment']={k:env.get(k)for k in ['LD_LIBRARY_PATH','LIBGL_ALWAYS_SOFTWARE','LP_NUM_THREADS','OMP_NUM_THREADS','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','LD_PRELOAD']}
def save():
 with lock:
  temp=A/'status.writing';temp.write_text(json.dumps(state,indent=2)+'\n');temp.replace(status)
def run(cmd,label,timeout):
 if stop.is_set():raise RuntimeError('Sibling step failed; further launch refused')
 row={'name':label,'command':list(map(str,cmd)),'deadline_seconds':timeout};log=A/(label+'.log');start=time.monotonic()
 with lock:state['steps'].append(row);save()
 with log.open('wb')as f:
  p=subprocess.Popen(cmd,cwd=O,env=env,stdout=f,stderr=subprocess.STDOUT,start_new_session=True)
  with lock:live[p.pid]=p;state['live_pids']=list(live);save()
  try:rc=p.wait(timeout=timeout)
  except subprocess.TimeoutExpired:
   os.killpg(p.pid,signal.SIGTERM)
   try:p.wait(timeout=3)
   except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait(timeout=3)
   rc=124
  finally:
   with lock:live.pop(p.pid,None);state['live_pids']=list(live)
 with lock:row.update(exit=rc,seconds=time.monotonic()-start,log_sha256=sha(log));save()
 if rc:stop.set();raise RuntimeError((label,rc))
 return log
try:
 state['phase']='copy-verify-and100links';save();deadline=time.monotonic()+1800
 # Byte-authenticate all original providers; copying uses independent reflinks, never hardlinks.
 for path,h in sourcepins.items():
  assert time.monotonic()<deadline,'1800s copy/link admission bound';assert Path(path).is_file()and sha(path)==h,('before-drift',path)
 assert sha(O/'CTestTestfile.cmake')==plan['ctest_metadata']['routed_sha256']
 for rel,info in success.items():
  assert time.monotonic()<deadline,'1800s copy/link admission bound';old=B/rel;new=O/rel;assert not new.exists();new.parent.mkdir(parents=True,exist_ok=True)
  subprocess.run(['cp','--reflink=always','--preserve=mode,timestamps','--',str(old),str(new)],check=True,timeout=max(1,deadline-time.monotonic()))
  assert new.stat().st_ino!=old.stat().st_ino and sha(new)==info['sha256'];new.chmod(0o555 if info['mode']&0o111 else 0o444);copies[str(new)]=info['sha256'];state['copy_count']+=1
  if state['copy_count']%20==0:save()
 state['copied_provider_outputs']=len(copies);save()
 def link(item):
  remaining=deadline-time.monotonic();assert remaining>0,'1800s remainder link bound';cmd=item['command'];assert str(B)not in ' '.join(cmd)and not(O/item['output']).exists()
  dep=next((x.split('=',1)[1]for x in cmd if x.startswith('-Wl,--dependency-file=')),None)
  if dep:(O/dep).parent.mkdir(parents=True,exist_ok=True)
  return run(cmd,'remaining-link-'+item['output'],min(600,remaining))
 with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:
  futures=[pool.submit(link,item)for item in plan['pending_links']]
  try:
   for future in concurrent.futures.as_completed(futures):future.result()
  except BaseException:
   stop.set()
   for future in futures:future.cancel()
   with lock:
    for p in live.values():
     if p.poll()is None:os.killpg(p.pid,signal.SIGTERM)
   raise
 assert len([x for x in state['steps']if x['name'].startswith('remaining-link-')and x['exit']==0])==100
 for item in plan['pending_links']:
  p=O/item['output'];assert p.is_file();state.setdefault('new_link_products',{})[item['output']]=sha(p)
 state['phase']='own-app-version';save();run([str(O/'dome-sculpt'),'--version'],'app-version',60)
 regex='^('+ '|'.join(re.escape(n)for n in plan['expected_tests'])+')$';listing=subprocess.check_output(['ctest','--test-dir',str(O),'--show-only=json-v1','-R',regex],env=env,text=True,timeout=60);(A/'selected-tests.json').write_text(listing);tests=json.loads(listing)['tests'];assert sorted(x['name']for x in tests)==plan['expected_tests'];assert all(x.get('command')and Path(x['command'][0]).is_file()and Path(x['command'][0]).is_relative_to(O)for x in tests);state['selected_names']=[x['name']for x in tests];state['selected_manifest_sha256']=sha(A/'selected-tests.json');save()
 state['phase']='await-common-graphics-lease';save();lease=open('/tmp/dome-sculpt-arc-device.lock','a');wait=time.monotonic()+600
 while True:
  try:fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB);break
  except BlockingIOError:
   assert time.monotonic()<wait,'600s graphics lease wait bound';time.sleep(.5)
 state['graphics_lease_acquired_unix']=time.time();state['phase']='exact82-native-software';save()
 try:log=run(['ctest','--test-dir',str(O),'--output-on-failure','--timeout','180','-R',regex],'exact82-native-software',2400)
 finally:fcntl.flock(lease,fcntl.LOCK_UN);lease.close();state['graphics_lease_released_unix']=time.time();save()
 passed=re.findall(r'Test\s+#\d+:\s+(\S+)\s+\.+\s+Passed',log.read_text());assert sorted(passed)==plan['expected_tests'],'Every exact82 native/software case must actually pass once'
 state['phase']='fresh-explicit-helpers';save();helper_deadline=time.monotonic()+300
 for item in plan['helpers']:
  for i,cmd in enumerate(item['commands']):
   out=O/cmd[cmd.index('-o')+1];assert not out.exists();out.parent.mkdir(parents=True,exist_ok=True);remaining=helper_deadline-time.monotonic();assert remaining>0;run(cmd,'helper-'+item['target']+'-'+str(i),remaining)
 for label,target,needle in [('workspace27','dome-sculpt-sculpt-chunk-workspace-test','PASS total 27'),('placed-workspace373','dome-sculpt-sculpt-placed-workspace-test','PASS 373 placed-owner workspace controls')]:
  log=run([str(O/target)],label,300);assert needle in log.read_text(),('missing actual control total',label)
 log=run([str(O/'dome-sculpt-baked-emission-workflow-test'),'--raw-source-worker-controls',str(O/'raw-uv1-worker')],'raw-source-worker90159',300);assert 'Baked emission raw UV1 worker 90159 checks PASS'in log.read_text()
 state['exit']=0
except BaseException as e:state.update(exit=1,error=repr(e))
finally:
 # Preserve all original failure evidence and complete providers, including the disclosed inspection log.
 state['phase']='final-input-and-copy-audit';save();audit_deadline=time.monotonic()+600;drift=[]
 for p,h in {**sourcepins,**copies}.items():
  if time.monotonic()>audit_deadline:drift.append('600s final audit bound exceeded');break
  if not Path(p).is_file()or sha(p)!=h:drift.append(p)
 state['input_or_copy_drift']=drift;state['state']='terminal';state['phase']='terminal';state['live_pids']=[pid for pid,p in live.items()if p.poll()is None]
 if drift or state['live_pids']:state['exit']=1
 save();print(json.dumps({k:state.get(k)for k in ['state','source','exit','error','input_or_copy_drift','live_pids']},indent=2),flush=True)
raise SystemExit(state['exit'])
