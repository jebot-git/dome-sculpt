#!/usr/bin/env python3
"""Actual OpenXR loader/stereo expression controls; synthetic input, no HMD."""
import argparse,json,os,signal,subprocess,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
parser=argparse.ArgumentParser(description=__doc__)
for name in ('binary','runtime','helper','inspector','source','out'):parser.add_argument(name,type=lambda p:Path(p).resolve())
parser.add_argument('--hardware',action='store_true',help='Require actual Intel Arc A770; caller selects PCI device/environment')
args=parser.parse_args();out=args.out;out.mkdir(parents=True,exist_ok=True);checks=[]
def check(value,label):
 checks.append({'passed':bool(value),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'physical_headset':False,'hardware_requested':args.hardware},indent=2)+'\n');print(('PASS 'if value else 'FAIL ')+label,flush=True);assert value,label
with tempfile.TemporaryDirectory(prefix='dome-expression-xr-')as home:
 home=Path(home);manifest=home/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(args.runtime)}}));env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LP_NUM_THREADS':'2','EGL_PLATFORM':'surfaceless','HOME':str(home),'XDG_RUNTIME_DIR':str(home),'XDG_CONFIG_HOME':str(home/'config'),'XDG_STATE_HOME':str(home/'state'),'XDG_DATA_HOME':str(home/'data')}
 for name in ['DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(name,None)
 if not args.hardware:env['LIBGL_ALWAYS_SOFTWARE']='1'
 source=home/'source.dsculpt';source.write_bytes(args.source.read_bytes());original_bytes=source.read_bytes();expected=home/'expected.dsculpt';subprocess.run([str(args.helper),'--expected',str(source),'0.1',str(expected)],env=env,check=True)
 inspect=lambda path:json.loads(subprocess.check_output([str(args.inspector),str(path)],env=env,text=True));original=inspect(source);proposed=inspect(expected)
 for mode in ['accept','cancel','focus']:
  target=out/(mode+'.dsculpt');report=out/(mode+'.json')
  with(out/(mode+'.log')).open('w')as log:p=subprocess.run([str(args.binary),'--xr','--open',str(source),'--frames','150','--save',str(target)],env={**env,'DOME_XR_FIXTURE_EXPRESSIONS':mode,'DOME_XR_FIXTURE_REPORT':str(report)},stdout=log,stderr=subprocess.STDOUT,timeout=180)
  check(p.returncode==0,mode+': actual native loader client exits after ordered Save');detail=json.loads(report.read_text());after=inspect(target)
  check(after==(proposed if mode=='accept'else original),mode+': actual Apply or Cancel/focus preserves complete native source/paint/rig/history semantics')
  check(source.read_bytes()==original_bytes and args.source.read_bytes()==original_bytes,mode+': original native input bytes remain untouched')
  check(detail.get('acquires',0)>0 and detail.get('acquires')==detail.get('image_waits')==detail.get('releases'),mode+': real two-eye GL swapchain ownership is balanced')
  renderer=detail.get('gl_renderer','').lower();vendor=detail.get('gl_vendor','').lower()
  check(('intel'in vendor and'arc'in renderer and'a770'in renderer and'llvmpipe'not in renderer and'softpipe'not in renderer)if args.hardware else'llvmpipe'in renderer,mode+': actual renderer matches the explicitly requested device')
  for frame in ['68','85']:check(all(detail.get('expression_images',{}).get(frame,{}).get(eye,0)>1000 for eye in ['0','1']),mode+': both actual eyes render shared expression controls at frame'+frame)
  if mode=='focus':check(detail.get('focus_losses')==1 and detail.get('focus_regains')==1,'Actual loader focus loss/regain cancels owning preview')
print(str(len(checks))+' actual-loader expression checks passed',flush=True)
