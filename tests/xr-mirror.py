#!/usr/bin/env python3
"""Actual OpenXR loader/GL stereo; synthetic Mirror controller actions."""
import argparse,json,os,signal,subprocess,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
parser=argparse.ArgumentParser(description=__doc__)
for name in ('binary','runtime','helper','out'):parser.add_argument(name,type=lambda p:Path(p).resolve())
parser.add_argument('--hardware',action='store_true',help='Require Intel Arc A770; caller selects PCI device/environment, software fallback refuses')
args=parser.parse_args();binary,runtime,helper,out=args.binary,args.runtime,args.helper,args.out
out.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'physical_headset':False,'requested_hardware':args.hardware},indent=2)+'\n');print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-mirror-xr-') as directory:
 root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}));env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LP_NUM_THREADS':'2','HOME':str(root),'XDG_RUNTIME_DIR':str(root),'XDG_CONFIG_HOME':str(root/'config'),'XDG_STATE_HOME':str(root/'state'),'XDG_DATA_HOME':str(root/'data')}
 if not args.hardware:env['LIBGL_ALWAYS_SOFTWARE']='1'
 source=root/'source.dsculpt';subprocess.run([str(helper),'--ui-fixture',str(source)],env=env,check=True);before_bytes=source.read_bytes()
 def inspect(p):
  data=subprocess.check_output([str(helper),'--inspect',str(p)],env=env,text=True);return json.loads(next(line.removeprefix('DOME_MIRROR_JSON ') for line in data.splitlines() if line.startswith('DOME_MIRROR_JSON ')))
 original=inspect(source);(out/"source.dsculpt").write_bytes(before_bytes);(out/"source.json").write_text(json.dumps(original,indent=2)+"\n")
 for mode,method in [('accept',0),('accept',1),('accept',2),('cancel',0),('focus',0)]:
  stem=mode+'-'+str(method);target=out/(stem+'.dsculpt');report=out/(stem+'.json')
  with (out/(stem+'.log')).open('w') as log:r=subprocess.run([str(binary),'--xr','--open',str(source),'--frames','126','--save',str(target)],env={**env,'DOME_XR_FIXTURE_MIRROR':mode,'DOME_XR_FIXTURE_MIRROR_METHOD':str(method),'DOME_XR_FIXTURE_REPORT':str(report)},stdout=log,stderr=subprocess.STDOUT,timeout=120)
  check(r.returncode==0,stem+': actual native XR client exits and ordered Save finishes');detail=json.loads(report.read_text());after=inspect(target)
  renderer=detail.get('gl_renderer','').lower();vendor=detail.get('gl_vendor','').lower()
  if args.hardware:check('intel' in vendor and 'arc' in renderer and 'a770' in renderer and 'llvmpipe' not in renderer,stem+': actual Intel Arc A770 renders both eyes; software fallback refuses')
  else:check('llvmpipe' in renderer,stem+': actual renderer matches the explicit software fixture')
  check(detail.get('acquires',0)>0 and detail.get('acquires')==detail.get('image_waits')==detail.get('releases') and detail.get('left_eye_nonblank',0)>0 and detail.get('right_eye_nonblank',0)>0,stem+': real stereo swapchain ownership is balanced and both eyes are nonblank')
  check(all(detail.get('mirror_images',{}).get('66',{}).get(eye,0)>1000 for eye in ['0','1']),stem+': both actual GL eyes render the settings after real controller row edits')
  check(source.read_bytes()==before_bytes,stem+': original native project bytes remain untouched')
  if mode=='accept':
   check(len(after['keys'])==3 and after['keys'][:2]==original['keys'],stem+': explicit Mirror adds one real key while keeping both original targets exact')
   delta=after['keys'][2]['delta'];check(max(abs(a-b) for a,b in zip(delta[3],[-.015,0,.04]))<1e-6 and max(abs(x) for x in delta[0])<1e-6,stem+': independent off-centre counterpart oracle verifies REST-position mirroring')
   check(after['channels']==original['channels'] and after['first_source']==original['first_source'],stem+': skin pose material paint placement and ultimately restored first source stay exact')
   checkpoint=original['restored_source'] if method==0 else original['scene']
   check(after['restored_source']==checkpoint and after['restore_steps']==(1 if method==0 else 2),stem+': native first-source and explicit authored-checkpoint Restore semantics remain exact')
  else:check(after['scene']==original['scene'],stem+': actual Cancel/focus loss retains the exact complete native Scene')
  if mode=='focus':check(detail.get('focus_losses')==1 and detail.get('focus_regains')==1,stem+': fixture crosses actual loader focus loss and regain')
print(str(len(checks))+' actual-loader Mirror checks passed',flush=True)
