#!/usr/bin/env python3
"""Real OpenXR loader/stereo GL + synthetic controller colour UV23 journey."""
import json,os,signal,subprocess,sys,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,helper,output=map(lambda p:Path(p).resolve(),sys.argv[1:]);output.mkdir(parents=True,exist_ok=True)
checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True)
 (output/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks)},indent=2))
 assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-corner-uv23-xr-') as directory:
 root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','HOME':directory,'XDG_RUNTIME_DIR':directory,'XDG_CONFIG_HOME':directory+'/config','XDG_STATE_HOME':directory+'/state'}
 fixture=root/'source.dsculpt';subprocess.run([str(helper),'--ui-fixture',str(fixture)],check=True,env=env)
 def inspect(path):
  value=json.loads(subprocess.check_output([str(helper),'--inspect',str(path)],env=env))
  def stable(item):
   if isinstance(item,dict):
    if 'voxel_size' in item and 'samples' in item:item['samples'].sort()
    for x in item.values():stable(x)
   elif isinstance(item,list):
    for x in item:stable(x)
  stable(value);return value
 original=inspect(fixture);target_index=next(i for i,o in enumerate(original['objects']) if o['id']==original['active'])
 for mode in ('accept','cancel','focus'):
  target=output/(mode+'.dsculpt');report=output/(mode+'.json')
  with (output/(mode+'.log')).open('w') as log:
   result=subprocess.run([str(binary),'--xr','--open',str(fixture),'--frames','36','--save',str(target)],env={**env,'DOME_XR_FIXTURE_REPORT':str(report),'DOME_XR_FIXTURE_CORNER_UV':mode},stdout=log,stderr=subprocess.STDOUT,timeout=120)
  check(result.returncode==0,'Actual loader colourUV '+mode+' finishes and saves normally')
  after=inspect(target);r=json.loads(report.read_text())
  check(r.get('acquires')==r.get('image_waits')==r.get('releases') and r.get('left_eye_nonblank',0)>0 and r.get('right_eye_nonblank',0)>0 and r.get('palette_images',0)>0 and r.get('tracking_losses',0)==0,'ColourUV '+mode+' draws both real GL eyes and the spatial UI with balanced image ownership')
  if mode=='accept':
   p=after['objects'][target_index]['painted'];bindings=after['source_materials']
   check(after['version']==30 and after['objects'][target_index]['bake_recipe']==original['objects'][target_index]['bake_recipe'] and bindings['version']==8 and any('uv_reprojection' in p for p in bindings['color_preparations']) and bool(p['baked']['normal']['bytes']) and bool(p['baked']['pbr_color']['bytes']),'Actual joystick UV Confirm publishes owning current-field UV receipt and complete baked normal/PBR planes')
   check(after['polygon_sources']['color_fields']==original['polygon_sources']['color_fields'] and after['polygon_sources']['color_bases']==original['polygon_sources']['color_bases'] and all(after['isolated_objects'][i]==original['isolated_objects'][i] for i in range(len(after['objects'])) if i!=target_index),'Loader current-colour UV keeps independent corner fields/bases and read-only source exact')
  else:check(after==original,'Actual colourUV '+mode+' retains the complete original even after a subsequent Confirm that would expose an uncancelled UV draft')
  if mode=='focus':check(r.get('focus_losses')==1 and r.get('focus_regains')==1,'Real OpenXR focus events cancel the unaccepted colour UV workspace')
print(str(len(checks))+' actual-loader current-colour UV checks passed',flush=True)
