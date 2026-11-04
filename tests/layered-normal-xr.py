#!/usr/bin/env python3
"""Actual XR loader/stereo controls for explicit conversion; synthetic poses only."""
import argparse,json,os,signal,subprocess,tempfile,time
from pathlib import Path
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
p=argparse.ArgumentParser(description=__doc__)
for name in ('binary','runtime','helper','out'):p.add_argument(name,type=lambda v:Path(v).resolve())
a=p.parse_args();out=a.out;out.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'physical_headset':False},indent=2)+'\n');print(('PASS 'if ok else'FAIL ')+label,flush=True);assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-canonical-xr-')as home:
 root=Path(home);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(a.runtime)}}))
 env={k:v for k,v in os.environ.items()if not k.startswith('DOME_XR_FIXTURE')}
 env.update(XR_RUNTIME_JSON=str(manifest),LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',HOME=home,XDG_RUNTIME_DIR=home,XDG_CONFIG_HOME=home+'/config',XDG_STATE_HOME=home+'/state',XDG_CACHE_HOME=home+'/cache',XDG_DATA_HOME=home+'/data')
 for mode in ['accept','cancel','focus','solo']:
  source=out/(mode+'-source.dsculpt');expected=out/(mode+'-expected.dsculpt');target=out/(mode+'.dsculpt');report=out/(mode+'.json')
  subprocess.run([str(a.helper),'--ui-solo-fixture'if mode=='solo'else'--ui-fixture',str(source)],env=env,check=True)
  subprocess.run([str(a.helper),'--ui-solo-converted'if mode=='solo'else'--ui-converted',str(expected)],env=env,check=True)
  def inspect(file):return json.loads(subprocess.check_output([str(a.helper),'--inspect',str(file)],env=env,text=True))
  original=inspect(source)['scene'];wanted=inspect(expected)['scene'];before=source.read_bytes()
  with(out/(mode+'.log')).open('w')as log:
   client=subprocess.Popen([str(a.binary),'--xr','--open',str(source),'--frames','80','--save',str(target)],env={**env,'DOME_XR_FIXTURE_CANONICAL':mode,'DOME_XR_FIXTURE_REPORT':str(report)},stdout=log,stderr=subprocess.STDOUT)
   deadline=time.monotonic()+180
   try:
    while client.poll()is None:
     try:
      maps=Path('/proc',str(client.pid),'maps').read_text()
      if 'libgallium'in maps:(out/(mode+'.maps')).write_text(maps)
     except FileNotFoundError:pass
     if time.monotonic()>deadline:raise subprocess.TimeoutExpired(client.args,180)
     time.sleep(.05)
   finally:
    if client.poll()is None:client.terminate();client.wait()
  check(client.returncode==0,mode+': actual XR application exits normally and ordered native Save drains')
  detail=json.loads(report.read_text());after=inspect(target)
  check('llvmpipe'in detail.get('gl_renderer','').lower(),mode+': actual renderer is explicitly software rather than physical GPU qualification')
  check(detail.get('acquires',0)>0 and detail.get('acquires')==detail.get('image_waits')==detail.get('releases') and detail.get('left_eye_nonblank',0)>0 and detail.get('right_eye_nonblank',0)>0,mode+': actual GL stereo images balance ownership and both eyes contain output')
  check(all(detail.get('canonical_images',{}).get('34',{}).get(eye,0)>1000 for eye in ['0','1']),mode+': both captured real eyes display the open Source/cage controls before conversion input')
  # Fixed synthetic camera, 1024px eyes and 640px panel put the conversion
  # label here. The button blue/background fail this white-glyph predicate;
  # geometry or an invisible hot rectangle cannot satisfy it.
  label_pixels=[]
  for eye in [0,1]:
   image=Image.open(str(report)+'.canonical-34-'+str(eye)+'.ppm').convert('RGB')
   label_pixels.append(image.size==(1024,1024)and sum(r>210 and g>215 and b>220 for r,g,b in image.crop((300,720,800,765)).get_flattened_data())>40)
  check(all(label_pixels),mode+': actual bitmap conversion label is visible in both rendered eyes')
  check(source.read_bytes()==before,mode+': original native project bytes remain unchanged')
  if mode in ['accept','solo']:
   check(after['converted'] and after['scene']==wanted,mode+': actual joystick and held ray trigger publish exactly the independently produced native conversion')
   if mode=='solo':check(len(after['scene']['objects'])==1,mode+': historical normal detail converts without inventing a live Bake source or recipe')
  else:check(not after['converted'] and after['scene']==original,mode+': Cancel or focus loss preserves the complete original Scene')
  if mode=='focus':check(detail.get('focus_losses')==1 and detail.get('focus_regains')==1,mode+': actual loader focus loss/regain crossed while the trigger remained held')
print(str(len(checks))+' actual-loader canonical conversion checks passed',flush=True)
