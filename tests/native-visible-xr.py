#!/usr/bin/env python3
"""Actual OpenXR loader/client; synthetic tracking, private runtime, no HMD."""
import json,os,signal,subprocess,sys,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,helper,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True)
 (out/'report.json').write_text(json.dumps({'checks':checks,'physical_headset':False},indent=2)+'\n');assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-native-weight-xr-') as directory:
 root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','HOME':str(root),'XDG_RUNTIME_DIR':str(root),'XDG_CONFIG_HOME':str(root/'config'),'XDG_STATE_HOME':str(root/'state'),'XDG_CACHE_HOME':str(root/'cache'),'XDG_DATA_HOME':str(root/'data')}
 fixture=root/'source.dsculpt';subprocess.run([str(helper),'--ui-fixture',str(fixture)],check=True,env=env)
 def inspect(p):return json.loads(subprocess.check_output([str(helper),'--inspect',str(p)],env=env))
 original=inspect(fixture);expected=json.loads(subprocess.check_output([str(helper),'--expected',str(fixture)],env=env));check(not original['bound'] and expected['bound'],'Actual unskinned affine target has independent native expected output')
 target=out/'accepted.dsculpt';report=out/'runtime.json'
 with (out/'client.log').open('w') as log:r=subprocess.run([str(binary),'--xr','--open',str(fixture),'--frames','64','--save',str(target)],env={**env,'DOME_XR_FIXTURE_REPORT':str(report),'DOME_XR_FIXTURE_SKIN':'native'},stdout=log,stderr=subprocess.STDOUT,timeout=120)
 check(r.returncode==0,'Actual loader Native finite-handle controller session closes and drains Save')
 detail=json.loads(report.read_text());current=inspect(target)
 check('llvmpipe' in detail.get('gl_renderer','').lower(),'Actual stereo client records requested softwareGL, not a vendor compute claim')
 check(detail.get('acquires',0)>0 and detail.get('acquires')==detail.get('image_waits')==detail.get('releases'),'Actual loader stereo swapchain acquisition/wait/release balanced')
 check(detail.get('left_eye_nonblank',0)>0 and detail.get('right_eye_nonblank',0)>0,'Both actual rendered eyes contain visible authoring geometry')
 check(current['scene']==expected['scene'],'Actual joystick Method ray, Native31 Bind and native worker emit exact complete independent Scene')
 restored=json.loads(subprocess.check_output([str(helper),'--restore',str(target)],env=env));check(restored['scene']==original['scene'],'Actual XR-saved first capsule restores complete original unskinned morph paint and affine context')
print(str(len(checks))+' actual-loader Native weight checks passed',flush=True)
