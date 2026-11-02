#!/usr/bin/env python3
"""Actual loader/shared View page; deterministic synthetic tracking, no headset."""
import json,os,signal,subprocess,sys,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,helper,output=[Path(p).resolve() for p in sys.argv[1:]]
output.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True)
 (output/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks),'synthetic_tracking':True},indent=2)+'\n');assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-pbr-xr-') as directory:
 home=Path(directory);manifest=home/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 fixture=home/'source.dsculpt';target=output/'result.dsculpt';report=output/'graphics.json'
 env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','HOME':directory,'XDG_RUNTIME_DIR':directory,'XDG_CONFIG_HOME':directory+'/config','XDG_STATE_HOME':directory+'/state','DOME_XR_FIXTURE_MENU_GATE':'1','DOME_XR_FIXTURE_PBR_VIEW':'1','DOME_XR_FIXTURE_REPORT':str(report)}
 subprocess.run([str(helper),'--fixture',str(fixture)],env=env,check=True)
 inspect=lambda p:json.loads(subprocess.check_output([str(helper),'--inspect',str(p)],env=env,text=True))
 original=inspect(fixture);source=fixture.read_bytes()
 with (output/'application.log').open('w') as stream:
  result=subprocess.run([str(binary),'--xr','--open',str(fixture),'--frames','42','--save',str(target)],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=90)
 check(result.returncode==0,'Actual OpenXR client closes and drains its native ordered Save')
 r=json.loads(report.read_text());check('llvmpipe' in r.get('gl_renderer','').lower(),'Requested software device renders the actual OpenXR images')
 check(r.get('acquires',0)>0 and r['acquires']==r['image_waits']==r['releases'],'Actual stereo swapchain ownership stays balanced')
 check(r.get('left_eye_nonblank',0)>0 and r.get('right_eye_nonblank',0)>0 and r.get('palette_images',0)>0,'Both eyes contain the real scene and shared interactive palette')
 for eye in ('0','1'):
  images=r.get('pbr_preview_images',{});authored=images.get('6',{}).get(eye,{}).get('opaque',0);pbr=images.get('18',{}).get(eye,{}).get('opaque',0);restored=images.get('34',{}).get(eye,{}).get('opaque',0)
  check(authored>500 and pbr>authored+500,'Eye '+eye+': root View header and actual joystick select genuine PBR alpha policy')
  check(restored>500 and restored==authored,'Eye '+eye+': second actual View command restores authored alpha policy')
 check(fixture.read_bytes()==source and inspect(target)==original,'Controller preview and restart preserve exact original scene, paint, frames and authored materials')
 check(not r.get('menu_gate_exhausted',False),'Shared View workflow completes within the actual-raster acknowledgement bound')
print(str(len(checks))+' actual-loader PBR preview checks passed')
