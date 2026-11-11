#!/usr/bin/env python3
"""GPU blur through the ordinary XR app, genuine loader and public stereo palette.
Existing resolution117 scenarios stay separate and unchanged. Requires hardware;
only synthetic controller actions and released GL images drive this test.
"""
import hashlib,json,os,signal,subprocess,sys,time,shutil
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if len(sys.argv)!=6:raise SystemExit('usage: gpu-color-blur-xr.py APP RUNTIME INSPECTOR FIXTURE_MANIFEST OUTPUT')
app,runtime,inspector,manifest_path,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
manifest=json.loads(manifest_path.read_text());fixtures={k:manifest_path.parent/v for k,v in manifest['files'].items()}
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins={str(p):sha(p) for p in [app,runtime,inspector,manifest_path,Path(__file__).resolve(),*fixtures.values()]}
checks=[];case_reports=[];process=None

def report(complete=False,error=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':error,'checks':checks,'count':len(checks),'cases':case_reports,'pins':pins,'drift':[p for p,v in pins.items() if sha(p)!=v],'scope':'Actual ordinary XR app/loader/GL, synthetic actions and durable native Save inspection; no observer or physical headset claim'},indent=2)+'\n')
def check(value,label):
 checks.append({'passed':bool(value),'label':label});report();print(('PASS ' if value else 'FAIL ')+label,flush=True);assert value,label

def inspect(path,env):return json.loads(subprocess.check_output([str(inspector),str(path)],env=env))
def stable_json(path):
 try:return json.loads(path.read_text())
 except (FileNotFoundError,json.JSONDecodeError):return {}

def drive_case(name,mode):
 source_key="original";expected_key="accepted"
 global process
 case=out/name;case.mkdir(exist_ok=False);home=case/'home';home.mkdir();gates=case/'gates';gates.mkdir();project=case/'input.dsculpt';project.write_bytes(fixtures[source_key].read_bytes());result=case/'final.dsculpt';runtime_report=case/'runtime.json';runtime_manifest=case/'loader.json'
 runtime_manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 env={k:v for k,v in os.environ.items() if not k.startswith('DOME_XR_FIXTURE_') and not k.startswith('DOME_XR_PAINT_') and not k.startswith('DOME_XR_TEXTURE_')}
 env.update({'XR_RUNTIME_JSON':str(runtime_manifest),'DOME_XR_FIXTURE_REPORT':str(runtime_report),'DOME_XR_PAINT_BLUR':mode,'DOME_XR_PAINT_GATES':str(gates),'HOME':str(home),'XDG_RUNTIME_DIR':str(home/'runtime'),'XDG_CONFIG_HOME':str(home/'config'),'XDG_STATE_HOME':str(home/'state'),'XDG_CACHE_HOME':str(home/'cache'),'LP_NUM_THREADS':'2','MESA_SHADER_CACHE_DISABLE':'true'})
 for k in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(k,None)
 if os.environ.get('DOME_BLUR_APP_PRELOAD'):env['LD_PRELOAD']=os.environ['DOME_BLUR_APP_PRELOAD']
 for k in ['XDG_RUNTIME_DIR','XDG_CONFIG_HOME','XDG_STATE_HOME','XDG_CACHE_HOME']:Path(env[k]).mkdir(mode=0o700)
 original=inspect(project,env);expected=inspect(fixtures[expected_key],env);stamp=project.stat().st_mtime_ns
 args=[str(app),'--xr','--open',str(project),'--save',str(result)]
 (case/'invocation.json').write_text(json.dumps({'args':args,'mode':mode,'source':source_key,'expected':expected_key,'strength_from_default_Flow':1},indent=2)+'\n')
 log=(case/'app.log').open('w');process=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT)
 history=True;phases=[('accepted',expected),('undone',original),('redone',expected)]
 deadline=time.monotonic()+240
 for marker,wanted in phases:
  while time.monotonic()<deadline:
   if process.poll() is not None:raise RuntimeError((name,'app ended before durable native phase',marker,process.returncode))
   state=stable_json(runtime_report)
   wanted_step={'accepted':67,'undone':81,'redone':95}[marker]
   if state.get('field_step',0)>=wanted_step and project.stat().st_mtime_ns!=stamp:
    try:actual=inspect(project,env)
    except subprocess.CalledProcessError:actual=None
    if actual==wanted:
     if history:
      root_step,close_step={'accepted':(61,65),'undone':(77,79),'redone':(91,93)}[marker]
      for eye in ['0','1']:
       root=state.get('field_root_visible',{}).get(str(root_step),{}).get(eye,{});closed=state.get('field_menu_closed',{}).get(str(close_step),{}).get(eye,{})
       check(root.get('save_selected') is True and root.get('redo_enabled') is (marker=='undone') and root.get('redo_disabled') is (marker!='undone') and root.get('last',0)-root.get('first',0)>=2,name+': '+marker+' eye'+eye+' public ROOT shows selected Save and the exact history availability')
       check(closed.get('root_title') is False and closed.get('panel_present') is False and closed.get('last',0)-closed.get('first',0)>=2,name+': '+marker+' eye'+eye+' public palette disappears after the Save press')
     check(True,name+': actual controller '+marker+' Save reaches the complete exact native endpoint')
     (gates/marker).write_text('Actual native Save inspected and matched\n');stamp=project.stat().st_mtime_ns;break
   time.sleep(.05)
  else:raise TimeoutError((name,'actual native phase did not complete',marker))
 while process.poll() is None and time.monotonic()<deadline:time.sleep(.05)
 if process.poll() is None:raise TimeoutError((name,'actual XR app failed to drain/close'))
 returncode=process.returncode;process=None;log.close();check(returncode==0,name+': ordinary XR app exits cleanly after actual session and workers drain')
 final=inspect(result,env);check(final==expected,name+': final native Save/fresh reopen is exact')
 check(project.read_bytes()!=fixtures['original'].read_bytes(),name+': real controller Save replaced the writable project, not its immutable original fixture')
 state=stable_json(runtime_report);check(state.get('frames_waited')==state.get('frames_begun')==state.get('frames_ended') and state.get('acquires')==state.get('image_waits')==state.get('releases') and state.get('swapchains_destroyed')==2,name+': real loader balances all stereo frame/image/session ownership')
 check(state.get('left_eye_nonblank',0)>0 and state.get('right_eye_nonblank',0)>0 and state.get('palette_images',0)>0 and all(str(step)+'-'+str(eye) in state.get('field_captures',{}) for step in [21,31,43,49,55] for eye in [0,1]),name+': both real GL eyes capture actual brush Filters and Blur palette navigation')
 for eye in ['0','1']:
  for step in [21,31,43,49,55]:
   ui=state.get('blur_page_ui',{}).get(str(step),{}).get(eye,{});check(ui.get('ready') is True and ui.get('checkbox_state_valid') is True and ui.get('label_bright_pixels',0)>=6 and ui.get('selected_pixels',0)*4>=ui.get('selected_samples',1)*3 and ui.get('last',0)-ui.get('first',0)>=2,name+': eye'+eye+' page'+str(ui.get('page'))+' publicly presents the actual highlighted label before its controller click')
  ui=state.get('blur_page_ui',{});check(ui.get('21',{}).get(eye,{}).get('erase_checkbox_fill_pixels')==0 and ui.get('43',{}).get(eye,{}).get('erase_checkbox_fill_pixels',0)>=2 and ui.get('49',{}).get(eye,{}).get('erase_checkbox_mark_pixels')==0,name+': eye'+eye+' real Painting Erase becomes checked while Filters navigation remains unmarked')
 check({8,14,20,26,30,36,42,48,54,60}.issubset({v['step'] for v in state.get('field_buttons',[])}) and not state.get('field_pressures',[]),name+': real loader Boolean/vector actions navigate the complete route and blur without inventing a paint stroke')
 if history:check(any(v['action']=='undo' and v['step']==74 for v in state.get('field_buttons',[])) and any(v['action']=='redo' and v['step']==88 for v in state.get('field_buttons',[])),name+': real loader Boolean history actions produce one exact Undo and Redo')
 renderer=os.environ.get('DOME_BLUR_XR_REQUIRE_RENDERER','Arc');check(renderer in state.get('gl_renderer','') and 'llvmpipe' not in state.get('gl_renderer','').lower(),name+': actual stereo context uses the explicitly required hardware renderer')
 maps=Path(str(runtime_report)+'.maps');check(maps.is_file() and str(app) in maps.read_text() and str(runtime) in maps.read_text(),name+': actual process maps contain the pinned ordinary app and new runtime module')
 case_reports.append({'name':name,'mode':mode,'runtime_report':str(runtime_report),'runtime_sha256':sha(runtime_report),'native_sha256':sha(result),'maps_sha256':sha(maps),'renderer':state.get('gl_renderer'),'tracking':'synthetic','scope':'Ordinary app; no private product observer'});report()

try:
 check(manifest.get('version')==1 and manifest['resolution']==64 and manifest['flow']==1 and manifest['independent_pixel_oracle'],'Actual producer supplies genuine nonuniform strictUV native color/alpha/PBR data and independent expected pixels')
 drive_case('apply-history','apply')
 check(not [p for p,v in pins.items() if sha(p)!=v],'All application/runtime/inspector/native fixture/script inputs remain unchanged')
 report(True)
except BaseException as e:report(error=repr(e));raise
finally:
 if process is not None and process.poll() is None:
  process.terminate()
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.kill();process.wait()
