#!/usr/bin/env python3
"""Plan by default. Native gates for exact coherent combined090f Release build."""
import argparse,fcntl,hashlib,importlib.util,json,os,re,subprocess,time
from pathlib import Path
R=Path('/home/blux/de-xr/dome-sculpt');P=Path(__file__).resolve().parent
A=R/'artifacts/native90-release-planning-20261115/release-090f00-20261117-audit'
PLAN=A/'plan.json';PLAN_SHA='ccf9931035481459d7f51ec03e829ee2572e424d381d2bbc69a29c1fe26e1661'
HEAD='090f009ede2c0623ca616fe892223b4f68f1c2c4'
MONITOR=R/'artifacts/artist-prepared-strict-monitor-20261115/owned_monitor_v2.py'
MONITOR_SHA='b510ef5655e3214bd38e7b5f56022c37a79879070c3733f2dd059bad74f21269'
CONTRACT=R/'artifacts/guard87-validation-fresh-v2-20261115'
RSS=6*1024**3

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def write(p,j):Path(p).write_text(json.dumps(j,indent=2)+'\n')
def add(pins,p,want=None):
 p=str(p);value=sha(p);assert want is None or value==want,('Input changed',p);assert p not in pins or pins[p]==value,('Conflicting expected hash',p);pins[p]=value

