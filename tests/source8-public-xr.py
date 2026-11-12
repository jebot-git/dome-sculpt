#!/usr/bin/env python3
"""FLOAT Bake through the ordinary XR app, loader and public stereo palette.
Existing blur39/resolution117 scenarios stay separate and unchanged. Only actual
released GL images and full native Save oracles authorize the synthetic actions.
"""
import hashlib,json,os,signal,subprocess,sys,time,shutil
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if len(sys.argv)!=7:raise SystemExit('usage: source8-public-xr.py APP RUNTIME INSPECTOR FIXTURE_MANIFEST TITLE_REFERENCE OUTPUT')
app,runtime,inspector,manifest_path,title_reference,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
title_manifest=json.loads(title_reference.read_text());font_file=Path(title_manifest['font_file']);manifest=json.loads(manifest_path.read_text());fixtures={k:manifest_path.parent/v for k,v in manifest['files'].items()}
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins={str(p):sha(p) for p in [app,runtime,inspector,manifest_path,title_reference,font_file,Path(__file__).resolve(),*fixtures.values()]}
checks=[];case_reports=[];process=None
suite_deadline=time.monotonic()+240
physical_frames_consumed=0

def report(complete=False,error=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':error,'checks':checks,'count':len(checks),'cases':case_reports,'pins':pins,'drift':[p for p,v in pins.items() if sha(p)!=v],'scope':'Actual ordinary XR app/loader/GL, synthetic actions and durable native Save inspection; no observer or physical headset claim'},indent=2)+'\n')
def check(value,label):
 checks.append({'passed':bool(value),'label':label});report();print(('PASS ' if value else 'FAIL ')+label,flush=True);assert value,label

def inspect(path,env):return json.loads(subprocess.check_output([str(inspector),'--inspect',str(path)],env=env))
def stable_json(path):
 try:return json.loads(path.read_text())
 except (FileNotFoundError,json.JSONDecodeError):return {}

def drive_case(name,mode):
 source_key="original";expected_key="xr-accepted"
 global process,physical_frames_consumed
 case=out/name;case.mkdir(exist_ok=False);task_root=case/'home';task_root.mkdir();gates=case/'gates';gates.mkdir();project=case/'input.dsculpt';project.write_bytes(fixtures[source_key].read_bytes());result=case/'final.dsculpt';runtime_report=case/'runtime.json';runtime_manifest=case/'loader.json'
 runtime_manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 env={k:v for k,v in os.environ.items() if not k.startswith('DOME_XR_FIXTURE_') and not k.startswith('DOME_XR_PAINT_') and not k.startswith('DOME_XR_TEXTURE_') and not k.startswith('DOME_XR_BAKED_EMISSION')}
 env.update({'XR_RUNTIME_JSON':str(runtime_manifest),'DOME_XR_FIXTURE_REPORT':str(runtime_report),'DOME_XR_BAKED_EMISSION':mode,'DOME_XR_BAKED_EMISSION_GATES':str(gates),'DOME_XR_BAKED_EMISSION_TITLE_REFERENCE':str(title_reference),'XDG_RUNTIME_DIR':str(task_root/'runtime'),'XDG_CONFIG_HOME':str(task_root/'config'),'XDG_STATE_HOME':str(task_root/'state'),'XDG_CACHE_HOME':str(task_root/'cache'),'LP_NUM_THREADS':'2','MESA_SHADER_CACHE_DISABLE':'true'})
 for k in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(k,None)
 if os.environ.get('DOME_BAKED_EMISSION_APP_PRELOAD'):env['LD_PRELOAD']=os.environ['DOME_BAKED_EMISSION_APP_PRELOAD']
 for k in ['XDG_RUNTIME_DIR','XDG_CONFIG_HOME','XDG_STATE_HOME','XDG_CACHE_HOME']:Path(env[k]).mkdir(mode=0o700)
 original=inspect(project,env);expected=inspect(fixtures[expected_key],env);stamp=project.stat().st_mtime_ns
 args=[str(app),'--xr','--open',str(project),'--save',str(result)]
 (case/'invocation.json').write_text(json.dumps({'args':args,'mode':mode,'source':source_key,'expected':expected_key,'settings':manifest['default_settings']},indent=2)+'\n')
 log=(case/'app.log').open('w');process=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT)
 history=True;phases=[('accepted',expected),('undone',original),('redone',expected)]
 deadline=suite_deadline
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
 check(state.get('left_eye_nonblank',0)>0 and state.get('right_eye_nonblank',0)>0 and state.get('palette_images',0)>0 and all(str(step)+'-page-ready-'+str(eye) in state.get('field_captures',{}) for step in [15,21,27] for eye in [0,1]),name+': both real GL eyes capture actual Objects, Mesh tools and Mesh baking palette navigation')
 check(all(state.get('bake_root_font_shape',{}).get(eye,{}).get('matching') is True for eye in ['0','1']),name+': both ROOT9 images first match the source-font bitmap and actual eye projection')
 for eye in ['0','1']:
  for step in [15,21,27]:
   ui=state.get('bake_page_ui',{}).get(str(step),{}).get(eye,{});check(ui.get('ready') is True and ui.get('title_shape',{}).get('matching') is True and ui.get('label_bright_pixels',0)>=6 and ui.get('selected_pixels',0)*4>=ui.get('selected_samples',1)*3 and ui.get('last',0)-ui.get('first',0)>=2,name+': eye'+eye+' page'+str(ui.get('page'))+' publicly presents the actual highlighted label before its controller click')
 check({8,14,20,26,30,60}.issubset({v['step'] for v in state.get('field_buttons',[])}) and not state.get('field_pressures',[]),name+': real loader Boolean/vector actions navigate the complete actual Bake route without inventing a paint stroke')
 if history:check(any(v['action']=='undo' and v['step']==74 for v in state.get('field_buttons',[])) and any(v['action']=='redo' and v['step']==88 for v in state.get('field_buttons',[])),name+': real loader Boolean history actions produce one exact Undo and Redo')
 renderer=os.environ.get('DOME_BAKED_EMISSION_XR_REQUIRE_RENDERER','Arc');check(renderer in state.get('gl_renderer','') and 'llvmpipe' not in state.get('gl_renderer','').lower(),name+': actual stereo context uses the explicitly required hardware renderer')
 maps=Path(str(runtime_report)+'.maps');check(maps.is_file() and str(app) in maps.read_text() and str(runtime) in maps.read_text(),name+': actual process maps contain the pinned ordinary app and new runtime module')
 physical_frames_consumed+=state['frames_waited'];check(physical_frames_consumed<=6000,name+': total original public frame ceiling6000 remains unchanged')
 case_reports.append({'name':name,'mode':mode,'runtime_report':str(runtime_report),'runtime_sha256':sha(runtime_report),'native_sha256':sha(result),'maps_sha256':sha(maps),'renderer':state.get('gl_renderer'),'tracking':'synthetic','scope':'Ordinary app; no private product observer'});report()

