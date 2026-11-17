"""Plan by default: direct ldd and ordinary --version of sealed whole090f App."""
from pathlib import Path
import argparse,fcntl,hashlib,importlib.util,json,os,re,subprocess,time
R=Path('/home/blux/de-xr/dome-sculpt');P=Path(__file__).resolve().parent
HEAD='090f009ede2c0623ca616fe892223b4f68f1c2c4';APP=R/'artifacts/native90-release-planning-20261115/release-090f00-20261117-owning/dome-sculpt';APP_SHA='9143ea5921d35a3612ab452c16eee086403813343a88b3d775196a8507eb8af1'
NATIVE=R/'artifacts/native90-release-gates-20261117/native90-v1-20261117/status.json';NATIVE_SHA='8f496335c8b92cedc8887a9a53112d61c798ab20ecb465b37dbc853ceeddaaf2'
OWN=R/'artifacts/native90-release-planning-20261115/release-090f00-20261117-audit/execution-native90-v1/status.json';OWN_SHA='3c4fd01b2a6702ee0069a744375b9d4a087ef4d8ed6a7faff829d22fe49be36c'
PRIOR=R/'artifacts/app1bc-component-loader-20261115/actual-loader-providers.json';PRIOR_SHA='40f45fe0f524f659d3aa15b1ecfdb4c5250b1f8968e6fab74c2359703055d537'
M=R/'artifacts/artist-prepared-strict-monitor-20261115/owned_monitor_v2.py';M_SHA='b510ef5655e3214bd38e7b5f56022c37a79879070c3733f2dd059bad74f21269'
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def write(p,j):Path(p).write_text(json.dumps(j,indent=2)+'\n')
def add(pins,path,want=None):
 path=str(path);v=sha(path);assert want is None or want==v,('Changed input',path);assert path not in pins or pins[path]==v,('Conflicting expected pin',path);pins[path]=v

