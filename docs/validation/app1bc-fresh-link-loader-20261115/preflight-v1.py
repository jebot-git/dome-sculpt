"""Authorized narrow component loader+version only. No GUI/XR app or installation."""
import fcntl,hashlib,json,os,re,signal,sys,time
from pathlib import Path
P=Path(__file__).resolve().parent;ROOT=P.parents[1];LINK=ROOT/'artifacts/app1bc-public-component-link-20261115';sys.path.insert(0,str(LINK))
from owned_monitor_v2 import run,identity,alive
from component_link_common import sha
BOUNDARY=ROOT/'artifacts/guard87-validation-fresh-v2-20261115/runtime-preflight.json';EXPECTED='949716280d10503f4899694790c1ade8779c7acce793b6f6c6d78408529129d4';APP=LINK/'trial-v2/dome-sculpt';APP_SHA='4988652c32940b4506fcceb66db6776f0949018972c8bdcc239421845ccdb042';LOCK=Path('/tmp/dome-sculpt-arc-device.lock')
assert sha(BOUNDARY)==EXPECTED and sha(APP)==APP_SHA
boundary=json.loads(BOUNDARY.read_text());pins=dict(boundary['pins']);assert str(APP)not in pins;pins[str(APP)]=APP_SHA
for file in[BOUNDARY,LINK/'execution-v1/status.json',LINK/'plan-v3.json',LINK/'root-link-review-v3.json',LINK/'owned_monitor_v2.py',Path(__file__).resolve(),Path('/usr/bin/ldd')]:
 value=sha(file);assert str(file)not in pins or pins[str(file)]==value;pins[str(file)]=value
assert not[p for p,h in pins.items()if not Path(p).is_file()or sha(p)!=h]
source_plan=json.loads((LINK/'plan-v3.json').read_text());prior_seed=str(Path(source_plan['authority']['old_build_path'])/'dome-sculpt');expected_rows=next(x['rows']for x in boundary['closure']if x['seed']==prior_seed)
# Preserve all historical App3d/provider expectations; the fresh App has a distinct path.
state={'state':'prepared','exit':1,'scope':'App1bc/components3d/assets3d component loader and --version ONLY. Not GUI/XR/app workflow, main or owning90 admission.','source_tuple':source_plan['scope_tuple'],'new_application':{'path':str(APP),'sha256':APP_SHA},'prior_boundary':{'path':str(BOUNDARY),'sha256':EXPECTED},'unchanged_prior_pin_count':len(boundary['pins']),'original_App_pin_preserved':True,'pins':pins,'steps':[],'lease':None,'input_drift':None,'signals':[]}
def save():
 q=P/'status.json';t=q.with_suffix('.writing');t.write_text(json.dumps(state,indent=2)+'\n');t.replace(q)
def terminate(sig,frame):state['signals'].append(sig);raise SystemExit(128+sig)
signal.signal(signal.SIGTERM,terminate);signal.signal(signal.SIGINT,terminate)
root=P/'private';root.mkdir();env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))}
for key in['LD_PRELOAD','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','DISPLAY','WAYLAND_DISPLAY','DBUS_SESSION_BUS_ADDRESS','BASH_ENV','ENV']:env.pop(key,None)
for key,name in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:
 q=root/name;q.mkdir(mode=0o700);q.chmod(0o700);env[key]=str(q)
env.update(boundary['environment']);env.update(XR_RUNTIME_JSON=str(root/'runtime'/'no-physical-xr-runtime.json'),OPENBLAS_NUM_THREADS='2',LP_NUM_THREADS='2',OMP_NUM_THREADS='2')
state['private_child_environment']={k:env.get(k)for k in['LD_LIBRARY_PATH','LD_PRELOAD','DISPLAY','WAYLAND_DISPLAY','DBUS_SESSION_BUS_ADDRESS','XR_RUNTIME_JSON','XDG_CONFIG_HOME','XDG_STATE_HOME','XDG_DATA_HOME','XDG_CACHE_HOME','XDG_RUNTIME_DIR']}
fd=LOCK.open('a+');acquired=False;save()
try:
 while not acquired:
  try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB);acquired=True
  except BlockingIOError:
   if state['state']!='waiting-for-existing-lease-owner':state['state']='waiting-for-existing-lease-owner';save();print('Lease busy: no product started; awaiting verified owner handoff',flush=True)
   time.sleep(.2)
 state.update(state='loader-running',lease={'path':str(LOCK),'owner':identity(os.getpid()),'acquired_epoch':time.time(),'before_any_ldd_or_product_child':True});save();print('LEASE_ACQUIRED',os.getpid(),flush=True)
 for name,command in[('actual-new-App-ldd',['/usr/bin/ldd',str(APP)]),('actual-new-App-version',[str(APP),'--version'])]:
  def progress(row):state['current_step']={'name':name,**row};save()
  row=run(command,cwd=root,env=env,log=P/(name+'.log'),timeout=30,max_rss=4*2**30,progress=progress);row['name']=name;state['steps'].append(row);save()
  if state['signals']:raise SystemExit(128+state['signals'][-1])
  assert row.get('exit')==0 and not row.get('error'),('Authorized loader/version step failed',name,row)
  assert not[v for v in row['known_processes']if alive(v)]
  if name=='actual-new-App-ldd':
   text=(P/(name+'.log')).read_text();assert 'not found'not in text;paths=re.findall(r'(?:=>\s+|^\s*)(/[^\s]+)',text,re.M);actual=[]
   for path in paths:
    physical=str(Path(path).resolve());assert path in boundary['pins']and physical in boundary['pins'];digest=sha(path);assert digest==boundary['pins'][path]==boundary['pins'][physical]
    actual.append({'loader_path':path,'resolved':physical,'sha256':digest})
   assert actual==expected_rows,('Actual App1bc loader providers differ from exact prior canonical boundary',actual,expected_rows)
   (P/'actual-loader-providers.json').write_text(json.dumps({'scope':'Actual fresh App1bc ldd; exact unchanged canonicalSDK/BDF/external providers from9497 boundary','application':str(APP),'application_sha256':APP_SHA,'rows':actual},indent=2)+'\n');state['actual_loader_providers']={'path':str(P/'actual-loader-providers.json'),'sha256':sha(P/'actual-loader-providers.json'),'count':len(actual)}
  else:state['actual_version_stdout']=(P/(name+'.log')).read_text()
  print(name,'EXIT',row['exit'],flush=True)
 state['input_drift']=[p for p,h in pins.items()if not Path(p).is_file()or sha(p)!=h];assert not state['input_drift'];state.update(exit=0,component_loader_and_version_admitted=True,no_GUI_XR_runtime_or_installation_acceptance=True)
except BaseException as exc:
 state.update(exit=128+state['signals'][-1]if state['signals']else 1,error=repr(exc))
finally:
 if acquired:fcntl.flock(fd,fcntl.LOCK_UN);state['lease']['released_epoch']=time.time()
 fd.close();state['state']='terminal';save()
print(json.dumps({k:state.get(k)for k in['state','exit','actual_loader_providers','actual_version_stdout','input_drift','error','lease']}),flush=True)
raise SystemExit(state['exit'])
