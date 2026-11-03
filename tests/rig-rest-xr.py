#!/usr/bin/env python3
"""Actual loader/GL stereo with synthetic humanoid landmark/Apply/focus input."""
import json,os,signal,subprocess,sys,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,helper,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True);(out/'report.json').write_text(json.dumps({'checks':checks,'physical_headset':False},indent=2)+'\n');assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-rig-fit-xr-') as directory:
 root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','HOME':str(root),'XDG_RUNTIME_DIR':str(root),'XDG_CONFIG_HOME':str(root/'config'),'XDG_STATE_HOME':str(root/'state'),'XDG_CACHE_HOME':str(root/'cache'),'XDG_DATA_HOME':str(root/'data')}
 fixture=root/'source.dsculpt';subprocess.run([str(helper),'--ui-fixture',str(fixture)],check=True,env=env)
 def inspect(p):return json.loads(subprocess.check_output([str(helper),'--inspect',str(p)],env=env))
 original=inspect(fixture)
 for mode in ('accept','focus'):
  target=out/(mode+'.dsculpt');report=out/(mode+'.json')
  with (out/(mode+'.log')).open('w') as log:r=subprocess.run([str(binary),'--xr','--open',str(fixture),'--frames','96','--save',str(target)],env={**env,'DOME_XR_FIXTURE_REPORT':str(report),'DOME_XR_FIXTURE_RIG_FIT':mode},stdout=log,stderr=subprocess.STDOUT,timeout=90)
  check(r.returncode==0,'Actual rig fitting '+mode+' loader session closes and saves normally')
  current=inspect(target);detail=json.loads(report.read_text())
  check('llvmpipe' in detail.get('gl_renderer','').lower(),'Rig fitting '+mode+' records actual requested software renderer')
  check(detail.get('acquires')==detail.get('image_waits')==detail.get('releases') and detail.get('left_eye_nonblank',0)>0 and detail.get('right_eye_nonblank',0)>0 and detail.get('tracking_losses',0)==0,'Rig fitting '+mode+' balances actual nonblank stereo swapchains')
  if mode=='accept':
   check(current['rig_source'] and current['scene']['version']==25,'Real XR joystick and trigger place original bone head then fit its rest/bind data through native worker')
   landmark=current['scene']['objects'][0]['deformation']['landmarks']['points']['hips'];check(abs(landmark[0]-.01)<1e-5 and abs(landmark[1]-1)<1e-5,'Actual headset-space grip location maps to independent expected original-mesh landmark')
   a=current['scene']['objects'][0].copy();b=original['scene']['objects'][0].copy();
   for key in ('deformation','rig_source','shape_source'):a.pop(key,None);b.pop(key,None)
   for key in set(a)-set(b):
    if a[key] is None:a.pop(key)
   check(a==b,'XR rest fitting preserves exact painting geometry material and placement source')
  else:
   check(current==original,'Actual runtime focus loss retains the complete unaccepted complete rig source')
   check(detail.get('focus_losses')==1 and detail.get('focus_regains')==1,'Rig fitting cancellation crosses actual loader focus-loss and regain events')
print(str(len(checks))+' actual-loader humanoid fitting checks passed',flush=True)
