#!/usr/bin/env python3
"""Real OpenXR loader/GL swapchains; synthetic controller atlas-import workflow."""
import hashlib,json,os,signal,subprocess,sys
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,helper,output=map(lambda value:Path(value).resolve(),sys.argv[1:])
output.mkdir(parents=True,exist_ok=True)
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
  fixture=assets/'source.glb';project=assets/'original.dsculpt';target=directory/'result.dsculpt'
  subprocess.run([str(helper),'--ui-fixture',str(fixture)],env=env,check=True)
  subprocess.run([str(helper),'--empty-project',str(project)],env=env,check=True)
  inspect=lambda path:json.loads(subprocess.check_output([str(helper),'--inspect',str(path)],env=env,text=True))
  original=inspect(project);source=fixture.read_bytes();project_bytes=project.read_bytes()
  with (directory/'application.log').open('w') as stream:
   result=subprocess.run([str(binary),'--xr','--open',str(project),'--frames','126','--save',str(target)],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=90)
  check(result.returncode==0,mode+': actual native XR import client stops and ordered Save completes')
  runtime_report=json.loads((directory/'runtime.json').read_text());after=inspect(target)
  check('llvmpipe' in runtime_report.get('gl_renderer','').lower(),mode+': explicitly requested software GL renders actual loader swapchains')
  check(runtime_report.get('acquires',0)>0 and runtime_report.get('acquires')==runtime_report.get('image_waits')==runtime_report.get('releases'),mode+': actual stereo swapchains preserve balanced acquire/wait/release ownership')
  check(runtime_report.get('stereo_submissions',0)>50 and runtime_report.get('palette_images',0)>0,mode+': actual stereo frames contain the shared XR palette')
  check(all(runtime_report.get('import_atlas_images',{}).get('54',{}).get(eye,0)>1000 for eye in ('0','1')),mode+': both eyes draw options after the real joystick atlas toggle')
  check((directory/'runtime.json.import-atlas-54.ppm').exists(),mode+': actual XR atlas-options image is retained for visual review')
  check(fixture.read_bytes()==source and project.read_bytes()==project_bytes,mode+': original model and project bytes remain untouched')
  if mode=='accept':
   objects=after['objects'][len(original['objects']):]
   check(len(objects)==2,mode+': actual joystick Import publishes both original instances')
   check(all(obj['painted']['resolution']==64 and 'source_uv1' not in obj['painted'].get('source_maps',{}) for obj in objects),mode+': joystick paint-size controls and atlas toggle reach actual editable UV0 maps')
   retained=after['source_materials']['origins']
   check(after['version']==18 and sum('source_uv1' in after['source_materials']['paints'][owner['paint']].get('source_maps',{}) for owner in retained)==2,mode+': saved Scene18 retains both exact original UV1 domains')
   check(len(after['source_models'])==1,mode+': both instances share one retained immutable original model')
  else:
   check(after==original,mode+': leaving without joystick Import preserves the complete original Scene')
 report('passed')
 print(str(len(checks))+' actual-loader atlas import checks passed',flush=True)
except BaseException as error:
 report('failed',str(error));raise
