#!/usr/bin/env python3
"""Plan by default. Exactguard87 current-owning gates precede every public app launch."""
import argparse,fcntl,importlib.util,json,os,re,signal,subprocess,sys,time,types
from pathlib import Path
_spec=importlib.util.spec_from_file_location('source8_cohort_guard87',Path(__file__).resolve().parent/'cohort.py');_cohort=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_cohort)
P,R,S,B,HEAD,TARGETS,G,sha,write,verify,admit=(_cohort.__dict__[key]for key in ['P','R','S','B','HEAD','TARGETS','G','sha','write','verify','admit'])
from process_observer import ProcessObserver
from monitor_lifetime_v3 import checked_monitor
from runtime_contract import read_runtime,merge_pins
XVFB_BUNDLE=R/'artifacts/guard87-xvfb-private-20261115'
_xspec=importlib.util.spec_from_file_location('private_xvfb_identity',XVFB_BUNDLE/'xvfb_identity.py');_xidentity=importlib.util.module_from_spec(_xspec);_xspec.loader.exec_module(_xidentity)
XVFB_HEALTH=XVFB_BUNDLE/'health-v2/status.json'
XVFB_HEALTH_SHA='b5e8c05b161194e64d721cae422badcde8b395a38d82c74c7bd1b9f1bc3c64eb'
def xvfb_contract():
 assert sha(XVFB_HEALTH)==XVFB_HEALTH_SHA
 j=json.loads(XVFB_HEALTH.read_text());assert j['state']=='terminal'and j['exit']==0 and not j['input_drift']and not j['remaining_owned']and j['actual_XTEST_key_received']and j['screen']==[1100,800]
 assert not verify(j['pins']);return j
C=R/'artifacts/strict-arc-20261111/control'
BUNDLE=R/'artifacts/gui-recovery-20261111'
OBS=R/'artifacts/paint-workflow-dso-build-20261111'
LIMITS={'gui':(900,6*2**30),'xr':(240,4*2**30)}
def pin(p):return {'path':str(p),'sha256':sha(p)}
def plan(a):
 return {'state':'planned-NOT-executed','source':HEAD,'profile':a.profile,'phase':a.phase,'case':a.case,'targets':TARGETS,'original_limits':{'GUI_step_seconds':50,'GUI_phase_seconds':900,'XR_physical_frames':6000,'XR_seconds':240},'external_limits':dict(zip(['seconds','descendant_RSS_bytes'],LIMITS[a.phase])),'requires_current_owning_native87_workspace_raw_producer_font_manual_software':True,'private_Xvfb_expected_health_sha256':XVFB_HEALTH_SHA,'critical_witness_policy':'Only a completed matching expected path/SHA+map/stat inode+mount-certified ELF observation may promote a witness; v2 exact-retirement exception classifier unchanged','common_graphics_lock':'/tmp/dome-sculpt-arc-device.lock','scenario_provenance':pin(P/'scenario-provenance.json'),'scenario_paths':[str(P/'scenarios'/n)for n in ['source8-public-menu.py','source8-public-xr.py']],'scope':'Actual exported desktop menus or actual-loader stereo app with synthetic actions, genuine UV1/currentUV0 Source8/FLOAT inputs and independent normal/radiance controls. Functional only; no physicalHMD/performance claim.'}
