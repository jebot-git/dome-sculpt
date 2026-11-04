#!/usr/bin/env python3
"""Real OpenXR loader/GL swapchains; synthetic controller atlas-import workflow."""
import hashlib,json,os,signal,subprocess,sys
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,helper,output=map(lambda value:Path(value).resolve(),sys.argv[1:])
output.mkdir(parents=True,exist_ok=True)
EXTRA=os.environ.get("DOME_IMPORT_EXTRA_UV")=="1"
RAW=os.environ.get("DOME_IMPORT_EXTRA_RAW")=="1"
checks=[]
def report(status,error=None):
 (output/'report.json').write_text(json.dumps({'status':status,'complete':status=='passed','checks':checks,'failed':sum(not item['passed'] for item in checks),'error':error,'synthetic_tracking':True},indent=2)+'\n')
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True);report('running');assert ok,label
report('running')
try:
 for mode in ('accept','cancel'):
  directory=output/mode;directory.mkdir(exist_ok=True)
  assets=directory/'assets';assets.mkdir(exist_ok=True)
  home=directory/'home';home.mkdir(exist_ok=True)
  runtime_directory=directory/'runtime';runtime_directory.mkdir(mode=0o700,exist_ok=True)
  manifest=runtime_directory/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
  env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','HOME':str(home),'XDG_RUNTIME_DIR':str(runtime_directory),'XDG_CONFIG_HOME':str(directory/'config'),'XDG_STATE_HOME':str(directory/'state'),'DOME_XR_FIXTURE_IMPORT_ATLAS':mode,'DOME_XR_FIXTURE_REPORT':str(directory/'runtime.json')}
  if RAW:env['DOME_XR_FIXTURE_IMPORT_EXTRA_RAW']='1'
  fixture=assets/'source.glb';project=assets/'original.dsculpt';target=directory/'result.dsculpt'
  subprocess.run([str(helper),'--ui-fixture',str(fixture)],env=env,check=True)
  subprocess.run([str(helper),'--empty-project',str(project)],env=env,check=True)
  inspect=lambda path:json.loads(subprocess.check_output([str(helper),'--inspect-import-contract',str(path)],env=env,text=True))
  original_inspection=inspect(project);original=original_inspection['scene'];source=fixture.read_bytes();project_bytes=project.read_bytes()
  with (directory/'application.log').open('w') as stream:
   result=subprocess.run([str(binary),'--xr','--open',str(project),'--frames','126','--save',str(target)],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=90)
  check(result.returncode==0,mode+': actual native XR import client stops and ordered Save completes')
  runtime_report=json.loads((directory/'runtime.json').read_text());after_inspection=inspect(target);after=after_inspection['scene']
  check('llvmpipe' in runtime_report.get('gl_renderer','').lower(),mode+': explicitly requested software GL renders actual loader swapchains')
  check(runtime_report.get('acquires',0)>0 and runtime_report.get('acquires')==runtime_report.get('image_waits')==runtime_report.get('releases'),mode+': actual stereo swapchains preserve balanced acquire/wait/release ownership')
  check(runtime_report.get('stereo_submissions',0)>50 and runtime_report.get('palette_images',0)>0,mode+': actual stereo frames contain the shared XR palette')
  check(all(runtime_report.get('import_atlas_images',{}).get('54',{}).get(eye,0)>1000 for eye in ('0','1')),mode+(': both eyes draw the unchanged original-coordinate options' if RAW else ': both eyes draw options after the real joystick atlas toggle'))
  check((directory/'runtime.json.import-atlas-54.ppm').exists(),mode+': actual XR atlas-options image is retained for visual review')
  check(fixture.read_bytes()==source and project.read_bytes()==project_bytes,mode+': original model and project bytes remain untouched')
  if mode=='accept':
   objects=after['objects'][len(original['objects']):]
   native=[after_inspection['native_objects'][str(obj['id'])] for obj in objects]
   check(len(objects)==2,mode+': actual joystick Import publishes both original instances')
   check(all(facts['resolution']==64 for facts in native),mode+': actual native paint-size joystick controls reach both imported instances')
   if EXTRA:
    def uv2(maps):
     return maps and maps['roles']['normal']['present'] and maps['roles']['normal']['texcoord']==2 and 2 in maps['extra_uv_sets']
    if RAW:
     check(all(not facts['prepared'] and uv2(facts['maps']) for facts in native),mode+': default joystick import retains the actual raw normal UV2 owner')
     check(after['version']==26,mode+': native raw extra-UV source retains the required Scene26 capability')
    else:
     check(all(facts['strict'] and facts['prepared'] and facts['valid_source_owner'] and facts['original_geometry_exact'] and facts['normal_pbr_frames'] for facts in native),mode+': real joystick atlas preparation owns a separate Strict writable grid and authenticated source geometry/frames')
     check(all(uv2(facts['maps']) and uv2(facts['before_maps']) and uv2(facts['origin_maps']) and facts['original_normal_coordinates_exact'] and facts['original_normal_image_strength_exact'] and not facts['sampling_reparameterized'] for facts in native),mode+': current and original actual UV2 samplers/images/strength remain exact across the independent writable atlas')
     check(after['version']==28,mode+': saved Scene28 admits actual Source4 ownership without fabricating a Mirror feature')
   else:
    def uv1(maps):
     return maps and maps['uv1'] and maps['roles']['normal']['present'] and maps['roles']['normal']['texcoord']==1
    check(all(facts['strict'] and facts['prepared'] and facts['valid_source_owner'] and facts['original_geometry_exact'] and facts['normal_pbr_frames'] for facts in native),mode+': joystick atlas preparation owns a separate Strict writable UV0 grid and authentic source geometry/frames')
    check(after['version']==28 and all(uv1(facts['maps']) and uv1(facts['before_maps']) and uv1(facts['origin_maps']) and facts['original_normal_coordinates_exact'] and facts['original_normal_image_strength_exact'] and not facts['sampling_reparameterized'] for facts in native),mode+': saved Scene28 preserves both actual original UV1 sampling owners, independent coordinates and normal intent')
   check(len(after['source_models'])==1,mode+': both instances share one retained immutable original model')
  else:
   check(after==original and after_inspection==original_inspection,mode+': leaving without joystick Import preserves the complete original Scene and actual native ownership/coordinate facts')
 report('passed')
 print(str(len(checks))+' actual-loader atlas import checks passed',flush=True)
except BaseException as error:
 report('failed',str(error));raise