def main(a):
 assert sha(PLAN)==PLAN_SHA;plan=json.loads(PLAN.read_text());B=Path(plan['destination']);S=Path(plan['source_path']);required=plan['selected_CTests']
 assert plan['source']==HEAD and len(required)==len(set(required))==90
 phase={'state':'prepared-NOT-executed','source':HEAD,'phase':'native90','build':str(B),'plan':{'path':str(PLAN),'sha256':PLAN_SHA},'requires_owning_status':str(A/'execution-native90-v1/status.json'),'limits':{'preflight_seconds':600,'postflight_seconds':600,'selected_native_seconds':2400,'default_individual_CTest_seconds':180,'workspace27_seconds':300,'workspace373_seconds':300,'raw90159_seconds':300,'app_version_seconds':60,'aggregate_command_RSS_bytes':RSS},'selected_native_tests':required,'scope':'Exact coherent090f Release owning1036 TUs,291 fresh/745 proved complete reused owners,737 fresh links/3 fresh generators; 90 native tests plus version/workspaces/raw controls. No GUI/physical HMD/Arc/performance/install claim.'}
 if not a.execute:print(json.dumps(phase,indent=2));return 0
 assert a.review and a.review_sha and sha(a.review)==a.review_sha;review=json.loads(a.review.read_text());assert review['state']=='reviewed-native90-Release'and review['driver_sha256']==sha(__file__)and review['source']==HEAD and review['plan_sha256']==PLAN_SHA
 assert sha(MONITOR)==MONITOR_SHA;spec=importlib.util.spec_from_file_location('qualified_owned_monitor',MONITOR);monitor=importlib.util.module_from_spec(spec);spec.loader.exec_module(monitor)
 out=P/a.tag;out.mkdir(exist_ok=False);state={**phase,'state':'preparing','exit':1,'steps':[],'pid':os.getpid()};pins={};lease=None;all_owned=[]
 def save():write(out/'status.json',state)
 def audit_pins(label,seconds):
  start=time.monotonic();bad=[]
  for path,want in pins.items():
   if not Path(path).is_file()or sha(path)!=want:bad.append(path)
   assert time.monotonic()-start<seconds,(label,'input audit deadline')
  return bad
 try:
  assert sha(MONITOR)==MONITOR_SHA;spec=importlib.util.spec_from_file_location('qualified_native90_owned_monitor',MONITOR);monitor=importlib.util.module_from_spec(spec);spec.loader.exec_module(monitor)
  assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=S,text=True).strip()==HEAD and not subprocess.check_output(['git','status','--porcelain'],cwd=S,text=True)
  assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=S,text=True).strip()==HEAD and not subprocess.check_output(['git','status','--porcelain'],cwd=S,text=True).strip()
  status=A/'execution-native90-v1/status.json';actual=json.loads(status.read_text());assert actual['state']=='terminal'and actual['exit']==0 and actual['source']==HEAD and not actual['input_drift']and not actual['remaining_owned_same_start']and actual['external_signal']is None
  assert [actual[k]for k in ['completed_compilations','completed_links','completed_generators','staged_whole_objects']]==[291,737,3,745]
  assert actual['plan']=={'path':str(PLAN),'sha256':PLAN_SHA};add(pins,PLAN,PLAN_SHA);add(pins,status,review['owning_status_sha256'])
  for path,want in plan['pins'].items():
   assert path not in pins or pins[path]==want;pins[path]=want
  inputs=actual['inputs'];add(pins,inputs['path'],inputs['sha256'])
  for path,want in json.loads(Path(inputs['path']).read_text()).items():
   assert path not in pins or pins[path]==want;pins[path]=want
  outputs=actual['owned_outputs'];add(pins,outputs['path'],outputs['sha256']);owned=json.loads(Path(outputs['path']).read_text());keys=set(plan['objects'])|set(plan['links'])|set(plan['generated_outputs']);assert set(owned)==keys and len(owned)==outputs['count']==1776
  for key,want in owned.items():
   path=str(B/key);assert path not in pins or pins[path]==want;pins[path]=want
  dep=actual['actual_current_dependency_audit'];add(pins,dep['path'],dep['sha256']);d=json.loads(Path(dep['path']).read_text());assert d['state']=='passed'and not d['errors']and dep['owners']==d['owners_visited']==1036 and dep['errors']==0 and set(d['records'])==set(plan['objects'])
  routes={'compile-complete-TU':0,'reuse-authenticated-whole-object':0}
  for key,row in d['records'].items():
   routes[row['route']]+=1
   if row['newly_compiled']:
    assert row['route']=='compile-complete-TU';add(pins,row['depfile'],row['actual_MD']['raw_sha256'])
   else:
    assert row['route']=='reuse-authenticated-whole-object'and row['current_source_equivalence_separate_from_MD'];add(pins,row['original_depfile'],row['actual_original_MD']['raw_sha256']);add(pins,row['unchanged_snapshot'],row['actual_original_MD']['raw_sha256']);assert not(B/plan['objects'][key]['depfile']).exists()
  assert routes=={'compile-complete-TU':291,'reuse-authenticated-whole-object':745};del d
  import sys
  sys.path.insert(0,str(CONTRACT));from runtime_contract import read_runtime,merge_pins
  runtime=read_runtime();merge_pins(pins,runtime['pins'])
  for p in [Path(__file__).resolve(),MONITOR,CONTRACT/'runtime_contract.py',CONTRACT/'runtime-preflight.json',a.review.resolve(),Path('/usr/bin/ctest'),Path('/usr/bin/python3')]:add(pins,p);add(pins,p.resolve())
  assert not audit_pins('before',600);state.update(owning_status_sha256=sha(status),owned_outputs_count=len(owned),actual_MD_owners=1036,dependency_routes=routes);write(out/'inputs-before.json',pins)
  env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))}
  for k in ['ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','LD_PRELOAD','DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','DBUS_SESSION_BUS_ADDRESS','__EGL_VENDOR_LIBRARY_FILENAMES']:env.pop(k,None)
  env.update(LD_LIBRARY_PATH=plan['environment']['LD_LIBRARY_PATH'],LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',OPENBLAS_NUM_THREADS='2',OMP_NUM_THREADS='2',EGL_PLATFORM='surfaceless',XR_RUNTIME_JSON=str(out/'no-physical-runtime.json'),CTEST_PARALLEL_LEVEL='1',PYTHONDONTWRITEBYTECODE='1')
  for key,leaf in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[key]=str(out/leaf);Path(env[key]).mkdir(mode=0o700)
  appearance=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';appearance.parent.mkdir();appearance.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':1}));add(pins,appearance)
  state['environment']={k:env.get(k)for k in ['LD_LIBRARY_PATH','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','LIBGL_ALWAYS_SOFTWARE','LD_PRELOAD','LP_NUM_THREADS','CTEST_PARALLEL_LEVEL']}
  lease=Path('/tmp/dome-sculpt-arc-device.lock').open('a+');fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB);state['lease']={'owner_pid':os.getpid(),'before_any_product':True,'acquired_epoch':time.time()};save()
  def step(name,command,seconds,expected=None):
   def progress(row):state['live']={'name':name,**row};save()
   row=monitor.run(command,cwd=B,env=env,log=out/(name+'.log'),timeout=seconds,max_rss=RSS,progress=progress);row.update(name=name,log_sha256=sha(out/(name+'.log')));all_owned.extend(row.get('known_processes',[]));state['steps'].append(row);state.pop('live',None);save();print(name,row.get('exit'),flush=True)
   assert row['exit']==0 and 'error'not in row and not row['signals'],(name,row)
   text=(out/(name+'.log')).read_text()
   if expected:assert expected in text,(name,'exact expected count missing',expected)
   return text
  step('app-version',[str(B/'dome-sculpt'),'--version'],60)
  regex='^('+'|'.join(re.escape(n)for n in required)+')$'
  listing=step('selected-CTest-list',['/usr/bin/ctest','--test-dir',str(B),'--show-only=json-v1','-R',regex],60);selected=json.loads(listing)['tests'];assert sorted(x['name']for x in selected)==sorted(required)and all(x.get('command')and Path(x['command'][0]).is_file()for x in selected)and all(not any(p['name']=='DISABLED'and p['value']for p in x.get('properties',[]))for x in selected)
  text=step('native90',['/usr/bin/ctest','--test-dir',str(B),'--output-on-failure','--timeout','180','--output-junit',str(out/'native90.xml'),'-R',regex],2400)
  passed=re.findall(r'Test\s+#\d+:\s+(\S+)\s+\.+\s+Passed',text);assert sorted(passed)==sorted(required)and len(passed)==90 and '***Skipped'not in text and '***Not Run'not in text;state['exact_native_passed']=sorted(passed);save()
  step('workspace27',[str(B/'dome-sculpt-sculpt-chunk-workspace-test')],300,'PASS total 27');step('workspace373',[str(B/'dome-sculpt-sculpt-placed-workspace-test')],300,'PASS 373 placed-owner workspace controls');step('raw90159',[str(B/'dome-sculpt-baked-emission-workflow-test'),'--raw-source-worker-controls',str(out/'raw-uv1-worker')],300,'Baked emission raw UV1 worker 90159 checks PASS')
  state['exit']=0
 except BaseException as e:state.update(error=repr(e),exit=1)
 finally:
  try:state['input_drift']=audit_pins('after',600)
  except BaseException as error:state.update(postflight_error=repr(error),input_drift=['postflight audit incomplete'],exit=1)
  state['remaining_owned_processes']=[r for r in all_owned if monitor.alive(r)];state['exit']=state['exit']or int(bool(state['input_drift'])or bool(state['remaining_owned_processes']));write(out/'inputs-final.json',pins)
  if lease:state['lease']['released_epoch']=time.time();fcntl.flock(lease,fcntl.LOCK_UN);lease.close()
  state['state']='terminal';save();print('TERMINAL',state['exit'],state.get('error'),flush=True)
 return state['exit']
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--execute',action='store_true');p.add_argument('--tag',default='native90-v1');p.add_argument('--review',type=Path);p.add_argument('--review-sha');raise SystemExit(main(p.parse_args()))