def environment(profile,out):
 env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_','MESA_','LIBGL_','GALLIUM_','DRI_'))}
 for k in ['LD_PRELOAD','DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','XR_RUNTIME_JSON','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS']:env.pop(k,None)
 env.update(PATH=str(XVFB_BUNDLE/'bin')+':'+str(P/'bin')+':'+str(BUNDLE/'bin')+':'+os.environ['PATH'],LD_LIBRARY_PATH=str(R/('artifacts/sdk-20261111/toolkit-'+profile+'/lib'))+':/opt/fltk-vr/bdf-runtime/lib:/usr/lib64',LP_NUM_THREADS='2',MESA_SHADER_CACHE_DISABLE='1',PYTHONDONTWRITEBYTECODE='1')
 task_root=out/'home';task_root.mkdir()
 for k,n in [('XDG_RUNTIME_DIR','runtime'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_STATE_HOME','state')]:
  env[k]=str(task_root/n);Path(env[k]).mkdir(mode=0o700)
 if profile=='sanitize':env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 return env

def execute(a):
 cohort=admit(a.profile);out=P/(a.profile+'-'+a.phase+'-'+a.case+'-'+a.tag);out.mkdir(exist_ok=False)
 pins=dict(cohort['pins']);xf=xvfb_contract();merge_pins(pins,xf['pins']);pins[str(XVFB_HEALTH)]=XVFB_HEALTH_SHA;write(out/'provider-admission.json',cohort);pins[str(out/'provider-admission.json')]=sha(out/'provider-admission.json')
 targets={k:Path(v['path'])for k,v in cohort['targets'].items()};env=environment(a.profile,out)
 for f in [Path(__file__).resolve(),P/'cohort.py',P/'process_observer.py',P/'monitor_lifetime_v2.py',P/'monitor_lifetime_v3.py',P/'scenario-provenance.json',P/'bin/Xvfb']:pins[str(f)]=sha(f)
 lease=None;active=None;all_owned={};app_owned={};seen=set();mapped={};dris={};apps=set();peak=0
 xvfb_certified={};critical={};critical_stat={};critical_expectations={};state={**plan(a),'state':'preparing','pins':pins,'external_guard':None,'exit':1,'authorization':'Existing root session authorization, conditional on exact current-owning native/input/provider gates; this record does not represent a separate signed clearance.'}
 sys.path.insert(0,str(C));spec=importlib.util.spec_from_file_location('monitor',C/'graphics-v3.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
 for f in [C/'graphics-v3.py',C/'mount_identity.py']:pins[str(f)]=sha(f)
 proc_observer=ProcessObserver(out,g.identity)
 def critical_file(name,f):
  f=Path(f);st=f.stat();ident=g.current_identity(f);critical[name]=(ident['map_device'],st.st_ino);critical_stat[name]=(st.st_dev,st.st_ino);actual=sha(f);expected=pins.get(str(f),pins.get(str(f.resolve())));assert expected is not None and expected==actual,('Expected owning provider absent/changed',name,str(f));pins.setdefault(str(f),expected);critical_expectations[name]={'path':str(f.resolve()),'sha256':expected}
 try:
  arc=G/'release-guard87-admitted-v3-manual-arc/status.json';arc_gate=json.loads(arc.read_text());assert arc_gate['state']=='terminal'and arc_gate['exit']==0 and not arc_gate['input_drift']and arc_gate['source']==HEAD;pins[str(arc)]=sha(arc)
  if a.phase=='xr':
   gui=P/(a.profile+'-gui-'+a.case+'-'+a.gui_tag)/'status.json';prior=json.loads(gui.read_text());assert prior['state']=='terminal'and prior['exit']==0 and not prior['input_drift']and not prior['remaining_owned_processes']and prior['source']==HEAD;assert prior['checks']>=10;pins[str(gui)]=sha(gui)
   arc=G/'release-guard87-admitted-v3-manual-arc/status.json';arc_gate=json.loads(arc.read_text());assert arc_gate['state']=='terminal'and arc_gate['exit']==0 and not arc_gate['input_drift']and arc_gate['source']==HEAD;pins[str(arc)]=sha(arc)
  suite=Path(cohort['fixture_root'])/'manifest.json';m=json.loads(suite.read_text());assert m['version']==2 and set(m['cases'])=={'pbr-quarter','pbr-tiny','mtoon-quarter','mtoon-tiny'}
  manifest=suite.parent/m['cases'][a.case];case=json.loads(manifest.read_text());assert case['version']==2 and case['kind']=='source8-uv1-float-public'and case['resolution']==64 and case['original_texcoord']==1 and case['current_texcoord']==0 and case['scene_version']==42 and case['paint_version']==8
  assert case['hidden_locked_source']and case['hidden_target_tiles']and case['hidden_sibling_physical_tiles']and case['independent_native_oracle']and case['independent_texel_oracle']
  assert case['visible_maximum_rgb_difference']==3 and case['visible_minimum_case_normal_effect']==7 and case['visible_minimum_geometry_difference']==12
  for f in [suite,manifest,*(manifest.parent/v for v in case['files'].values()),*(manifest.parent/v for v in case['view_files'].values())]:pins[str(f)]=sha(f)
  script=P/'scenarios'/('source8-public-menu.py'if a.phase=='gui'else'source8-public-xr.py');pins[str(script)]=sha(script)
  if a.phase=='gui':
   command=[str(BUNDLE/'venv/bin/python'),str(script),str(targets['app']),str(targets['workflow']),str(manifest),str(out/'workflow')];env.update(DOME_BAKED_EMISSION_MENU_SCREEN='1100x800x24',LIBGL_ALWAYS_SOFTWARE='1')
   for f in [P/'bin/Xvfb',BUNDLE/'bin/Xvfb',BUNDLE/'deps/usr/bin/Xvfb',BUNDLE/'venv/bin/python']:pins[str(f)]=sha(f)
   critical_file('consumer',targets['app']);critical_file('canonical_FLTK',R/'artifacts/sdk-20261111/toolkit-release/lib/libfltk.so');critical_file('system_GLX',Path('/usr/lib64/libGLX.so.0'));critical_file('system_gallium',Path(read_runtime()['roles']['system_gallium']))
  else:
   ref=Path(cohort['font_reference']);fd=json.loads(ref.read_text())
   for f in [ref,Path(fd['font_file'])]:pins[str(f)]=sha(f)
   observer=OBS/'device-auth-all-egl-release.so'
   for f in [observer,OBS/'device-auth-all-egl.cpp',OBS/'device-auth-all-egl-derivation.json']:pins[str(f)]=sha(f)
   command=[sys.executable,str(script),str(targets['app']),str(targets['runtime']),str(targets['workflow']),str(manifest),str(ref),str(out/'workflow')]
   env.update(DRI_PRIME='pci-0000_03_00_0',MESA_LOADER_DRIVER_OVERRIDE='iris',EGL_PLATFORM='surfaceless',__EGL_VENDOR_LIBRARY_FILENAMES='/usr/share/glvnd/egl_vendor.d/50_mesa.json',DOME_BAKED_EMISSION_XR_REQUIRE_RENDERER='Arc',DOME_BAKED_EMISSION_APP_PRELOAD=str(observer),DOME_MARKER_DEVICE_REPORT=str(out/'actual-device'))
   for k,f in [('consumer',targets['app']),('runtime',targets['runtime']),('observer',observer),('system_EGL',Path('/usr/lib64/libEGL_mesa.so.0.0.0')),('system_gallium',Path(read_runtime()['roles']['system_gallium'])),('canonical_FLTK',R/'artifacts/sdk-20261111/toolkit-release/lib/libfltk.so'),('real_OpenXR_loader',Path('/usr/lib64/libopenxr_loader.so.1'))]:critical_file(k,f)
  lease=Path('/tmp/dome-sculpt-arc-device.lock').open('a+');fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB);state.update(lease_acquired_epoch=time.time(),lease_released_epoch=None,lease_before_any_graphics_child=True)
  assert not verify(pins)
  state.update(command=command,state='running',critical_expected_ELFs=critical_expectations,environment={k:env.get(k)for k in ['LD_LIBRARY_PATH','LD_PRELOAD','DOME_BAKED_EMISSION_APP_PRELOAD','LIBGL_ALWAYS_SOFTWARE','DRI_PRIME','MESA_LOADER_DRIVER_OVERRIDE','ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS']});write(out/'status.json',state)
  start=time.monotonic();seconds,rsscap=LIMITS[a.phase]
  with (out/'process.log').open('w')as log:
   active=subprocess.Popen(command,env=env,cwd=BUNDLE,stdout=log,stderr=subprocess.STDOUT,start_new_session=True,preexec_fn=g.no_core);state['pid']=active.pid;write(out/'status.json',state)
   while active.poll()is None:
    rss=0
    for pid in g.descendants(active.pid):
     identity=g.identity(pid)
     if not identity:continue
     all_owned[pid]=identity;proc_observer.capture(pid,identity,'live')
     try:
      if a.phase=='gui':
       actual_exe=Path('/proc',str(pid),'exe').stat();expected_exe=(XVFB_BUNDLE/'deps/usr/bin/Xvfb').stat()
       if(actual_exe.st_dev,actual_exe.st_ino)==(expected_exe.st_dev,expected_exe.st_ino):
        current_maps=Path('/proc',str(pid),'maps').read_text();map_paths=sorted({line.split(None,5)[5]for line in current_maps.splitlines()if len(line.split(None,5))==6 and line.split(None,5)[5].startswith('/')and not line.split(None,5)[5].endswith('(deleted)')})
        if xvfb_certified.get(str(pid),{}).get('mapped_paths')!=map_paths:
         proof=_xidentity.certify(pid,pins,sealing=False);assert proof['identity']['start']==identity['start'],'Observed Xvfb PID identity changed';certificate=out/('actual-private-Xvfb-'+str(pid)+'.json');write(certificate,proof);xvfb_certified[str(pid)]={'identity':identity,'mapped_paths':map_paths,'certificate_path':str(certificate),'certificate_sha256':sha(certificate),'mapped_ELFs':len(proof['files'])}
      for line in Path('/proc',str(pid),'status').read_text().splitlines():
       if line.startswith('VmRSS:'):rss+=int(line.split()[1])*1024
      if a.phase in ['gui','xr']and Path('/proc',str(pid),'exe').resolve()==targets['app']:
       apps.add(pid);stage=out/('app-'+str(pid));stage.mkdir(exist_ok=True);checked_monitor(g.monitor,g.identity,types.SimpleNamespace(pid=pid),identity,app_owned,critical,critical_stat,seen,mapped,dris,stage,out/'retired-mountinfo-reads.jsonl',critical_expectations)
     except(FileNotFoundError,ProcessLookupError):pass
    if a.phase in ['producer','font']:g.monitor(active,app_owned,critical,critical_stat,seen,mapped,dris,out)
    peak=max(peak,rss)
    if peak>rsscap:state['external_guard']='descendant RSS';raise RuntimeError(('RSS',peak,rsscap))
    if time.monotonic()-start>=seconds:state['external_guard']='wall deadline';raise TimeoutError(seconds)
    time.sleep(.01)
   code=active.wait()
  state.update(process_exit=code,seconds=time.monotonic()-start,peak_RSS=peak,actual_app_pids=sorted(apps),critical_observed=sorted(seen))
  if lease:fcntl.flock(lease,fcntl.LOCK_UN);lease.close();lease=None;state['lease_released_epoch']=time.time();write(out/'status.json',state)
  state['remaining_owned_processes']=g.alive(all_owned)
  proc_observer.snapshot(all_owned,'driver-return')
  assert not state['remaining_owned_processes'],('owned process still live',state['remaining_owned_processes'])
  assert code==0,('actual process failure',code)
  if a.phase=='gui':assert xvfb_certified,'Actual private Xvfb identity/closure never certified';state['actual_private_Xvfb']=xvfb_certified
  assert set(critical)<=seen,('missing actual ELF witness',set(critical)-seen)
  logs=[out/'process.log',*(out/'workflow').rglob('app*.log')]
  assert all(not re.search(r'ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:',f.read_text(errors='replace'))for f in logs)
  if a.phase=='producer':
   m=json.loads((out/'native/manifest.json').read_text());assert m['version']==1 and m['resolution']==64 and m['peak']==52016 and m['hidden_locked_source']and m['independent_pixel_oracle']
   match=re.findall(r'^Baked emission public producer (\d+) checks PASS$',(out/'process.log').read_text(),re.M);assert len(match)==1;state['native_checks']=int(match[0]);state['manifest']=pin(out/'native/manifest.json');state['native_files']={k:pin(out/'native'/v)for k,v in m['files'].items()}
  elif a.phase=='font':
   fd=json.loads((out/'titles.json').read_text())
   assert fd['version']==1 and fd['font_pixels']==16 and fd['unit']==1 and fd['width']==fd['height']==640 and fd['opacity']==255
   assert set(fd['masks'])=={'Dome Sculpt','Objects','Mesh tools','Mesh baking'} and all(v['ink']for v in fd['masks'].values())
   state.update(reference=pin(out/'titles.json'),font=pin(Path(fd['font_file'])),style={k:fd[k]for k in ['font_pixels','unit','light','opacity']})
  else:
   report=json.loads((out/'workflow/report.json').read_text());assert report['complete']and not report['error']and not report['drift']and all(v['passed']for v in report['checks']);assert len(report['checks'])>=(10 if a.phase=='gui'else 33);state.update(checks=len(report['checks']),report=pin(out/'workflow/report.json'))
   if a.phase=='xr':
    devices=list((out/'actual-device').glob('*/device.txt'));assert devices
    for f in devices:
     d=dict(x.split('=',1)for x in f.read_text().splitlines()if '='in x and not x.startswith('fd='))
     assert 'Arc'in d['renderer']and'A770'in d['renderer']and d['vendor']=='Intel'and d['render_node']=='/dev/dri/renderD129'and d['pci'].endswith('/0000:03:00.0')and d['vendor_id']=='0x8086'and d['device_id']=='0x56a0'
     assert int(d['egl_context'],16)and int(d['egl_display'],16)and re.search(r'^fd=\d+$',f.read_text(),re.M)and (f.parent/'context-maps.txt').is_file()
    assert Path('/sys/dev/char/226:129/device/driver').resolve().name=='i915';state['device_reports']={str(f):sha(f)for f in devices}
  state['exit']=0
 except BaseException as e:
  state.update(exit=1,error=repr(e))
  proc_observer.snapshot(all_owned,'failure-observed')
  proc_observer.snapshot(all_owned,'immediately-before-cleanup')
  if active is not None and (active.poll()is None or g.alive(all_owned)):g.cleanup(active,all_owned)
  proc_observer.snapshot(all_owned,'after-cleanup')
 finally:
  if lease:fcntl.flock(lease,fcntl.LOCK_UN);lease.close();state['lease_released_epoch']=time.time()
  proc_observer.snapshot(all_owned,'finally')
  state.update(state='terminal',input_drift=verify(pins));state['exit']=state['exit']or int(bool(state['input_drift']));write(out/'status.json',state);write(out/'mapped-ELFs.json',mapped);write(out/'readable-DRI.json',dris)
 print(json.dumps({k:state.get(k)for k in ['state','exit','pid','checks','native_checks','seconds','error','lease_released_epoch']}),flush=True)
 return state['exit']
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('profile',choices=['release']);ap.add_argument('phase',choices=['gui','xr']);ap.add_argument('--case',choices=['pbr-quarter','pbr-tiny','mtoon-quarter','mtoon-tiny'],required=True);ap.add_argument('--tag',default='private-xvfb-v4');ap.add_argument('--gui-tag',default='private-xvfb-v4');ap.add_argument('--execute',action='store_true');a=ap.parse_args()
 if not a.execute:print(json.dumps(plan(a),indent=2))
 else:raise SystemExit(execute(a))
