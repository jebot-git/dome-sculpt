#!/usr/bin/env python3
"""Retained paint UV mode through the real loader with synthetic controllers."""
import json,os,signal,subprocess,sys
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,helper,output=[Path(p).resolve() for p in sys.argv[1:]]
output.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True)
 (output/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks),'synthetic_tracking':True},indent=2)+'\n');assert ok,label
for mode in ('accept','cancel','focus'):
 directory=output/mode;directory.mkdir(exist_ok=True);home=directory/'home';home.mkdir(exist_ok=True)
 manifest=directory/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 fixture=directory/'original.dsculpt';target=directory/'result.dsculpt';report=directory/'graphics.json'
 env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','HOME':str(home),'XDG_RUNTIME_DIR':str(home),'XDG_CONFIG_HOME':str(home/'config'),'XDG_STATE_HOME':str(home/'state'),'DOME_XR_FIXTURE_MENU_GATE':'1','DOME_XR_FIXTURE_UV':'1','DOME_XR_FIXTURE_NORMAL_DOMAIN':mode,'DOME_XR_FIXTURE_PAINT_DOMAIN':'1','DOME_XR_FIXTURE_REPORT':str(report)}
 subprocess.run([str(helper),'--ui-fixture',str(fixture)],env=env,check=True)
 inspect=lambda p:json.loads(subprocess.check_output([str(helper),'--inspect',str(p)],env=env,text=True))
 original=inspect(fixture);data=fixture.read_bytes()
 with (directory/'application.log').open('w') as stream:
  result=subprocess.run([str(binary),'--xr','--open',str(fixture),'--frames','42','--save',str(target)],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=90)
 check(result.returncode==0,mode+': actual loader client closes normally and drains the ordered native Save')
 graphics=json.loads(report.read_text());after=inspect(target)
 check('llvmpipe' in graphics.get('gl_renderer','').lower(),mode+': requested software device rendered actual OpenXR swapchains')
 check(graphics.get('acquires',0)>0 and graphics['acquires']==graphics['image_waits']==graphics['releases'],mode+': stereo image acquire, wait and release ownership remains balanced')
 check(graphics.get('left_eye_nonblank',0)>0 and graphics.get('right_eye_nonblank',0)>0 and graphics.get('palette_images',0)>0,mode+': actual stereo images contain the shared interactive UV palette')
 check(fixture.read_bytes()==data,mode+': original native project bytes remain untouched')
 if mode=='accept':
  check(after['version']==24 and after['source_materials']['version']==9 and bool(after['normal_sampling']['domains']) and bool(after['paint_sampling']['domains']),mode+': actual controller Keep paint ray, Scale axes and Confirm install independent paint/colour/normal ownership')
  check(after['source_materials']['normal_frames']==original['source_materials']['normal_frames'],mode+': all original and current normal/key frame attributes survive controller authoring')
  check(after['objects'][0]['painted']['payload']['mesh']!=original['objects'][0]['painted']['mesh'],mode+': the real accepted UV workspace changed its chart rather than saving a no-op')
 else:check(after==original,mode+': controller Cancel or actual session focus loss discards the complete draft and Keep-detail choice')
print(str(len(checks))+' actual-loader retained-paint UV checks passed')
