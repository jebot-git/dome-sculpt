#!/usr/bin/env python3
"""Explicit Rasterize/RetainSource prepared-source second Bake via real public XR controls.
Actual both-eye bitmap labels gate source/normals choices; full native recipes,
retained sources and independent finite normal decode authorize durable Save.
"""
import hashlib,json,os,re,signal,subprocess,sys,time,shutil
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if len(sys.argv)!=7:raise SystemExit('usage: source-advanced-public-xr.py APP RUNTIME INSPECTOR CASE_MANIFEST LABEL_REFERENCE OUTPUT')
app,runtime,inspector,manifest_path,title_reference,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
title_manifest=json.loads(title_reference.read_text());font_file=Path(title_manifest['font_file']);manifest=json.loads(manifest_path.read_text());fixtures={'original':manifest_path.parent/manifest['before'],'xr-accepted':manifest_path.parent/manifest['routes']['xr']['expected'],'wrong-original':manifest_path.parent/manifest['routes']['xr']['wrong_original']}
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins={str(p):sha(p) for p in [app,runtime,inspector,manifest_path,title_reference,font_file,Path(__file__).resolve(),*fixtures.values()]}
checks=[];case_reports=[];process=None
suite_deadline=time.monotonic()+240
physical_frames_consumed=0
shader_cache_root=out/'mesa-shader-code-cache'
shader_cache_root.mkdir(exist_ok=False)
assert not any(shader_cache_root.iterdir()),'Each coordinator case starts with an empty shader code cache'
shader_cache_events=[]

def shader_cache_boundary(name,case,pid,started):
 text=(case/'app.log').read_text()
 stats=[{'hits':int(hit),'misses':int(miss)}for hit,miss in re.findall(r'disk shader cache:\s+hits\s*=\s*(\d+),\s*misses\s*=\s*(\d+)',text)]
 files={str(p.relative_to(shader_cache_root)):{'sha256':sha(p),'bytes':p.stat().st_size}for p in shader_cache_root.rglob('*')if p.is_file()}
 event={'name':name,'actual_app_pid':pid,'App_start_to_complete_seconds':time.monotonic()-started,'disk_cache_stats':stats,'disk_hits':sum(x['hits']for x in stats),'disk_misses':sum(x['misses']for x in stats),'cache_files':files,'cache_file_count':len(files)}
 if not shader_cache_events:
  assert event['disk_misses']>0 and files,'The first app must prove actual cold shader compilation/cache creation'
 shader_cache_events.append(event)
 (out/'shader-code-cache-provenance.json').write_text(json.dumps({'scope':'Initially empty cache shared only by this coordinator case; Mesa shader code only, no scene/frame/native-state cache','root':str(shader_cache_root),'initially_empty':True,'events':shader_cache_events},indent=2)+'\n')

