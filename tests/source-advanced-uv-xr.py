#!/usr/bin/env python3
"""Create genuine advanced normal UV sources using actual XR UI actions.
Separate from the second-Bake/history scenario; no private product callback.
"""
import hashlib,json,os,signal,subprocess,sys,time
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if len(sys.argv)!=7:raise SystemExit('usage: source-advanced-uv-xr.py APP RUNTIME INSPECTOR CASE_MANIFEST FONT_REFERENCE OUTPUT')
app,runtime,inspector,mf,reference,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
m=json.loads(mf.read_text());font=json.loads(reference.read_text());before=mf.parent/m['before'];expected=mf.parent/m['routes']['xr']['expected'];wrong=mf.parent/m['routes']['xr']['wrong_method']
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins={str(p):sha(p)for p in [app,runtime,inspector,mf,reference,Path(font['font_file']),before,expected,wrong,Path(__file__).resolve()]}
checks=[];process=None;started=time.monotonic()
def report(complete=False,error=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':error,'checks':checks,'count':len(checks),'pins':pins,'drift':[p for p,h in pins.items()if sha(p)!=h],'scope':'Actual public UV Keep detail/Turn90/Apply on genuine first Bake and fresh ordinary XR reopen. Source-use second Bake/history is a separate scenario. Both-eye captures are evidence; no physical headset or font-shape-gated timing claim.'},indent=2)+'\n')
def check(ok,label):checks.append({'passed':bool(ok),'label':label});report();print(('PASS 'if ok else'FAIL ')+label,flush=True);assert ok,label
try:
 check(m['kind']=='advanced-normal-uv-authoring'and m['genuine_first_bake']and m['source_normal_version_before']==1 and m['uv_method']in ['Rasterize','RetainSource'],'Native fixture owns a genuine ordinary first Bake and explicit publicly selectable UV method')
 previous=before
 for stage in ['author','reopen']:
  case=out/stage;case.mkdir();home=case/'home';home.mkdir();project=case/'input.dsculpt';project.write_bytes(previous.read_bytes());target=case/'final.dsculpt';graphics=case/'runtime.json';loader=case/'loader.json';loader.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
  env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_XR_','DOME_ADVANCED_'))};env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None);env.pop('WAYLAND_SOCKET',None);env.pop('LD_PRELOAD',None)
  env.update(XR_RUNTIME_JSON=str(loader),DOME_XR_FIXTURE_REPORT=str(graphics),LP_NUM_THREADS='2')
  for key,leaf in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[key]=str(home/leaf);Path(env[key]).mkdir(mode=0o700)
  appearance=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';appearance.parent.mkdir();appearance.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':1}))
  if os.environ.get('DOME_BAKED_EMISSION_APP_PRELOAD'):env['LD_PRELOAD']=os.environ['DOME_BAKED_EMISSION_APP_PRELOAD']
  if stage=='author':env.update(DOME_XR_FIXTURE_UV='1',DOME_XR_FIXTURE_NORMAL_DOMAIN='advanced-public',DOME_XR_ADVANCED_UV=m['uv_method'])
  argv=[str(app),'--xr','--open',str(project),'--frames','42'if stage=='author'else'6','--save',str(target)]
  (case/'invocation.json').write_text(json.dumps({'command':argv,'method':m['uv_method'],'scope':stage},indent=2)+'\n')
  with (case/'app.log').open('w')as log:
   process=subprocess.Popen(argv,env=env,stdout=log,stderr=subprocess.STDOUT)
   code=process.wait(timeout=max(.001,240-(time.monotonic()-started)))
  process=None;check(code==0,stage+': ordinary XR application exits cleanly after ordered native Save')
  native=json.loads(subprocess.check_output([str(inspector),'--verify-advanced-uv',str(mf),'xr','accepted'if stage=='author'else'reopened',str(target)],env=env,timeout=max(.001,240-(time.monotonic()-started))))
  (case/'native-verified.json').write_text(json.dumps(native,indent=2)+'\n');check(native['verified']and native['normal_oracle']['maximum_component_error']<.035,stage+': native endpoint/method/Original/first-recipe and independent finite both-role normal oracle pass')
  state=json.loads(graphics.read_text());check(state.get('left_eye_nonblank',0)>0 and state.get('right_eye_nonblank',0)>0,stage+': both released GL eyes contain actual presentation')
  check(state['frames_waited']==state['frames_begun']==state['frames_ended']and state['acquires']==state['image_waits']==state['releases'],stage+': real loader frame and stereo image ownership balances')
  renderer=os.environ.get('DOME_BAKED_EMISSION_XR_REQUIRE_RENDERER','Arc');check(renderer in state.get('gl_renderer','')and'llvmpipe'not in state.get('gl_renderer','').lower(),stage+': actual stereo context uses the explicitly required hardware renderer')
  if stage=='author':
   wanted={18,26}|({15}if m['uv_method']=='RetainSource'else set());press=state.get('advanced_uv_pressures',[])
   check({v['frame']for v in press}==wanted and all(v['action']=='paint_pressure'and v['pressure']>.8 for v in press),'Actual pressure edges select Keep detail only for RetainSource, then Turn90 and Apply')
   check(any(v['frame']==18 and v['x']==366 and v['y']==85 for v in state.get('advanced_uv_aims',[])),'Actual controller aim reaches the shared Turn90 button, not a replacement scene callback')
   capture=state.get('advanced_uv_captures',{});check(all(str(frame)+'-'+str(eye)in capture for frame in [14,16,19,25,27]for eye in [0,1]),'Both actual released eyes capture UV state around mode choice, rotation and Apply')
   for row in capture.values():check(Path(row['ppm']).is_file()and Path(row['rgba']).stat().st_size==row['width']*row['height']*4,'Captured UV eye has full actual PPM/RGBA payload')
  else:check(not state.get('advanced_uv_pressures'),'Fresh ordinary reopen injects no authoring pressure')
  previous=target
 check(not[p for p,h in pins.items()if sha(p)!=h],'All application/runtime/native/capsule/font inputs remain unchanged')
 report(True)
except BaseException as error:report(error=repr(error));raise
finally:
 if process is not None and process.poll()is None:
  process.terminate()
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.kill();process.wait()
