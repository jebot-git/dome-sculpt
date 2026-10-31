#!/usr/bin/env python3
"""Selection through actual loader/stereo GL, deterministic synthetic tracking."""
import json,os,signal,subprocess,sys,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,helper,output=map(lambda x:Path(x).resolve(),sys.argv[1:]);output.mkdir(parents=True,exist_ok=True)
checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True)
 (output/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks),'synthetic_tracking':True},indent=2)+'\n')
 assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-selection-xr-') as directory:
 root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 hardware=os.environ.get('DOME_SELECTION_HARDWARE')=='1'
 env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'0' if hardware else '1','LP_NUM_THREADS':'2','HOME':directory,'XDG_RUNTIME_DIR':directory,'XDG_CONFIG_HOME':directory+'/config','XDG_STATE_HOME':directory+'/state'}
 fixture=root/'source.dsculpt';subprocess.run([str(helper),'--fixture',str(fixture)],env=env,check=True)
 inspect=lambda p:json.loads(subprocess.check_output([str(helper),'--inspect',str(p)],env=env,text=True))
 original=inspect(fixture);target=output/'selected.dsculpt';report=output/'runtime.json'
 with (output/'application.log').open('w') as log:
  result=subprocess.run([str(binary),'--xr','--open',str(fixture),'--frames','42','--save',str(target)],env={**env,'DOME_XR_FIXTURE_REPORT':str(report),'DOME_XR_FIXTURE_SELECTION':'1'},stdout=log,stderr=subprocess.STDOUT,timeout=90)
 check(result.returncode==0,'Actual native OpenXR selection client closes and saves normally')
 r=json.loads(report.read_text());after=inspect(target)
 check(('Arc' in r.get('gl_renderer','')) if hardware else ('llvmpipe' in r.get('gl_renderer','').lower()),'Requested graphics device renders the actual released stereo images')
 check(r.get('acquires')==r.get('image_waits')==r.get('releases') and r.get('left_eye_nonblank',0)>0 and r.get('right_eye_nonblank',0)>0,'Balanced native stereo swapchains contain visible selection scene')
 for eye in ('0','1'):
  first=r.get('selection_images',{}).get('24',{}).get(eye,{})
  multi=r.get('selection_images',{}).get('29',{}).get(eye,{})
  check(first.get('red_solid',0)>100 and first.get('green_solid',0)<20 and first.get('ghost',0)>100,'Eye '+eye+': direct trigger keeps active mesh solid and unselected meshes transparent')
  check(multi.get('red_solid',0)>100 and multi.get('green_solid',0)>100 and multi.get('ghost',0)>100,'Eye '+eye+': grip+trigger creates two solid selected meshes and retains transparent reference')
 check(after['objects']==original['objects'],'Real controller selection changes no original object transform, paint, topology or materials')
 check(after['active']==original['objects'][1]['id'],'Last controller-added mesh becomes the saved active object')
 check(all(r.get('selection_images',{}).get('40',{}).get(eye,{}).get('opaque',0)>9000 for eye in ('0','1')),'Actual XR hierarchy is visibly drawn in both eyes after direct header navigation')
 check((output/'runtime.json.selection-40.ppm').is_file(),'Actual XR hierarchy image retained for visual inspection')
print(str(len(checks))+' actual-loader selection checks passed')
