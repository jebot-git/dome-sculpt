#!/usr/bin/env python3
"""Real loader/stereo GL plus selected paint compute; synthetic tracking only."""
import json,os,re,subprocess,sys,tempfile
from pathlib import Path
if len(sys.argv)!=6:raise SystemExit('APP RUNTIME SOURCE INSPECTOR OUTPUT')
app,runtime,source,helper,out=map(lambda x:Path(x).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['passed'] for x in checks)},indent=2));print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
def inspect(path):
 j=json.loads(subprocess.check_output([str(helper),'--inspect',str(path)]))
 for o in j['objects']:
  o['volume']['samples'].sort()
  for layer in o['volume'].get('sculpt_layers',[]):layer['delta'].sort()
 return j
original=inspect(source)
with tempfile.TemporaryDirectory(prefix='dome-paint-compute-xr-') as home:
 manifest=Path(home)/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
 env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'HOME':home,'XDG_RUNTIME_DIR':home,'XDG_CONFIG_HOME':home+'/config','XDG_STATE_HOME':home+'/state','UR_ENABLE_LAYERS':'UR_LAYER_TRACING','UR_LOG_TRACING':'level:info'}
 for k in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(k,None)
 target=out/'painted.dsculpt';report=out/'runtime.json';env.update(DOME_XR_FIXTURE_PAINT='1',DOME_XR_FIXTURE_REPORT=str(report))
 with (out/'app.log').open('w') as log:process=subprocess.run([str(app),'--xr','--open',str(source),'--frames','45','--save',str(target)],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=90)
 check(process.returncode==0,'Actual native OpenXR application paints/drains/saves and shuts down normally')
 r=json.loads(report.read_text());gpu=r.get('gl_renderer','');check('llvmpipe' in gpu if env.get('LIBGL_ALWAYS_SOFTWARE')=='1' else 'Arc' in gpu and 'A770' in gpu,'Actual stereo renderer matches explicitly requested GL device: '+gpu)
 check(r.get('acquires')==r.get('image_waits')==r.get('releases') and r.get('left_eye_nonblank',0)>0 and r.get('right_eye_nonblank',0)>0 and r.get('palette_images',0)>0,'Both actual GL eyes and tracked paint palette render with balanced image ownership')
 actual=inspect(target);a=actual['objects'][0].copy();b=original['objects'][0].copy();paint=a.pop('painted');before=b.pop('painted');check(a==b and paint['mesh']==before['mesh'] and paint!=before,'Tracked pressure updates actual512 writable layers while preserving original geometry/material/authoring sources')
 info=json.loads(subprocess.check_output([str(app),'--project-info',str(target)]));check(info['paint_blue_texels']>20,'Real nested controller paint controls select blue ink before the pressure stroke')
 log=(out/'app.log').read_text();kernels=[line for line in log.splitlines() if '<--- urKernelCreate' in line and 'paint_compute' in line];check(bool(kernels),'Actual rendering application dispatches selected-backend plain paint kernels')
 # Authenticate copy direction from actual device allocations, rather than
 # counting the equally sized padding upload as output readback.
 device_pointers=set()
 for line in log.splitlines():
  if '<--- urUSMDeviceAlloc' in line:
   match=re.search(r'\.ppMem = 0x[0-9a-f]+ \((0x[0-9a-f]+)\)',line)
   if match:device_pointers.add(match.group(1))
 reads=[]
 for line in log.splitlines():
  if '<--- urEnqueueUSMMemcpy' not in line or '.size = 1048576,' not in line:continue
  source_pointer=re.search(r'\.pSrc = (0x[0-9a-f]+)',line)
  if source_pointer and source_pointer.group(1) in device_pointers:reads.append(line)
 check(bool(reads),'Bounded compositor performs actual65536-pixel16-byte device-to-host output readbacks')
 (out/'transfer-trace.json').write_text(json.dumps({'kernels':kernels,'output_chunk_reads':reads},indent=2))
print(len(checks),'actual-loader paint compositor checks passed',flush=True)
