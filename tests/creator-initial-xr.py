#!/usr/bin/env python3
"""Separate pristine first-workspace presentation; existing authored tests stay unchanged."""
import hashlib,json,os,signal,subprocess,sys,time
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if len(sys.argv)!=6:raise SystemExit('usage: creator-initial-xr.py APP RUNTIME VERIFIER MODE OUTPUT')
app,runtime,verifier=map(lambda x:Path(x).resolve(),sys.argv[1:4]);mode=sys.argv[4];out=Path(sys.argv[5]).resolve();out.mkdir(parents=True,exist_ok=False)
assert mode in ['accept','cancel','focus'];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();pins={str(p):sha(p)for p in [app,runtime,verifier,Path(__file__).resolve()]};checks=[];process=None;started=time.monotonic()
def report(complete=False,error=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':error,'count':len(checks),'checks':checks,'pins':pins,'drift':[p for p,h in pins.items()if sha(p)!=h],'scope':'Separate real App pristine seed/creator/controller/stereo presentation and ordinary reopen; unchanged physical poses and native bytes. Synthetic runtime; no physical-HMD acceptance.'},indent=2)+'\n')
def check(yes,label):checks.append({'passed':bool(yes),'label':label});report();print(('PASS 'if yes else'FAIL ')+label,flush=True);assert yes,label
try:
 previous=None
 for stage in ['initial','reopen']:
  case=out/stage;case.mkdir();home=case/'home';home.mkdir();target=case/'final.dsculpt';graphics=case/'runtime.json';loader=case/'loader.json';loader.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}})+'\n')
  env={k:v for k,v in os.environ.items()if not k.startswith(('DOME_XR_','DOME_ADVANCED_'))}
  for key in ['DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','LD_PRELOAD']:env.pop(key,None)
  env.update(XR_RUNTIME_JSON=str(loader),DOME_XR_FIXTURE_REPORT=str(graphics),DOME_XR_FIXTURE_CREATOR=mode if stage=='initial'else'reopen',LP_NUM_THREADS='2')
  for key,leaf in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[key]=str(home/leaf);Path(env[key]).mkdir(mode=0o700)
  appearance=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';appearance.parent.mkdir();appearance.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':1})+'\n')
  if os.environ.get('DOME_BAKED_EMISSION_APP_PRELOAD'):env['LD_PRELOAD']=os.environ['DOME_BAKED_EMISSION_APP_PRELOAD']
  frames=960 if stage=='initial'else 90
  argv=[str(app),'--xr']+([]if previous is None else['--open',str(previous)])+['--frames',str(frames),'--save',str(target)]
  (case/'invocation.json').write_text(json.dumps({'command':argv,'mode':mode,'stage':stage,'scope':'Initial route intentionally has no --open: real pristine Document seed lifecycle'},indent=2)+'\n')
  with(case/'app.log').open('w')as log:
   process=subprocess.Popen(argv,env=env,stdout=log,stderr=subprocess.STDOUT);code=process.wait(timeout=max(.001,240-(time.monotonic()-started)))
  process=None;check(code==0,stage+': real application closes normally after ordered native Save')
  with(case/'endpoint.log').open('w')as log:code=subprocess.run([str(verifier),'--verify-initial',mode,str(target)],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=max(.001,240-(time.monotonic()-started))).returncode
  check(code==0,stage+': complete native seed/selected-avatar state matches independent expected result')
  state=json.loads(graphics.read_text());check(state['frames_waited']==state['frames_begun']==state['frames_ended']==frames and state['acquires']==state['image_waits']==state['releases'],stage+': real frame and stereo ownership balances')
  check(state['initial_physical_view_matches']==2*state['stereo_submissions']and state['stereo_submissions']>0,stage+': submitted physical eye poses and FoV exactly match actual runtime views')
  check('Arc'in state.get('gl_renderer','')and'A770'in state.get('gl_renderer','')and state.get('left_eye_nonblank',0)>0 and state.get('right_eye_nonblank',0)>0,stage+': both actual Arc-rendered eyes present nonblank images')
  capture=state.get('creator_captures',{});wanted=[600,900]if stage=='initial'else[2,4,37,61,89]
  check(all(f'{frame}-{eye}'in capture for frame in wanted for eye in [0,1]),stage+': late real released-eye captures retain settled presentation')
  check(all(Path(r['ppm']).stat().st_size>r['width']*r['height']*3 for r in capture.values()),stage+': every captured stereo image contains complete actual pixels')
  if stage=='initial':
   buttons=state.get('creator_buttons',[]);wanted={8,14,20,26,40,46,52,58,64,70,76,82,88,102,108,114}
   check(wanted<={r['frame']for r in buttons}and all(r['action']=='recenter'for r in buttons),'Original physical controller entry/preset/history/slider route remains present')
   check(any(r['frame']==22 and r['flags']==0 for r in state.get('creator_space_flags',[]))and any(r['frame']==27 and r['flags']==0 for r in state.get('creator_view_flags',[]))and any(r['frame']==28 and not r['should_render']for r in state.get('creator_frame_states',[])),'Original tracking-loss, invalid-view and skipped-render events remain present')
   check(state.get('reference_changes')==1,'Original actual reference-space change remains present')
   pressure={r['frame']for r in state.get('creator_pressures',[])};check(pressure==({92,96,118,122}if mode=='focus'else{92,96,118,122,278,282}),'Original released pressure-control gates remain present')
   check(state.get('focus_losses',0)==(1 if mode=='focus'else 0)and state.get('focus_regains',0)==(1 if mode=='focus'else 0),'Original focus policy remains exact before the neutral observation tail ends')
   check(all(any(r['frame']==frame and r['eye']==eye for r in state['creator_actual_views'])for frame in [600,900]for eye in [0,1]),'Actual late physical stereo view bounds are recorded independently of presentation fitting')
  else:
   check(not state.get('creator_buttons')and not state.get('creator_pressures'),'Ordinary existing-project reopen injects no creator input')
   check(sha(previous)==sha(target),'Ordinary reopen preserves every canonical native byte')
  previous=target
 check(not[p for p,h in pins.items()if sha(p)!=h],'Product/runtime/verifier/scenario bytes remain unchanged');report(True)
except BaseException as e:report(error=repr(e));raise
finally:
 if process is not None and process.poll()is None:
  process.terminate()
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.kill();process.wait()
