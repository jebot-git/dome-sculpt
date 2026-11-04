#!/usr/bin/env python3
"""Actual saved-recipe ray Recall, control Confirm/Cancel and Bake through loader/GL stereo."""
import argparse,json,os,signal,subprocess,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
p=argparse.ArgumentParser(description=__doc__)
for name in ('binary','runtime','helper','out'):p.add_argument(name,type=lambda v:Path(v).resolve())
a=p.parse_args();out=a.out;out.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'physical_headset':False},indent=2)+'\n');print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-cage-xr-') as directory:
 root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(a.runtime)}}))
 env={**os.environ,'DOME_XR_FIXTURE_RECIPE':'1','XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','HOME':str(root),'XDG_RUNTIME_DIR':str(root),'XDG_CONFIG_HOME':str(root/'config'),'XDG_STATE_HOME':str(root/'state'),'XDG_CACHE_HOME':str(root/'cache'),'XDG_DATA_HOME':str(root/'data')};source=out/'source.dsculpt';subprocess.run([str(a.helper),'--ui-fixture',str(source)],env=env,check=True);before=source.read_bytes()
 def inspect(path):return json.loads(subprocess.check_output([str(a.helper),'--inspect',str(path)],env=env,text=True))
 original_inspection=inspect(source);original=original_inspection['scene'];(out/'source.json').write_text(json.dumps(original,indent=2)+'\n')
 for mode in ['accept','cancel','focus']:
  target=out/(mode+'.dsculpt');report=out/(mode+'.json')
  with (out/(mode+'.log')).open('w') as log:r=subprocess.run([str(a.binary),'--xr','--open',str(source),'--frames','100','--save',str(target)],env={**env,'DOME_XR_FIXTURE_CAGE':mode,'DOME_XR_FIXTURE_REPORT':str(report)},stdout=log,stderr=subprocess.STDOUT,timeout=180)
  check(r.returncode==0,mode+': actual native XR client exits and ordered Save drains');detail=json.loads(report.read_text());after=inspect(target)
  check('llvmpipe' in detail.get('gl_renderer','').lower(),mode+': explicit software renderer is actually used')
  check(detail.get('acquires',0)>0 and detail.get('acquires')==detail.get('image_waits')==detail.get('releases') and detail.get('left_eye_nonblank',0)>0 and detail.get('right_eye_nonblank',0)>0,mode+': actual stereo ownership balances and both eyes contain rendered output')
  check(all(detail.get('cage_images',{}).get('58',{}).get(eye,0)>1000 for eye in ['0','1']),mode+': both real GL eyes render the edited Source/Cage controls')
  check(source.read_bytes()==before,mode+': input project bytes remain untouched')
  if mode=='accept':
   check(after['baked'],mode+': actual ray Recall restores the projection range needed by the off-axis cage before explicit Bake')
   check(after['scene']['objects'][0]['bake_recipe']['settings']==original['objects'][0]['bake_recipe']['settings'],mode+': persisted recipe matches every originally recalled setting after the earlier controller range change')
   scene=after['scene'];check(after['sources']==original_inspection['sources'] and scene['objects'][0]['painted']['layers']==original['objects'][0]['painted']['layers'],mode+': complete hidden source/cage and target editable paint remain exact')
   color=scene['objects'][0]['painted']['baked']['color']['bytes'];check(all(color[i:i+4]==[0,0,255,255] for i in range(0,len(color),4)),mode+': every projected pixel matches independently placed off-axis blue source')
  else:check(after['scene']==original,mode+': Cancel/focus loss keeps the complete Scene exact')
  if mode=='focus':check(detail.get('focus_losses')==1 and detail.get('focus_regains')==1,mode+': actual loader focus loss and regain crossed')
print(str(len(checks))+' actual-loader custom cage checks passed',flush=True)
