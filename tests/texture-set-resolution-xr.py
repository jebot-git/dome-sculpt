#!/usr/bin/env python3
"""Ordinary XR app + real loader/GL images, synthetic actions and native Save gates.
No product observer, wrapped dispatcher, alternate frontend or renderer is used.
Run fixture producer and fresh native numeric suite before this workflow gate.
"""
import hashlib,json,os,signal,subprocess,sys,time,shutil
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if len(sys.argv)!=6:raise SystemExit('usage: texture-set-resolution-xr.py APP RUNTIME INSPECTOR FIXTURE_MANIFEST OUTPUT')
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
def actor(scene):return next(v for v in scene['objects'] if v['id']==scene['active'])
def same_sibling(before,after,set_id):
 a=actor(before);b=actor(after)
 if set_id==0:return a['texture_sets']['additional']==b['texture_sets']['additional']
 return a['material']==b['material'] and a['texture_sets']['primary']==b['texture_sets']['primary']
def stable_json(path):
 try:return json.loads(path.read_text())
 except (FileNotFoundError,json.JSONDecodeError):return {}

def drive_case(name,mode,source_key,expected_key,target,set_id):
 global process
 case=out/name;case.mkdir(exist_ok=False);home=case/'home';home.mkdir();gates=case/'gates';gates.mkdir();project=case/'input.dsculpt';project.write_bytes(fixtures[source_key].read_bytes());result=case/'final.dsculpt';runtime_report=case/'runtime.json';runtime_manifest=case/'loader.json'
 runtime_manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 env={k:v for k,v in os.environ.items() if not k.startswith('DOME_XR_FIXTURE_') and not k.startswith('DOME_XR_TEXTURE_')}
 env.update({'XR_RUNTIME_JSON':str(runtime_manifest),'DOME_XR_FIXTURE_REPORT':str(runtime_report),'DOME_XR_TEXTURE_RESOLUTION':mode,'DOME_XR_TEXTURE_TARGET':str(target),'DOME_XR_TEXTURE_GATES':str(gates),'HOME':str(home),'XDG_RUNTIME_DIR':str(home/'runtime'),'XDG_CONFIG_HOME':str(home/'config'),'XDG_STATE_HOME':str(home/'state'),'XDG_CACHE_HOME':str(home/'cache'),'LIBGL_ALWAYS_SOFTWARE':os.environ.get('LIBGL_ALWAYS_SOFTWARE','1'),'LP_NUM_THREADS':'2','MESA_SHADER_CACHE_DISABLE':'true'})
 for k in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(k,None)
 for k in ['XDG_RUNTIME_DIR','XDG_CONFIG_HOME','XDG_STATE_HOME','XDG_CACHE_HOME']:Path(env[k]).mkdir(mode=0o700)
 original=inspect(project,env);expected=inspect(fixtures[expected_key],env);stamp=project.stat().st_mtime_ns
 args=[str(app),'--xr','--open',str(project),'--save',str(result)]
 (case/'invocation.json').write_text(json.dumps({'args':args,'mode':mode,'source':source_key,'expected':expected_key,'target':target,'set':set_id},indent=2)+'\n')
 log=(case/'app.log').open('w');process=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT)
 history=mode.startswith('apply-');phases=[('accepted',expected),('undone',original),('redone',expected)] if history else [] if mode=='quality' else [('unchanged',original)]
 deadline=time.monotonic()+240
 for marker,wanted in phases:
  while time.monotonic()<deadline:
   if process.poll() is not None:raise RuntimeError((name,'app ended before durable native phase',marker,process.returncode))
   state=stable_json(runtime_report)
   wanted_step=45 if marker=='accepted' else 59 if marker=='undone' else 73 if marker=='redone' else 49 if mode in ['focus','reference'] else 45
   if state.get('field_step',0)>=wanted_step and project.stat().st_mtime_ns!=stamp:
    try:actual=inspect(project,env)
    except subprocess.CalledProcessError:actual=None
    if actual==wanted:
     check(True,name+': actual controller '+marker+' Save reaches the complete exact native endpoint')
     check(same_sibling(original,actual,set_id),name+': '+marker+' preserves every sibling field byte')
     (gates/marker).write_text('Actual native Save inspected and matched\n');stamp=project.stat().st_mtime_ns;break
   time.sleep(.05)
  else:raise TimeoutError((name,'actual native phase did not complete',marker))
 while process.poll() is None and time.monotonic()<deadline:time.sleep(.05)
 if process.poll() is None:raise TimeoutError((name,'actual XR app failed to drain/close'))
 returncode=process.returncode;process=None;log.close();check(returncode==0,name+': ordinary XR app exits cleanly after actual session and workers drain')
 final=inspect(result,env);check(final==expected,name+': final native Save/fresh reopen is exact')
 check(same_sibling(original,final,set_id),name+': final sibling data is exact')
 state=stable_json(runtime_report);check(state.get('frames_waited')==state.get('frames_begun')==state.get('frames_ended') and state.get('acquires')==state.get('image_waits')==state.get('releases') and state.get('swapchains_destroyed')==2,name+': real loader balances all stereo frame/image/session ownership')
 check(state.get('left_eye_nonblank',0)>0 and state.get('right_eye_nonblank',0)>0 and state.get('palette_images',0)>0 and '27-0' in state.get('field_captures',{}) and '27-1' in state.get('field_captures',{}),name+': both real GL eyes and the actual staged resolution panel are captured')
 check({8,14,20,26}.issubset({v['step'] for v in state.get('field_buttons',[])}) and any(v['step']==30 for v in state.get('field_pressures',[])),name+': actual Boolean/vector/pose/float APIs deliver menu navigation and slider rays')
 if history:check(any(v['action']=='undo' and v['step']==52 for v in state.get('field_buttons',[])) and any(v['action']=='redo' and v['step']==66 for v in state.get('field_buttons',[])),name+': real loader Boolean history actions produce one exact Undo and Redo')
 if mode=='focus':check(state.get('focus_losses')==1 and state.get('focus_regains')==1,name+': actual XR focus boundary cancels the unconfirmed resize')
 if mode=='reference':check(state.get('reference_changes')==1,name+': actual runtime reference-space boundary cancels the old resize authority')
 if mode=='quality':
  check(any(v['step']==36 for v in state.get('field_pressures',[])),name+': actual Apply ray is delivered to the independently measured rejecting input')
  check(manifest['quality']['measured_maximum']>.30 or manifest['quality']['measured_rms']>.08,name+': real native loose control measured loss beyond the default acceptance bound')
 renderer=os.environ.get('DOME_FIELD_XR_REQUIRE_RENDERER');check(not renderer or renderer in state.get('gl_renderer',''),name+': renderer matches the explicitly requested runtime profile')
 maps=Path(str(runtime_report)+'.maps');check(maps.is_file() and str(app) in maps.read_text() and str(runtime) in maps.read_text(),name+': actual process maps contain the pinned ordinary app and new runtime module')
 case_reports.append({'name':name,'mode':mode,'runtime_report':str(runtime_report),'runtime_sha256':sha(runtime_report),'native_sha256':sha(result),'maps_sha256':sha(maps),'renderer':state.get('gl_renderer'),'tracking':'synthetic','scope':'Ordinary app; no private product observer'});report()

try:
 check(manifest.get('version')==1 and manifest['authoring']['primary_resolution']==64 and manifest['authoring']['extra_resolution']==128 and 0<manifest['authoring']['tiny_float_coverage']<1/255 and manifest['authoring']['common_fallback_coverage']==0,'Actual fixture producer supplies independent64/128 all-plane native authoring and sub-UNORM8 FLOAT coverage')
 for case in [('extra64','apply-extra','rich-extra128','extra64',64,1),('extra256','apply-extra','rich-extra128','extra256',256,1),('primary128','apply-primary','rich-primary64','primary128',128,0),('cancel','cancel','rich-extra128','rich-extra128',64,1),('focus','focus','rich-extra128','rich-extra128',64,1),('reference','reference','rich-extra128','rich-extra128',64,1),('noop','noop','rich-extra128','rich-extra128',128,1),('quality','quality','quality-source','quality-source',64,1)]:drive_case(*case)
 check(not [p for p,v in pins.items() if sha(p)!=v],'All application/runtime/inspector/native fixture/script inputs remain unchanged')
 report(True)
except BaseException as e:report(error=repr(e));raise
finally:
 if process is not None and process.poll() is None:
  process.terminate()
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.kill();process.wait()
