#!/usr/bin/env python3
"""Real native loader/stereo/joystick editable V1 export with synthetic tracking."""
import argparse,json,os,signal,struct,subprocess,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
p=argparse.ArgumentParser(description=__doc__)
for name in ['binary','runtime','helper','fixture','gate','out']:p.add_argument(name,type=lambda v:Path(v).resolve())
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True);checks=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});(a.out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['passed'] for x in checks),'physical_headset':False},indent=2)+'\n');print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-v1-editable-xr-') as home:
 root=Path(home);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(a.runtime)}}));original=a.fixture.read_bytes()
 env={**os.environ,'HOME':home,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2'}
 for name,suffix in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[name]=str(root/suffix);Path(env[name]).mkdir(mode=0o700)
 def inspect(path):return json.loads(subprocess.check_output([str(a.helper),str(path)],env={k:v for k,v in env.items() if k!='LD_PRELOAD'},text=True))
 baseline=inspect(a.fixture)
 for name,mode in [('accept256','256'),('accept512','512'),('accept1024','1024'),('cancel','cancel'),('retry','256'),('recover','recover')]:
  case=a.out/name;case.mkdir();source=case/'source.dsculpt';source.write_bytes(original);package=case/'source.vrm';gate=case/'gate';gate.mkdir();target=case/'final.dsculpt';report=case/'runtime.json';e={**env,'DOME_XR_FIXTURE_EDITABLE_AVATAR':mode,'DOME_XR_FIXTURE_FILES':'1','DOME_XR_FIXTURE_REPORT':str(report)}
  if mode=='recover':opened=a.out/'accept512/source.vrm'
  else:
   opened=source;e.update(LD_PRELOAD=os.environ.get('DOME_XR_TEST_PRELOAD_PREFIX','')+str(a.gate),DOME_TEST_EXPORT_OUTPUT=str(package),DOME_TEST_EXPORT_GATE_DIR=str(gate))
   if mode=='cancel':package.write_bytes(b'Prior native Avatar output')
  stamp=source.stat().st_mtime_ns
  with (case/'app.log').open('w') as log:result=subprocess.run([str(a.binary),'--xr','--open',str(opened),'--frames','3000','--save',str(target)],env=e,stdout=log,stderr=subprocess.STDOUT,timeout=240)
  check(result.returncode==0,name+': actual loader client exits cleanly and accepted worker/Save drains');detail=json.loads(report.read_text());check(detail.get('acquires',0)>0 and detail.get('acquires')==detail.get('image_waits')==detail.get('releases') and detail.get('left_eye_nonblank',0)>0 and detail.get('right_eye_nonblank',0)>0,name+': real stereo GL ownership balances with both nonblank eyes');check(inspect(target)==baseline,name+': final native Save preserves all source/paint/material/rig/morph authorities');check(a.fixture.read_bytes()==original,name+': immutable original input project remains unchanged')
  if mode=='recover':
   saves=list((Path(env['XDG_STATE_HOME'])/'dome-sculpt').glob('Recovery-*.dsculpt'));check(bool(saves) and any(inspect(path)==baseline for path in saves),name+': joystick Save after real package Open writes an independent native recovery project');continue
  check(detail.get('editable_avatar_durable_gate_observed') and (gate/'entered').exists(),name+': actual editable V1 worker reaches durable temporary output before controller continuation');check(all(detail.get('editable_avatar_images',{}).get('13',{}).get(eye,0)>1000 for eye in ['0','1']),name+': actual Files page with editable avatar choices is rendered in both eyes');check(source.stat().st_mtime_ns!=stamp and inspect(source)==baseline,name+': real joystick Native Save queues behind accepted export with exact complete Scene')
  if mode=='cancel':check(package.read_bytes()==b'Prior native Avatar output' and not list(case.glob('source.vrm.tmp-*')),name+': actual joystick Cancel work prevents replacement and cleans temporary output')
  else:
   check(inspect(package)==baseline,name+': actual selected-avatar package restores exact native owners');b=package.read_bytes();length=struct.unpack_from('<I',b,12)[0];metadata=json.loads(b[20:20+length]);check(metadata['extras']['domeSculptAuthoring']['scope']=='exportedObjects' and not metadata['extras']['domeSculptAuthoring']['fallback']['lossless'] and metadata['extensions']['VRMC_vrm']['specVersion']=='1.0',name+': actual output declares V1 selected subset and approximate portable view');binary=b[28+length:];sizes=[]
   for image in metadata.get('images',[]):
    view=metadata['bufferViews'][image['bufferView']];start=view.get('byteOffset',0);blob=binary[start:start+view['byteLength']]
    if blob.startswith(b'\x89PNG\r\n\x1a\n'):sizes.append(struct.unpack_from('>II',blob,16))
   size=256 if mode=='256' else int(mode);check((size,size) in sizes,name+': chosen controller policy reaches its real portable texture dimensions')
print('PASS',len(checks),'actual native editable V1 XR checks',flush=True)
