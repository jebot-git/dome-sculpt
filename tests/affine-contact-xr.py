#!/usr/bin/env python3
"""Scene13 contact painting through real OpenXR loader/stereo GL; synthetic tracking."""
import json,os,signal,subprocess,sys,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,helper,output=map(lambda p:Path(p).resolve(),sys.argv[1:]);output.mkdir(parents=True,exist_ok=True)
checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True)
 (output/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks)},indent=2))
 assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-affine-contact-xr-') as directory:
 root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'0' if os.environ.get('DOME_CONTACT_XR_HARDWARE')=='1' else '1','LP_NUM_THREADS':'2','HOME':directory,'XDG_RUNTIME_DIR':directory,'XDG_CONFIG_HOME':directory+'/config','XDG_STATE_HOME':directory+'/state'}
 fixture=root/'source.dsculpt';subprocess.run([str(helper),'--ui-fixture',str(fixture)],check=True,env=env)
 def inspect(p):return json.loads(subprocess.check_output([str(helper),'--inspect',str(p)],env=env))
 original=inspect(fixture)
 for mode in ('accept','focus'):
  target=output/(mode+'.dsculpt');report=output/(mode+'.json')
  with (output/(mode+'.log')).open('w') as log:
   result=subprocess.run([str(binary),'--xr','--open',str(fixture),'--frames','32','--save',str(target)],env={**env,'DOME_XR_FIXTURE_REPORT':str(report),'DOME_XR_FIXTURE_AFFINE_CONTACT':mode},stdout=log,stderr=subprocess.STDOUT,timeout=90)
  check(result.returncode==0,'Affine '+mode+' closes and saves normally through real loader')
  after=inspect(target);r=json.loads(report.read_text())
  check(('Arc' in r.get('gl_renderer','')) if os.environ.get('DOME_CONTACT_XR_HARDWARE')=='1' else ('llvmpipe' in r.get('gl_renderer','').lower()),'Actual '+mode+' graphics renderer matches the requested test device')
  check(r.get('acquires')==r.get('image_waits')==r.get('releases') and r.get('left_eye_nonblank',0)>0 and r.get('right_eye_nonblank',0)>0 and r.get('tracking_losses',0)==0,'Affine '+mode+' renders balanced nonblank stereo with actual GL images')
  if mode=='accept':
   check(after['center_coverage']>200 and after['scene']!=original['scene'],'Real controller grip paints original affine source UV center')
   a=dict(after['scene']);b=dict(original['scene']);a['objects'][0]['painted']=b['objects'][0]['painted']
   check(a==b,'XR contact retains all original placement/source/geometry/material fields')
  else:
   check(after==original,'Actual OpenXR focus loss restores complete unaccepted affine source exactly')
   check(r.get('focus_losses')==1 and r.get('focus_regains')==1,'Focus cancellation crosses actual runtime loss and regain events')
print(str(len(checks))+' actual-loader affine contact checks passed',flush=True)
