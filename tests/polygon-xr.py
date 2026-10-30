#!/usr/bin/env python3
"""Real OpenXR loader/stereo GL/joystick polygon workflow; synthetic devices."""
import json,os,signal,subprocess,sys,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,output=map(lambda p:Path(p).resolve(),sys.argv[1:]);output.mkdir(parents=True,exist_ok=True)
checks=[]
def report(complete=False,error=None):
 (output/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks),'complete':complete,'status':'failed' if error else 'passed' if complete else 'running','error':error,'synthetic_tracking':True},indent=2)+'\n')
def error_hook(kind,value,trace):report(error=str(value));sys.__excepthook__(kind,value,trace)
sys.excepthook=error_hook
def check(value,message):
 checks.append({'passed':bool(value),'check':message});print(('PASS ' if value else 'FAIL ')+message,flush=True);report();assert value,message
with tempfile.TemporaryDirectory(prefix='dome-polygon-xr-') as directory:
 root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','HOME':directory,'XDG_RUNTIME_DIR':directory,'XDG_CONFIG_HOME':directory+'/config','XDG_STATE_HOME':directory+'/state'}
 helper=binary.parent/'dome-sculpt-polygon-document-test';fixture=root/'source.dsculpt';subprocess.run([str(helper),'--xr-fixture',str(fixture)],env=env,check=True)
 def inspect(path):
  scene=json.loads(subprocess.check_output([str(helper),'--inspect',str(path)],env=env))
  volumes=[o['volume'] for o in scene['objects']]+scene.get('polygon_sources',{}).get('volumes',[])
  for volume in volumes:
   volume['samples'].sort()
   for layer in volume.get('sculpt_layers',[]):layer['delta'].sort()
   if 'sculpt_mask' in volume:volume['sculpt_mask']['values'].sort()
  return scene
 original=inspect(fixture);accepted=None
 modes=os.environ.get('DOME_POLYGON_XR_MODES','accept,cancel,noop,unconfirmed,focus,restore,restore-cancel,restore-default').split(',')
 for mode in modes:
  source=output/'accept.dsculpt' if mode.startswith('restore') else fixture
  target=output/(mode+'.dsculpt');stats=output/(mode+'.json')
  with (output/(mode+'.log')).open('w') as log:
   result=subprocess.run([str(binary),'--xr','--open',str(source),'--save',str(target)],env={**env,'DOME_XR_FIXTURE_POLYGON':mode,'DOME_XR_FIXTURE_REPORT':str(stats)},stdout=log,stderr=subprocess.STDOUT,timeout=60)
  check(result.returncode==0 and target.is_file(),'Real-loader polygon '+mode+' closes normally and writes its ordered native endpoint')
  after=inspect(target);r=json.loads(stats.read_text())
  check(r.get('frames_waited')==r.get('frames_begun')==r.get('frames_ended') and r.get('acquires')==r.get('image_waits')==r.get('releases') and r.get('swapchains_destroyed')==2 and r.get('left_eye_nonblank',0)>0 and r.get('right_eye_nonblank',0)>0,'Polygon '+mode+' balances both real GL eye images and session ownership')
  check(r.get('polygon_initial_ready') is True and r.get('polygon_script_step',0)>=36 and not r.get('polygon_gate_exhausted') and r.get('tracking_losses',0)==0,'Polygon '+mode+' reaches actual enabled controls without tracking/timeline shortcuts')
  if not mode.startswith('restore'):
   check(r.get('polygon_source_ready') is True and r.get('polygon_selection_ready') is True,'Polygon '+mode+' waits for the actual prepared workspace and its enabled operation control')
  if mode=='accept':
   accepted=after;obj=after['objects'][0];authority=after['polygon_sources']['authorities'][obj['polygon']]
   check(after['version']==12 and len(authority['topology']['faces'])==10 and obj['painted']['uv_policy']=='polygon-authored','Actual tracked aim/pressure plus joystick Extrude/Apply stores real N-gons and new side faces')
   old=dict(original['objects'][0]);new=dict(obj)
   for key in ('painted','polygon'):old.pop(key,None);new.pop(key,None)
   check(old==new,'Actual XR topology acceptance preserves exact source sculpt/layers/masks, object/material/transform and identities')
   origin=after['polygon_sources']['origins'][authority['origin']]
   prior=after['polygon_sources']['authorities'][origin['polygon']]
   old_authority=original['polygon_sources']['authorities'][original['objects'][0]['polygon']]
   check(after['polygon_sources']['paints'][origin['painted']]==original['objects'][0]['painted'] and after['polygon_sources']['volumes'][origin['volume']]==original['objects'][0]['volume'] and prior['topology']==old_authority['topology'],'Accepted XR Scene12 retains the complete original paint/sculpt and editable N-gon source capsule')
   check(r.get('polygon_edit_ready') is True and r.get('polygon_pressure_samples',0)>0,'Actual enabled Apply follows completed topology preparation and real pressure selection')
  elif mode.startswith('restore'):
   expected=original if mode=='restore' else accepted if accepted is not None else inspect(output/'accept.dsculpt')
   check(after==expected,'XR '+mode+' observes explicit source choice/default Cancel and preserves the exact expected complete source')
  else:check(after==original,'Polygon '+mode+' preserves the complete committed scene, source archives and authoring values exactly')
  if mode=='focus':check(r.get('focus_losses')==1 and r.get('focus_regains')==1 and r.get('polygon_motion_base_ready') is True and r.get('polygon_motion_visible') is True and r.get('polygon_motion_changed_pixels',0)>100,'Actual focus loss cancels a rendered GPU component move; cursor colors do not acknowledge surface movement')
report(complete=True)
print(str(len(checks))+' actual-loader polygon checks passed',flush=True)