def report(complete=False,error=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':error,'checks':checks,'count':len(checks),'cases':case_reports,'pins':pins,'drift':[p for p,v in pins.items() if sha(p)!=v],'scope':'Actual ordinary XR app/loader/GL, synthetic actions and durable native Save inspection; SourceUV is native-produced before this public second-Bake scenario; no public UV-authoring action or physical headset claim'},indent=2)+'\n')
def check(value,label):
 checks.append({'passed':bool(value),'label':label});report();print(('PASS ' if value else 'FAIL ')+label,flush=True);assert value,label

def inspect(path,env):return json.loads(subprocess.check_output([str(inspector),'--inspect',str(path)],env=env))
def stable_json(path):
 try:return json.loads(path.read_text())
 except (FileNotFoundError,json.JSONDecodeError):return {}

def drive_case(name,mode,source):
 expected_key='xr-accepted';reopen=mode=='prepared-reopen'
 global process,physical_frames_consumed
 case=out/name;case.mkdir(exist_ok=False);task_root=case/'home';task_root.mkdir();gates=case/'gates';gates.mkdir();project=case/'input.dsculpt';project.write_bytes(source.read_bytes());result=case/'final.dsculpt';runtime_report=case/'runtime.json';runtime_manifest=case/'loader.json'
 runtime_manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 env={k:v for k,v in os.environ.items() if not k.startswith('DOME_XR_PREPARED_') and not k.startswith('DOME_XR_FIXTURE_') and not k.startswith('DOME_XR_PAINT_') and not k.startswith('DOME_XR_TEXTURE_') and not k.startswith('DOME_XR_BAKED_EMISSION')}
 env.pop('MESA_SHADER_CACHE_DISABLE',None)
 env.update({'XR_RUNTIME_JSON':str(runtime_manifest),'DOME_XR_FIXTURE_REPORT':str(runtime_report),'DOME_XR_BAKED_EMISSION':mode,'DOME_XR_BAKED_EMISSION_GATES':str(gates),'DOME_XR_BAKED_EMISSION_TITLE_REFERENCE':str(title_reference),'XDG_RUNTIME_DIR':str(task_root/'runtime'),'XDG_CONFIG_HOME':str(task_root/'config'),'XDG_STATE_HOME':str(task_root/'state'),'XDG_CACHE_HOME':str(task_root/'cache'),'LP_NUM_THREADS':'2','MESA_SHADER_CACHE_DIR':str(shader_cache_root),'MESA_SHADER_CACHE_SHOW_STATS':'1'})
 for k in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(k,None)
 if os.environ.get('DOME_BAKED_EMISSION_APP_PRELOAD'):env['LD_PRELOAD']=os.environ['DOME_BAKED_EMISSION_APP_PRELOAD']
 for k in ['XDG_RUNTIME_DIR','XDG_CONFIG_HOME','XDG_STATE_HOME','XDG_CACHE_HOME']:Path(env[k]).mkdir(mode=0o700)
 for env_key,label_key in [('DOME_XR_PREPARED_ORIGINAL_KEY','source_original'),('DOME_XR_PREPARED_SOURCE_KEY','source_prepared'),('DOME_XR_PREPARED_BAKING_KEY','baking_prepared'),('DOME_XR_PREPARED_NORMALS_NO_KEY','normals_no'),('DOME_XR_PREPARED_NORMALS_YES_KEY','normals_yes')]:env[env_key]=manifest['label_keys'][label_key]
 original=inspect(project,env);expected=inspect(fixtures[expected_key],env);stamp=project.stat().st_mtime_ns
 args=[str(app),'--xr','--open',str(project),'--save',str(result)]
 (case/'invocation.json').write_text(json.dumps({'args':args,'mode':mode,'source':str(source),'expected':expected_key,'settings':manifest['routes']['xr']['settings']},indent=2)+'\n')
 if not shader_cache_events:assert not any(p.is_file()for p in shader_cache_root.rglob('*')),'The first actual app starts before any shader code cache entry exists'
 log=(case/'app.log').open('w');case_started=time.monotonic();process=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT);case_pid=process.pid
 history=not reopen;phases=[('reopened',expected)] if reopen else [('accepted',expected),('undone',original),('redone',expected)]
 deadline=suite_deadline
 for marker,wanted in phases:
  while time.monotonic()<deadline:
   if process.poll() is not None:raise RuntimeError((name,'app ended before durable native phase',marker,process.returncode))
   state=stable_json(runtime_report)
   wanted_step={'accepted':67,'undone':81,'redone':95,'reopened':67}[marker]
   if state.get('field_step',0)>=wanted_step and project.stat().st_mtime_ns!=stamp:
    try:actual=inspect(project,env)
    except subprocess.CalledProcessError:actual=None
    if actual==wanted:
     root_step,close_step={'accepted':(61,65),'undone':(77,79),'redone':(91,93),'reopened':(61,65)}[marker]
     for eye in ['0','1']:
      root=state.get('field_root_visible',{}).get(str(root_step),{}).get(eye,{});closed=state.get('field_menu_closed',{}).get(str(close_step),{}).get(eye,{})
      check(root.get('save_selected') is True and root.get('redo_enabled') is (marker=='undone') and root.get('redo_disabled') is (marker!='undone') and root.get('last',0)-root.get('first',0)>=2,name+': '+marker+' eye'+eye+' public ROOT shows selected Save and the exact history availability')
      check(closed.get('root_title') is False and closed.get('panel_present') is False and closed.get('last',0)-closed.get('first',0)>=2,name+': '+marker+' eye'+eye+' public palette disappears after the Save press')
      if reopen:check(state.get('field_ui',{}).get(str(root_step),{}).get(eye,{}).get('undo_bright_pixels')==0,name+': fresh reopen publicly disables Undo as well as Redo')
     verified=json.loads(subprocess.check_output([str(inspector),'--verify-advanced-public',str(manifest_path),'xr',marker,str(project)],env=env,timeout=max(.001,deadline-time.monotonic())))
     (case/('native-'+marker+'.json')).write_text(json.dumps(verified,indent=2)+'\n')
     check(verified.get('verified') is True,name+': actual controller '+marker+' Save passes exact source IDs/first recipe/wrong-Original exclusion and independent finite normal oracle')
     (gates/('accepted' if reopen else marker)).write_text('Actual native Save inspected and matched\n');stamp=project.stat().st_mtime_ns;break
   time.sleep(.05)
  else:raise TimeoutError((name,'actual native phase did not complete',marker))
 while process.poll() is None and time.monotonic()<deadline:time.sleep(.05)
 if process.poll() is None:raise TimeoutError((name,'actual XR app failed to drain/close'))
 returncode=process.returncode;process=None;log.close();check(returncode==0,name+': ordinary XR app exits cleanly after actual session and workers drain')
 final=inspect(result,env);check(final==expected,name+': final native Save/fresh reopen is exact')
 check(project.read_bytes()!=fixtures['original'].read_bytes(),name+': real controller Save replaced the writable project, not its immutable original fixture')
 state=stable_json(runtime_report);check(state.get('frames_waited')==state.get('frames_begun')==state.get('frames_ended') and state.get('acquires')==state.get('image_waits')==state.get('releases') and state.get('swapchains_destroyed')==2,name+': real loader balances all stereo frame/image/session ownership')
 check(state.get('left_eye_nonblank',0)>0 and state.get('right_eye_nonblank',0)>0,name+': both released GL eyes contain actual presentation')
 check(all(state.get('bake_root_font_shape',{}).get(eye,{}).get('matching') is True for eye in ['0','1']),name+': both fresh app ROOT9 images match the canonical font source')
 if not reopen:
  check(state.get('left_eye_nonblank',0)>0 and state.get('right_eye_nonblank',0)>0 and state.get('palette_images',0)>0 and all(str(step)+'-page-ready-'+str(eye) in state.get('field_captures',{}) for step in [15,21,27] for eye in [0,1]),name+': both real GL eyes capture actual Objects, Mesh tools and Mesh baking palette navigation')
  check(all(state.get('bake_root_font_shape',{}).get(eye,{}).get('matching') is True for eye in ['0','1']),name+': both ROOT9 images first match the source-font bitmap and actual eye projection')
  for eye in ['0','1']:
   for step in [15,21,27]:
    ui=state.get('bake_page_ui',{}).get(str(step),{}).get(eye,{});check(ui.get('ready') is True and ui.get('title_shape',{}).get('matching') is True and ui.get('label_bright_pixels',0)>=6 and ui.get('selected_pixels',0)*4>=ui.get('selected_samples',1)*3 and ui.get('last',0)-ui.get('first',0)>=2,name+': eye'+eye+' page'+str(ui.get('page'))+' publicly presents the actual highlighted label before its controller click')
  for step,key in [(31,'source_original'),(35,'source_prepared'),(45,'normals_no'),(49,'normals_yes'),(55,'baking_prepared')]:
   for eye in ['0','1']:
    ui=state.get('prepared_ui',{}).get(str(step),{}).get(eye,{})
    check(ui.get('ready') is True and ui.get('key')==manifest['label_keys'][key] and ui.get('shape',{}).get('matching') is True and ui.get('last',0)-ui.get('first',0)>=2 and str(step)+'-prepared-ready-'+eye in state.get('field_captures',{}),name+': eye'+eye+' actual source-font label '+key+' authorizes the next real control action')
  check({8,14,20,26,30,44,58,60}.issubset({v['step'] for v in state.get('field_buttons',[])}),'Real Boolean/vector actions navigate Source, normal options, Bake and Save')
  pressures=state.get('field_pressures',[])
  check({v['step']for v in pressures}=={33,38,47,52} and all(v['action']=='paint_pressure' and v['pressure']>.8 for v in pressures),'Real pressure edges act only on Source/normal controls and Done; no editable-world stroke')
 else:
  check(not state.get('field_pressures') and not any(v['action'] in ['undo','redo']for v in state.get('field_buttons',[])),name+': fresh reopen has no invented edit/history action')
 if history:check(any(v['action']=='undo' and v['step']==74 for v in state.get('field_buttons',[])) and any(v['action']=='redo' and v['step']==88 for v in state.get('field_buttons',[])),name+': real loader Boolean history actions produce one exact Undo and Redo')
 renderer=os.environ.get('DOME_BAKED_EMISSION_XR_REQUIRE_RENDERER','Arc');check(renderer in state.get('gl_renderer','') and 'llvmpipe' not in state.get('gl_renderer','').lower(),name+': actual stereo context uses the explicitly required hardware renderer')
 maps=Path(str(runtime_report)+'.maps');check(maps.is_file() and str(app) in maps.read_text() and str(runtime) in maps.read_text(),name+': actual process maps contain the pinned ordinary app and new runtime module')
 physical_frames_consumed+=state['frames_waited'];check(physical_frames_consumed<=6000,name+': total original public frame ceiling6000 remains unchanged')
 shader_cache_boundary(name,case,case_pid,case_started)
 case_reports.append({'name':name,'mode':mode,'runtime_report':str(runtime_report),'runtime_sha256':sha(runtime_report),'native_sha256':sha(result),'maps_sha256':sha(maps),'renderer':state.get('gl_renderer'),'tracking':'synthetic','scope':'Ordinary app; no private product observer'});report();return result

try:
 check(manifest.get('version')==1 and manifest['kind']=='advanced-prepared-normal-public' and manifest['source_uv_applied'] and manifest['uv_method'] in ['Rasterize','RetainSource'] and manifest['source_normal_version']==(1 if manifest['uv_method']=='Rasterize' else 2) and manifest['all_three_objects_retained'] and manifest['source_choice_initial']==manifest['original_source'] and manifest['source_choice_required']==manifest['prepared_source'] and manifest['wrong_original_must_fail_full_native_endpoint'],'Genuine first-Bake Prepared and Original remain eligible; real public source choice is required')
 accepted=drive_case('prepared-history','prepared-rebake',fixtures['original'])
 drive_case('fresh-reopen','prepared-reopen',accepted)
 check(not [p for p,v in pins.items() if sha(p)!=v],'Every app/runtime/producer/manifest/label/original/expected/wrong-source input stays unchanged')
 check(any(event['disk_hits']>0 for event in shader_cache_events[1:]),'Fresh reopened app proves actual shader code cache hits without scene/state reuse')
 report(True)
except BaseException as e:report(error=repr(e));raise
finally:
 if process is not None and process.poll() is None:
  process.terminate()
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.kill();process.wait()