try:
 check(manifest.get('version')==2 and manifest['kind']=='source8-uv1-float-public' and manifest['resolution']==64 and manifest['original_texcoord']==1 and manifest['current_texcoord']==0 and manifest['scene_version']==42 and manifest['paint_version']==8 and manifest['hidden_locked_source'] and manifest['hidden_target_tiles'] and manifest['hidden_sibling_physical_tiles'] and manifest['independent_native_oracle'] and manifest['independent_texel_oracle'],'Actual independent producer supplies genuine UV1/Source8/FLOAT and complete hidden/normal/native endpoints')
 drive_case('apply-history','apply')
 # Original33 public Bake predicates execute above, unchanged apart from the
 # richer manifest admission. Add independent actual-source display cases.
 view_files={k:manifest_path.parent/v for k,v in manifest['view_files'].items()}
 for file in view_files.values():pins[str(file)]=sha(file)
 descriptors={};controls={};wrong_controls={};normal_effects={0:[],1:[]}
 # Source8 public runtime keeps the original320-square recommendation. The
 # control program independently casts each declared public pixel ray per eye.
 for eye in [0,1]:
  for probe in range(4):
   control=out/('independent-eye'+str(eye)+'-probe'+str(probe)+'.dsculpt')
   descriptor=json.loads(subprocess.check_output([str(inspector),'--visible-control',str(manifest_path),'xr','320','320',str(eye),str(probe),str(control)]))
   assert descriptor['width']==descriptor['height']==320 and descriptor['eye']==eye and descriptor['probe']==probe
   descriptors[eye,probe]=descriptor;controls[eye,probe]=control;pins[str(control)]=sha(control)
   wrong=out/('without-baked-normal-eye'+str(eye)+'-probe'+str(probe)+'.dsculpt')
   wrong_descriptor=json.loads(subprocess.check_output([str(inspector),'--visible-control',str(manifest_path),'xr','320','320',str(eye),str(probe),str(wrong),'--without-baked-detail'],timeout=max(.001,suite_deadline-time.monotonic())))
   check(wrong_descriptor['control_kind']=='without-baked-detail','Independent XR wrong-normal control remains explicitly distinct from accepted source data')
   wrong_controls[eye,probe]=wrong;pins[str(wrong)]=sha(wrong)
 def view_image(path):
  from PIL import Image
  with Image.open(path)as image:return image.convert('RGB').copy()
 def image_pixel(image,descriptor):
  x,y=descriptor['pixel_bottom_to_top'];return image.getpixel((x,image.height-1-y))
 def drive_source_view(name,scene,accept):
  global process,physical_frames_consumed
  if time.monotonic()>=suite_deadline:raise TimeoutError('Original240s public workflow phase deadline reached before next source view')
  case=out/name;case.mkdir(exist_ok=False);task_root=case/'home';task_root.mkdir();gates=case/'gates';gates.mkdir();project=case/'input.dsculpt';project.write_bytes(scene.read_bytes());result=case/'final.dsculpt';runtime_report=case/'runtime.json';loader=case/'loader.json';loader.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
  env={k:v for k,v in os.environ.items()if not k.startswith('DOME_XR_FIXTURE_')and not k.startswith('DOME_XR_BAKED_EMISSION')}
  env.update(XR_RUNTIME_JSON=str(loader),DOME_XR_FIXTURE_REPORT=str(runtime_report),DOME_XR_BAKED_EMISSION='source8-view',DOME_XR_BAKED_EMISSION_GATES=str(gates),DOME_XR_BAKED_EMISSION_TITLE_REFERENCE=str(title_reference),LP_NUM_THREADS='2',MESA_SHADER_CACHE_DISABLE='true')
  for k in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(k,None)
  if os.environ.get('DOME_BAKED_EMISSION_APP_PRELOAD'):env['LD_PRELOAD']=os.environ['DOME_BAKED_EMISSION_APP_PRELOAD']
  for key,suffix in [('XDG_RUNTIME_DIR','runtime'),('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_CACHE_HOME','cache')]:env[key]=str(task_root/suffix);Path(env[key]).mkdir(mode=0o700)
  expected=inspect(scene,env);log=(case/'app.log').open('w');process=subprocess.Popen([str(app),'--xr','--open',str(project),'--save',str(result)],env=env,stdout=log,stderr=subprocess.STDOUT);deadline=suite_deadline;captured=None
  while process.poll()is None and time.monotonic()<deadline:
   state=stable_json(runtime_report)
   if all(state.get('source8_view',{}).get(str(eye),{}).get('frames',0)>=3 for eye in [0,1]):
    try:images={eye:view_image(state['source8_view'][str(eye)]['image'])for eye in [0,1]}
    except (OSError,KeyError):images=None
    if images and all(image.size==(320,320)for image in images.values())and accept(images):
     captured=images
     for eye,image in images.items():image.save(case/('qualified-eye-'+str(eye)+'.png'))
     (gates/'visible-inspected').write_text('Both actual eye images inspected against independent declared controls\n');break
   time.sleep(.05)
  if captured is None:raise TimeoutError((name,'actual both-eye independent source display gate failed'))
  while process.poll()is None and time.monotonic()<deadline:time.sleep(.05)
  if process.poll()is None:raise TimeoutError((name,'actual view session did not drain before original240s deadline'))
  code=process.returncode;process=None;log.close();check(code==0,name+': actual source/control app closes after public real-loader lifecycle')
  check(inspect(result,env)==expected,name+': fresh native loader preserves every actual source/control field/normal/hidden owner')
  state=stable_json(runtime_report);check(state.get('frames_waited')==state.get('frames_begun')==state.get('frames_ended')and state.get('acquires')==state.get('image_waits')==state.get('releases')and state.get('swapchains_destroyed')==2,name+': actual both-eye frame/image/session ownership balances')
  physical_frames_consumed+=state['frames_waited'];check(physical_frames_consumed<=6000,name+': aggregate real-loader workflow stays inside original6000frame ceiling')
  maps=Path(str(runtime_report)+'.maps');check(maps.is_file()and str(app)in maps.read_text()and str(runtime)in maps.read_text(),name+': live source/control process maps include exact actual app and new runtime')
  check(not state.get('field_buttons')and not state.get('field_pressures')and not state.get('field_gate_exhausted'),name+': independent visible-source view invents no sculpt/paint/history action')
  check(os.environ.get('DOME_BAKED_EMISSION_XR_REQUIRE_RENDERER','Arc')in state.get('gl_renderer',''),name+': actual source/control views retain required hardware renderer')
  case_reports.append({'name':name,'mode':'source8-view','source_native_sha256':sha(scene),'final_native_sha256':sha(result),'runtime_report':str(runtime_report),'runtime_sha256':sha(runtime_report),'maps_sha256':sha(maps),'actual_eye_images':{str(eye):{'path':str(case/('qualified-eye-'+str(eye)+'.png')),'sha256':sha(case/('qualified-eye-'+str(eye)+'.png'))}for eye in [0,1]},'scope':'Actual both-eye released source/control imagery and fresh native load; independent ordinary-material oracle, no private product callback or physicalHMD'});report()
  return captured
 empty=drive_source_view('source8-hidden-counterfactual',view_files['empty'],lambda images:True)
 def actual_geometry(images):
  return all(max(abs(a-b)for a,b in zip(image_pixel(images[eye],descriptors[eye,probe]),image_pixel(empty[eye],descriptors[eye,probe])))>=12 for eye in [0,1]for probe in range(4))
 source=drive_source_view('source8-visible-original',view_files['original'],actual_geometry)
 for eye in [0,1]:
  for probe in range(4):
   descriptor=descriptors[eye,probe]
   def independent_match(images):
    got=image_pixel(source[eye],descriptor);wanted=image_pixel(images[eye],descriptor);hidden=image_pixel(empty[eye],descriptor)
    return max(abs(a-b)for a,b in zip(got,wanted))<=3 and max(abs(a-b)for a,b in zip(wanted,hidden))>=12
   control=drive_source_view('source8-control-eye'+str(eye)+'-probe'+str(probe),controls[eye,probe],independent_match)
   got=image_pixel(source[eye],descriptor);wanted=image_pixel(control[eye],descriptor);hidden=image_pixel(empty[eye],descriptor)
   def wrong_geometry(images):return max(abs(a-b)for a,b in zip(image_pixel(images[eye],descriptor),hidden))>=12
   wrong=drive_source_view('source8-without-baked-detail-eye'+str(eye)+'-probe'+str(probe),wrong_controls[eye,probe],wrong_geometry)
   wrong_rgb=image_pixel(wrong[eye],descriptor);normal_effects[eye].append({'probe':probe,'independent_effect':max(abs(a-b)for a,b in zip(wanted,wrong_rgb)),'actual_effect':max(abs(a-b)for a,b in zip(got,wrong_rgb))})
   check(not all(v==255 for v in wanted)and not all(v==255 for v in wrong_rgb),'Actual eye'+str(eye)+' Source8 ordinary point controls avoid vacuous clipped-white equality')
   check(descriptor['maximum_rgb_difference']==3 and max(abs(a-b)for a,b in zip(got,wanted))<=3,'Actual eye'+str(eye)+' Source8 probe'+str(probe)+' matches independent original/current normal/FLOAT ordinary material within fixed renderer bound3')
   check(descriptor['minimum_geometry_counterfactual_difference']==12 and max(abs(a-b)for a,b in zip(wanted,hidden))>=12,'Actual eye'+str(eye)+' Source8 probe'+str(probe)+' cannot pass from shared empty/background imagery')
   (out/('source8-eye'+str(eye)+'-probe'+str(probe)+'.json')).write_text(json.dumps({**descriptor,'actual_RGB':got,'independent_RGB':wanted,'hidden_RGB':hidden,'without_baked_detail_RGB':wrong_rgb},indent=2)+'\n')

 for eye in [0,1]:check(manifest['visible_minimum_case_normal_effect']==7 and any(v['independent_effect']>=7 and v['actual_effect']>3 for v in normal_effects[eye]),'Actual eye'+str(eye)+' Source8 retains disjoint correct/wrong-normal acceptance intervals and actual wrong-normal rejection')
 (out/'source8-normal-sensitivity.json').write_text(json.dumps(normal_effects,indent=2)+'\n')
 check(not [p for p,v in pins.items() if sha(p)!=v],'All application/runtime/inspector/native fixture/script inputs remain unchanged')
 report(True)
except BaseException as e:report(error=repr(e));raise
finally:
 if process is not None and process.poll() is None:
  process.terminate()
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.kill();process.wait()
