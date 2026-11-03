#!/usr/bin/env python3
"""Synthetic XR + real Arc GL/compute; no headset or session restart."""
import json,os,subprocess,tempfile,re,sys
from pathlib import Path
if len(sys.argv)!=6:raise SystemExit("usage: resolution-quality-xr.py ONEAPI_APP RUNTIME CPU_FIXTURE INSPECTOR OUTPUT")
binary,runtime,oracle,helper,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
source=out/'source.dsculpt';reference=out/'expected.dsculpt'
with (out/'oracle.log').open('w') as stream:subprocess.run([str(oracle),str(source),str(reference)],check=True,stdout=stream,stderr=subprocess.STDOUT)
checks=[]
def check(value,label):
 checks.append({'passed':bool(value),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['passed'] for x in checks)},indent=2));print(('PASS ' if value else 'FAIL ')+label,flush=True);assert value,label
def inspect(path):
 j=json.loads(subprocess.check_output([str(helper),'--inspect',str(path)]))
 for o in j['objects']:
  v=o['volume'];v['samples'].sort()
  for layer in v.get('sculpt_layers',[]):layer['delta'].sort()
  if 'sculpt_mask' in v:v['sculpt_mask']['values'].sort()
 return j
original=inspect(source);expected=inspect(reference)
check(len(original['objects'][0]['volume']['sculpt_layers'][0]['delta'])>=32768,'Original raw layer qualifies for measured aggregate tier')
with tempfile.TemporaryDirectory(prefix='dome-quality-xr-') as home:
 manifest=Path(home)/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 for mode in ['accept','cancel']:
  target=out/(mode+'.dsculpt');report=out/(mode+'.json');log=out/(mode+'.log')
  env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'HOME':home,'XDG_RUNTIME_DIR':home,'XDG_CONFIG_HOME':home+'/config','XDG_STATE_HOME':home+'/state','LIBGL_ALWAYS_SOFTWARE':'0','DOME_XR_FIXTURE_REPORT':str(report),'DOME_XR_FIXTURE_RESOLUTION':mode,'UR_ENABLE_LAYERS':'UR_LAYER_TRACING','UR_LOG_TRACING':'level:info'}
  for name in ['GALLIUM_DRIVER','MESA_LOADER_DRIVER_OVERRIDE','WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(name,None)
  with log.open('w') as stream:
   process=subprocess.run([str(binary),'--xr','--open',str(source),'--frames','36','--save',str(target)],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=90)
  check(process.returncode==0,'Actual native XR '+mode+' drains and closes normally')
  r=json.loads(report.read_text());renderer=r.get('gl_renderer','');check('Arc' in renderer and 'A770' in renderer,'Actual '+mode+' stereo renderer is Intel Arc A770: '+renderer)
  check(r.get('acquires')==r.get('image_waits')==r.get('releases') and r.get('left_eye_nonblank',0)>0 and r.get('right_eye_nonblank',0)>0 and r.get('palette_images',0)>0,'Both GL eyes and resolution palette render with balanced image ownership')
  actual=inspect(target)
  if mode=='accept':
   check(actual['objects']==expected['objects'],'Joystick/ray accepted coarsening matches complete CPU-reference object data exactly')
   text=log.read_text();kernels=[line for line in text.splitlines() if '<--- urKernelCreate' in line and 'SparseFieldSampler13maximum_error' in line]
   reads=[line for line in text.splitlines() if '<--- urEnqueueUSMMemcpy' in line and re.search(r'\.size = 8(?:,| )',line)]
   check(len(kernels)>=2 and len(reads)>=1,'Actual rendering application executes both aggregate kernels and scalar8 readback')
   (out/'aggregate-trace-summary.json').write_text(json.dumps({'kernel_create_lines':kernels,'scalar_readback_lines':reads},indent=2))
  else:check(actual==original,'Cancelled actual XR coarsening preserves complete original native scene')
print(len(checks),'native XR aggregate checks passed',flush=True)