def main(a):
 plan={'state':'prepared-NOT-executed','source':HEAD,'application':{'path':str(APP),'sha256':APP_SHA},'requires_native90_status':{'path':str(NATIVE),'sha256':NATIVE_SHA},'requires_owning_status':{'path':str(OWN),'sha256':OWN_SHA},'exact_effective_provider_reference':{'path':str(PRIOR),'sha256':PRIOR_SHA,'count':72},'commands':[['/usr/bin/ldd',str(APP)],[str(APP),'--version']],'limits':{'each_command_seconds':30,'aggregate_command_RSS_bytes':4*2**30},'scope':'Whole090f App direct loader and ordinary version only. Actual mapped ELF witnesses belong to forthcoming separately reviewed GUI/XR monitor, not this headless phase. No GUI/GL/HMD/install claim.'}
 if not a.execute:print(json.dumps(plan,indent=2));return 0
 assert a.review and a.review_sha and sha(a.review)==a.review_sha;review=json.loads(a.review.read_text());assert review['state']=='reviewed-whole090f-loader'and review['driver_sha256']==sha(__file__)and review['application_sha256']==APP_SHA and review['owning_status_sha256']==OWN_SHA and review['native90_status_sha256']==NATIVE_SHA
 out=P/a.tag;out.mkdir(exist_ok=False);pins={};state={**plan,'state':'preparing','exit':1,'steps':[]};lease=None
 assert sha(M)==M_SHA;spec=importlib.util.spec_from_file_location('qualified_whole_app_owned_monitor',M);monitor=importlib.util.module_from_spec(spec);spec.loader.exec_module(monitor)
 def save():write(out/'status.json',state)
 def drift():return[p for p,h in pins.items()if not Path(p).is_file()or sha(p)!=h]
 try:
  for p,h in [(APP,APP_SHA),(NATIVE,NATIVE_SHA),(OWN,OWN_SHA),(PRIOR,PRIOR_SHA),(M,M_SHA)]:add(pins,p,h)
  native=json.loads(NATIVE.read_text());own=json.loads(OWN.read_text());assert native['state']==own['state']=='terminal'and native['exit']==own['exit']==0 and native['source']==own['source']==HEAD and not native['input_drift']and not own['input_drift']and not native['remaining_owned_processes']and not own['remaining_owned_same_start']and len(native['exact_native_passed'])==90 and native['owned_outputs_count']==1776 and native['actual_MD_owners']==1036
  nf=NATIVE.parent/'inputs-final.json';add(pins,nf)
  for p,h in json.loads(nf.read_text()).items():assert p not in pins or pins[p]==h;pins[p]=h
  owned=own['owned_outputs'];add(pins,owned['path'],owned['sha256']);assert json.loads(Path(owned['path']).read_text())['dome-sculpt']==APP_SHA
  source=R/'artifacts/authoring-normal-integration-20261115';assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==HEAD and not subprocess.check_output(['git','status','--porcelain'],cwd=source,text=True)
  subprocess.run(['git','diff','--exit-code','1bc13125',HEAD,'--','src','include','third_party'],cwd=source,check=True,stdout=subprocess.DEVNULL)
  for p in [Path(__file__).resolve(),a.review.resolve(),Path('/usr/bin/ldd')]:add(pins,p)
  expected=json.loads(PRIOR.read_text())['rows'];assert len(expected)==72;assert not drift();write(out/'inputs-before.json',pins)
  env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))}
  for k in ['ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','LD_PRELOAD','DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','DBUS_SESSION_BUS_ADDRESS','BASH_ENV','ENV']:env.pop(k,None)
  for k,leaf in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[k]=str(out/leaf);Path(env[k]).mkdir(mode=0o700)
  env.update(LD_LIBRARY_PATH='/home/blux/de-xr/dome-sculpt/artifacts/sdk-20261111/toolkit-release/lib:/opt/fltk-vr/bdf-runtime/lib:/usr/lib64',XR_RUNTIME_JSON=str(out/'runtime/no-physical-XR.json'),OPENBLAS_NUM_THREADS='2',OMP_NUM_THREADS='2',LP_NUM_THREADS='2')
  lease=Path('/tmp/dome-sculpt-arc-device.lock').open('a+');fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB);state['lease']={'before_any_ldd_or_product':True,'owner_pid':os.getpid(),'acquired_epoch':time.time()};save()
  for name,cmd in zip(['ldd','version'],plan['commands']):
   def progress(row):state['live']={'name':name,**row};save()
   row=monitor.run(cmd,cwd=out,env=env,log=out/(name+'.log'),timeout=30,max_rss=4*2**30,progress=progress);row['name']=name;state['steps'].append(row);state.pop('live',None);save();assert row['exit']==0 and 'error'not in row and not row['signals']and not[v for v in row['known_processes']if monitor.alive(v)],(name,row)
   text=(out/(name+'.log')).read_text()
   if name=='ldd':
    assert 'not found'not in text;rows=[]
    for path in re.findall(r'(?:=>\s+|^\s*)(/[^\s]+)',text,re.M):
     resolved=str(Path(path).resolve());assert path in pins and resolved in pins and sha(path)==pins[path]==pins[resolved];rows.append({'loader_path':path,'resolved':resolved,'sha256':sha(path)})
    assert rows==expected,('Actual wholeApp loader differs from prior exact effective72',rows,expected);write(out/'actual-loader-providers.json',{'application':str(APP),'application_sha256':APP_SHA,'rows':rows});state['actual_loader_providers']={'path':str(out/'actual-loader-providers.json'),'sha256':sha(out/'actual-loader-providers.json'),'count':len(rows)}
   else:assert text=='Dome Sculpt 0.1.0 / CPU / Blender 5.4 Alpha trackball basis\n';state['actual_version_stdout']=text
  state.update(exit=0,production_body_exact='1bc13125',native90_admission={'path':str(NATIVE),'sha256':NATIVE_SHA},whole_owning_admission={'path':str(OWN),'sha256':OWN_SHA})
 except BaseException as e:state.update(error=repr(e),exit=1)
 finally:
  state['input_drift']=drift();state['exit']=state['exit']or int(bool(state['input_drift']));write(out/'inputs-final.json',pins)
  if lease:state['lease']['released_epoch']=time.time();fcntl.flock(lease,fcntl.LOCK_UN);lease.close()
  state['state']='terminal';save();print('TERMINAL',state['exit'],state.get('error'),flush=True)
 return state['exit']
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--execute',action='store_true');p.add_argument('--review',type=Path);p.add_argument('--review-sha');p.add_argument('--tag',default='whole090f-loader-v1-20261117');raise SystemExit(main(p.parse_args()))
