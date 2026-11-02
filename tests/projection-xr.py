#!/usr/bin/env python3
"""Real loader/stereo GL plus synthetic controller image browser and paint; no HMD claim."""
import json,os,signal,subprocess,sys,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,helper,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True);(out/'report.json').write_text(json.dumps({'checks':checks,'physical_headset':False},indent=2)+'\n');assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-projection-xr-') as directory:
 root=Path(directory);home=root/'images';home.mkdir();manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','HOME':str(home),'XDG_RUNTIME_DIR':str(root),'XDG_CONFIG_HOME':str(root/'config'),'XDG_STATE_HOME':str(root/'state'),'XDG_CACHE_HOME':str(root/'cache'),'XDG_DATA_HOME':str(root/'data')}
 fixture=home/'source.dsculpt';subprocess.run([str(helper),'--ui-fixture',str(fixture)],check=True,env=env);subprocess.run([str(helper),'--image-fixture',str(home/'projector.png')],check=True,env=env)
 def inspect(p):return json.loads(subprocess.check_output([str(helper),'--inspect',str(p)],env=env))
 original=inspect(fixture)
 for mode in ('accept','focus'):
  target=out/(mode+'.dsculpt');report=out/(mode+'.json')
  with (out/(mode+'.log')).open('w') as log:result=subprocess.run([str(binary),'--xr','--open',str(fixture),'--frames','96','--save',str(target)],env={**env,'DOME_XR_FIXTURE_REPORT':str(report),'DOME_XR_FIXTURE_PROJECTION':mode},stdout=log,stderr=subprocess.STDOUT,timeout=90)
  check(result.returncode==0,'Actual projection '+mode+' loader session closes and saves normally')
  current=inspect(target);r=json.loads(report.read_text())
  check('llvmpipe' in r.get('gl_renderer','').lower(),'Projection '+mode+' captures actual requested software GL renderer')
  check(r.get('acquires')==r.get('image_waits')==r.get('releases') and r.get('left_eye_nonblank',0)>0 and r.get('right_eye_nonblank',0)>0 and r.get('tracking_losses',0)==0,'Projection '+mode+' balances actual nonblank stereo swapchains without tracking loss')
  if mode=='accept':
   check(current['red']>240 and current['blue']<5 and current['coverage']>200,'Real joystick navigates image browser, loads PNG and paints its color through native worker')
   a=current['scene']['objects'][0].copy();b=original['scene']['objects'][0].copy();paint=a.pop('painted');before=b.pop('painted');check(a==b and paint['mesh']==before['mesh'],'Actual XR projection retains exact REST/source/material/placement fields')
  else:
   check(current==original,'Actual runtime focus loss cancels the complete unaccepted projected gesture')
   check(r.get('focus_losses')==1 and r.get('focus_regains')==1,'Projection cancellation crosses real loader focus-loss/regain events')
print(str(len(checks))+' actual loader projection checks passed',flush=True)
