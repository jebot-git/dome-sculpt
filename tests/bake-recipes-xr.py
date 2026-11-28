#!/usr/bin/env python3
"""Actual saved-recipe ray Recall, control Confirm/Cancel and Bake through loader/GL stereo."""
import argparse,json,os,signal,subprocess,tempfile,time
from pathlib import Path
from xr_test_graphics import hardware_environment, hardware_renderer
signal.pthread_sigmask(signal.SIG_SETMASK,[])
p=argparse.ArgumentParser(description=__doc__)
for name in ('binary','runtime','helper','out'):p.add_argument(name,type=lambda v:Path(v).resolve())
p.add_argument('--texture-set',action='store_true',help='Replay Recall and Bake on the selected additional material')
p.add_argument('--mtoon',action='store_true',help='Use a typed MToon material on the selected texture set')
p.add_argument('--supersample',action='store_true',help='Select four samples with the actual controller before baking')
a=p.parse_args()
if a.mtoon and not a.texture_set:p.error('--mtoon requires --texture-set')
if a.supersample and a.texture_set:p.error('--supersample uses the primary-field cage fixture')
out=a.out;out.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'physical_headset':False},indent=2)+'\n');print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-cage-xr-') as directory:
 root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(a.runtime)}}))
 env={**hardware_environment(),'DOME_XR_FIXTURE_RECIPE':'1','XR_RUNTIME_JSON':str(manifest),'LP_NUM_THREADS':'2','HOME':str(root),'XDG_RUNTIME_DIR':str(root),'XDG_CONFIG_HOME':str(root/'config'),'XDG_STATE_HOME':str(root/'state'),'XDG_CACHE_HOME':str(root/'cache'),'XDG_DATA_HOME':str(root/'data')}
 source=out/'source.dsculpt';reference=out/'reference.dsculpt'
 if a.texture_set:
  if a.mtoon:env['DOME_BAKE_FIXTURE_MTOON']='1'
  subprocess.run([str(a.helper),'--texture-set-xr-fixture',str(source),str(reference)],env=env,check=True)
 else:
  subprocess.run([str(a.helper),'--ui-wide-fixture' if os.environ.get('DOME_RECIPE_WIDE')=='1' else '--ui-fixture',str(source)],env=env,check=True)
  if a.supersample:
   env['DOME_XR_FIXTURE_BAKE_SAMPLING']='1'
   subprocess.run([str(a.helper),'--ui-supersample-reference',str(source),str(reference)],env=env,check=True)
 before=source.read_bytes()
 def inspect(path):return json.loads(subprocess.check_output([str(a.helper),'--inspect',str(path)],env=env,text=True))
 original_inspection=inspect(source);original=original_inspection['scene'];(out/'source.json').write_text(json.dumps(original,indent=2)+'\n')
 for mode in ['accept','cancel','focus']:
  target=out/(mode+'.dsculpt');report=out/(mode+'.json')
  with (out/(mode+'.log')).open('w') as log:
   r=subprocess.Popen([str(a.binary),'--xr','--open',str(source),'--frames','120' if a.supersample else '100','--save',str(target)],env={**env,'DOME_XR_FIXTURE_CAGE':mode,'DOME_XR_FIXTURE_REPORT':str(report)},stdout=log,stderr=subprocess.STDOUT)
   deadline=time.monotonic()+180
   try:
    while r.poll() is None:
     try:
      mappings=Path('/proc',str(r.pid),'maps').read_text()
      if 'libgallium' in mappings:(out/(mode+'.maps')).write_text(mappings)
     except FileNotFoundError:pass
     if time.monotonic()>deadline:raise subprocess.TimeoutExpired(r.args,180)
     time.sleep(.05)
   finally:
    if r.poll() is None:r.terminate();r.wait()

  check(r.returncode==0,mode+': actual native XR client exits and ordered Save drains');detail=json.loads(report.read_text());after=inspect(target)
  check(hardware_renderer(detail),mode+': hardware OpenGL renders the actual swapchains')
  check(detail.get('acquires',0)>0 and detail.get('acquires')==detail.get('image_waits')==detail.get('releases') and detail.get('left_eye_nonblank',0)>0 and detail.get('right_eye_nonblank',0)>0,mode+': actual stereo ownership balances and both eyes contain rendered output')
  check(all(detail.get('cage_images',{}).get('58',{}).get(eye,0)>1000 for eye in ['0','1']),mode+': both real GL eyes render the edited Source/Cage controls')
  check(source.read_bytes()==before,mode+': input project bytes remain untouched')
  if a.texture_set:
   expected=reference.read_bytes() if mode=='accept' else before
   check(target.read_bytes()==expected,mode+': exact entire native project matches the selected-field Bake reference or the original cancelled project')
  elif mode=='accept':
   if a.supersample:
    check(all(detail.get('cage_images',{}).get('76',{}).get(eye,0)>1000 for eye in ['0','1']),mode+': both eyes render the sampling controls')
    check(target.read_bytes()==reference.read_bytes(),mode+': ray-edited sampling and ordered Save reproduce the whole supersampled project')
    continue
   check(after['baked'],mode+': actual ray Recall restores the projection range needed by the off-axis cage before explicit Bake')
   check(after['scene']['objects'][0]['bake_recipe']['settings']==original['objects'][0]['bake_recipe']['settings'],mode+': persisted recipe matches every originally recalled setting after the earlier controller range change')
   scene=after['scene'];check(after['sources']==original_inspection['sources'] and scene['objects'][0]['painted']['layers']==original['objects'][0]['painted']['layers'],mode+': complete hidden source/cage and target editable paint remain exact')
   color=scene['objects'][0]['painted']['baked']['color']['bytes'];check(all(color[i:i+4]==[0,0,255,255] for i in range(0,len(color),4)),mode+': every projected pixel matches independently placed off-axis blue source')
  else:check(after['scene']==original,mode+': Cancel/focus loss keeps the complete Scene exact')
  if mode=='focus':check(detail.get('focus_losses')==1 and detail.get('focus_regains')==1,mode+': actual loader focus loss and regain crossed')
print(str(len(checks))+' actual-loader custom cage checks passed',flush=True)
