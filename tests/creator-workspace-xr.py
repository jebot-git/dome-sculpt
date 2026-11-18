#!/usr/bin/env python3
"""Public creator controls through the real loader and synthetic controller input."""
import hashlib,json,os,signal,subprocess,sys,time
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if len(sys.argv)!=7:raise SystemExit('usage: creator-workspace-xr.py APP RUNTIME INSPECTOR INPUT MODE OUTPUT')
app,runtime,inspector,source=map(lambda p:Path(p).resolve(),sys.argv[1:5]);mode=sys.argv[5];out=Path(sys.argv[6]).resolve();out.mkdir(parents=True,exist_ok=True)
assert mode in ['accept','cancel','focus'];checks=[];process=None;started=time.monotonic();sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();pins={str(p):sha(p)for p in [app,runtime,inspector,source,Path(__file__).resolve()]}
def report(complete=False,error=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':error,'checks':checks,'count':len(checks),'pins':pins,'drift':[p for p,h in pins.items()if sha(p)!=h],'scope':'Actual native FLTK/OpenXR app, public creator/controller actions and ordinary reopen on real Arc GL; poses/actions synthetic, no physical HMD claim.'},indent=2)+'\n')
def check(ok,label):checks.append({'passed':bool(ok),'label':label});report();print(('PASS 'if ok else'FAIL ')+label,flush=True);assert ok,label
try:
 previous=source
 for stage in ['author','reopen']:
  case=out/stage;case.mkdir();home=case/'home';home.mkdir();target=case/'final.dsculpt';graphics=case/'runtime.json';loader=case/'loader.json';loader.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
  env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_XR_','DOME_ADVANCED_'))}
  for key in ['DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','LD_PRELOAD']:env.pop(key,None)
  env.update(XR_RUNTIME_JSON=str(loader),DOME_XR_FIXTURE_REPORT=str(graphics),DOME_XR_FIXTURE_CREATOR=mode if stage=='author'else'reopen',LP_NUM_THREADS='2')
  for key,leaf in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[key]=str(home/leaf);Path(env[key]).mkdir(mode=0o700)
  appearance=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';appearance.parent.mkdir();appearance.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':1}))
  if os.environ.get('DOME_BAKED_EMISSION_APP_PRELOAD'):env['LD_PRELOAD']=os.environ['DOME_BAKED_EMISSION_APP_PRELOAD']
  argv=[str(app),'--xr','--open',str(previous),'--frames','318'if stage=='author'else'6','--save',str(target)];(case/'invocation.json').write_text(json.dumps({'command':argv,'mode':mode,'stage':stage},indent=2)+'\n')
  with(case/'app.log').open('w')as log:
   process=subprocess.Popen(argv,env=env,stdout=log,stderr=subprocess.STDOUT);code=process.wait(timeout=max(.001,240-(time.monotonic()-started)))
  process=None;check(code==0,stage+': actual public creator app exits0 after ordered native Save')
  with(case/'endpoint.log').open('w')as log:code=subprocess.run([str(inspector),'--verify-xr',str(source),str(target),mode],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=max(.001,240-(time.monotonic()-started))).returncode
  check(code==0,stage+': complete native endpoint matches exact prior authoring and independent selected-control result')
  state=json.loads(graphics.read_text());check(state.get('left_eye_nonblank',0)>0 and state.get('right_eye_nonblank',0)>0,stage+': both actual GL eyes present nonblank frames')
  check(state['frames_waited']==state['frames_begun']==state['frames_ended']and state['acquires']==state['image_waits']==state['releases'],stage+': actual loader frame and stereo ownership balances')
  check('Arc'in state.get('gl_renderer','')and'A770'in state.get('gl_renderer','')and'llvmpipe'not in state.get('gl_renderer','').lower(),stage+': actual stereo context reports required Intel Arc A770')
  if stage=='author':
   buttons=state.get('creator_buttons',[]);wanted={8,14,20,26,40,46,52,58,64,70,76,82,88,102,108,114}
   check(wanted<={row['frame']for row in buttons}and all(row['action']=='recenter'for row in buttons),'Actual returned joystick edges open creator, copy saved settings, select preset and perform local Undo/Redo')
   check(any(row['frame']==22 and row['flags']==0 for row in state.get('creator_space_flags',[]))and any(row['frame']==27 and row['flags']==0 for row in state.get('creator_view_flags',[]))and any(row['frame']==28 and not row['should_render']for row in state.get('creator_frame_states',[])),'Actual original pose/view tracking losses and normally skipped render samples remain present')
   check(state.get('reference_changes')==1,'Actual reference-space event cancels the first unaccepted draft before its fresh reopen route')
   pressures=state.get('creator_pressures',[]);wanted={92,96,118,122}if mode=='focus'else{92,96,118,122,278,282}
   check({row['frame']for row in pressures}==wanted and all(row['action']=='paint_pressure'and row['value']>.8 for row in pressures),'Actual returned pressure edges edit and confirm metric body controls and explicit red hair recolour')
   capture=state.get('creator_captures',{});required=[29,37,61,67,73,79,85,89,93,97,115,120,124]
   if mode!='focus':required+=[275,279,283,290]
   check(all(str(frame)+'-'+str(eye)in capture for frame in required for eye in [0,1]),'Both actually released eyes capture copied settings, local history, real sliders and completed creator state')
   check(all(Path(row['ppm']).is_file()and Path(row['ppm']).stat().st_size>row['width']*row['height']*3 for row in capture.values()),'Every captured stereo image contains full actual colour payload')
   if mode=='focus':check(state.get('focus_losses')==1 and state.get('focus_regains')==1,'Real focus-loss/regain events cancel the unaccepted new-avatar draft')
  else:check(not state.get('creator_buttons')and not state.get('creator_pressures'),'Fresh ordinary reopen injects no creator command or pressure')
  previous=target
 check(not[p for p,h in pins.items()if sha(p)!=h],'All product/runtime/native/source inputs remain unchanged');report(True)
except BaseException as error:report(error=repr(error));raise
finally:
 if process is not None and process.poll()is None:
  process.terminate()
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.kill();process.wait()
