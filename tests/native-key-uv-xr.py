#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual native loader with private synthetic tracking; no physical session."""
import json,os,pathlib,subprocess,time,hashlib,signal,sys
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if len(sys.argv)!=6:raise SystemExit('usage: native-key-uv-xr.py APP RUNTIME INSPECTOR FIXTURE OUTPUT')
binary,runtime,helper,fixture_source,out=[pathlib.Path(p).resolve() for p in sys.argv[1:]]
out.mkdir(parents=True,exist_ok=True);checks=[]
def check(x,label):
 checks.append({'passed':bool(x),'check':label});print(('PASS ' if x else 'FAIL ')+label,flush=True);(out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks),'synthetic_tracking':True},indent=2)+'\n');assert x,label
def paint_value(s,p):
 v=dict(p)
 if v.get('version')==2:
  d=dict(s['paint_sampling']['domains'][v['domain']]);d['basis']=s['paint_sampling']['bases'][d['basis']];v['domain']=d
 return v
def nodes(s,name):
 result=[]
 for n in s['source_materials'][name]:
  v=dict(n)
  if v.get('paint') is not None:v['paint']=paint_value(s,s['source_materials']['paints'][v['paint']])
  result.append(v)
 return result
sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest();inputs={str(p):sha(p) for p in [binary,helper,runtime,fixture_source]};(out/'before.json').write_text(json.dumps(inputs,indent=2)+'\n');start=time.monotonic();statuses=[]
for mode in ['accept','cancel','focus']:
 d=out/mode;d.mkdir(exist_ok=True);home=d/'home';home.mkdir(exist_ok=True);manifest=d/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}})+'\n');fixture=d/'original.dsculpt';fixture.write_bytes(fixture_source.read_bytes());target=d/'result.dsculpt';report=d/'graphics.json';env=os.environ.copy();env.update(XR_RUNTIME_JSON=str(manifest),LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',HOME=str(home),XDG_RUNTIME_DIR=str(home),XDG_CONFIG_HOME=str(home/'config'),XDG_STATE_HOME=str(home/'state'),DOME_XR_FIXTURE_MENU_GATE='1',DOME_XR_FIXTURE_UV='1',DOME_XR_FIXTURE_NORMAL_DOMAIN=mode,DOME_XR_FIXTURE_PAINT_DOMAIN='1',DOME_XR_FIXTURE_REPORT=str(report));inspect=lambda p:json.loads(subprocess.check_output([str(helper),str(p)],env=env,text=True));original=inspect(fixture);data=fixture.read_bytes();cmd=[str(binary),'--xr','--open',str(fixture),'--frames','42','--save',str(target)];t=time.monotonic();maps=''
 with open(d/'application.log','w') as log:
  child=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT)
  while child.poll() is None:
   if time.monotonic()-t>90:child.terminate();child.wait();raise TimeoutError('Native key UV real-loader bounded run timed out')
   try:
    current=(pathlib.Path('/proc')/str(child.pid)/'maps').read_text()
    if 'libgallium' in current:maps=current
   except OSError:pass
   time.sleep(.01)
  code=child.wait()
 (d/'actual-app-maps.txt').write_text(maps);statuses.append({'mode':mode,'command':cmd,'pid':child.pid,'exit':code,'seconds':time.monotonic()-t});check(code==0,mode+': actual native OpenXR loader closes and drains accepted Save');graphics=json.load(open(report));after=inspect(target);check('llvmpipe' in graphics.get('gl_renderer','').lower(),mode+': actual requested software device renders OpenXR swapchains');check(graphics.get('acquires',0)>0 and graphics['acquires']==graphics['image_waits']==graphics['releases'],mode+': both swapchain image ownership streams remain balanced');check(graphics.get('left_eye_nonblank',0)>0 and graphics.get('right_eye_nonblank',0)>0 and graphics.get('palette_images',0)>0,mode+': actual stereo images contain the interactive UV palette');check(fixture.read_bytes()==data,mode+': complete original input file remains exact')
 if mode=='accept':
  check(after['version']==36 and after['source_materials']['version']==18,mode+': real controller Scale/Confirm installs current native key/paint ownership');a=next(o for o in after['objects'] if o['id']==after['active']);o=next(o for o in original['objects'] if o['id']==original['active']);check(a['painted']['payload']['mesh']!=o['painted']['payload']['mesh'],mode+': actual accepted chart changes rather than saving a no-op');check(after['source_materials']['normal_frames']==original['source_materials']['normal_frames'] and all(nodes(after,k)==nodes(original,k) for k in ['key_derivations','native_frame_sources','origins']),mode+': every complete current key/frame/capture record survives with resolved paint authority');check(all(a.get(k)==o.get(k) for k in ['deformation','shape_source','rig_source','appearance','polygon','material']),mode+': actual posed skin/morph and original full sources remain exact');check({k:v for k,v in a['painted']['payload'].items() if k!='mesh'}=={k:v for k,v in o['painted']['payload'].items() if k!='mesh'},mode+': all editable channels/masks/layer values survive controller UV transport')
 else:check(after==original,mode+': real controller Cancel or focus loss discards the entire UV draft and preserves all source/paint/key values')
check(all(sha(p)==h for p,h in inputs.items()),'All observed native app/core/render/runtime/helper/input files remain unchanged');(out/'status.json').write_text(json.dumps({'state':'terminal','seconds':time.monotonic()-start,'steps':statuses,'inputs':inputs,'drift':[p for p,h in inputs.items() if sha(p)!=h]},indent=2)+'\n')
