#!/usr/bin/env python3
"""Focused existing real-loader retopo controls on a genuine affine fixture.

Synthetic tracking and software GL only: no physical HMD or device claim.
The runtime retains its independently fixed world-z=0 triangle positions.
"""
import hashlib,json,os,signal,subprocess,sys
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK, [])
binary,runtime,inspector,fixture,out=map(lambda p:Path(p).resolve(),sys.argv[1:])
out.mkdir(parents=True,exist_ok=True)
checks=[]
def check(ok,message):
    checks.append({'passed':bool(ok),'check':message})
    (out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['passed'] for x in checks)},indent=2)+'\n')
    print(('PASS ' if ok else 'FAIL ')+message,flush=True)
    assert ok,message
def scene(path):
    value=json.loads(subprocess.check_output([str(inspector),str(path)],text=True))
    for obj in value['objects']:
        volume=obj['volume'];volume['samples'].sort()
        for layer in volume.get('sculpt_layers',[]):layer['delta'].sort()
        if 'sculpt_mask' in volume:volume['sculpt_mask']['values'].sort()
    return value
manifest=out/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
home=out/'home';home.mkdir(exist_ok=True)
env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2',
     'HOME':str(home),'XDG_RUNTIME_DIR':str(home),'XDG_CONFIG_HOME':str(home/'config'),'XDG_STATE_HOME':str(home/'state')}
for key in ('DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
original_hash=hashlib.sha256(fixture.read_bytes()).hexdigest()
before=scene(fixture)
def run(mode,frames):
    path=out/(mode+'.dsculpt');report=out/(mode+'.json')
    e={**env,'DOME_XR_FIXTURE_RETOPO':mode,'DOME_XR_FIXTURE_REPORT':str(report)}
    with (out/(mode+'.log')).open('w') as log:
        p=subprocess.Popen([str(binary),'--xr','--frames',str(frames),'--save',str(path),'--open',str(fixture)],env=e,stdout=log,stderr=subprocess.STDOUT)
        # Actual process maps are provenance; clean exit remains mandatory.
        import time
        for _ in range(100):
            if p.poll() is not None:break
            try:
                mapped=Path('/proc/'+str(p.pid)+'/maps').read_text()
                if 'libgallium' in mapped:(out/(mode+'-maps.txt')).write_text(mapped);break
            except FileNotFoundError:pass
            time.sleep(.05)
        rc=p.wait(timeout=120)
    check(rc==0,'Actual loader retopology '+mode+' exits cleanly without suppression')
    check(path.is_file(),'Actual '+mode+' writes its requested native output')
    return scene(path),json.loads(report.read_text())
committed,_=run('committed',14)
source=committed['objects'][0]
check(source['material']['color'][2]>.7 and source['material']['color'][0]<.1,
      'Real controller commits its material change before the retopo draft')
expected=dict(before['objects'][0]);actual=dict(source);expected.pop('material');actual.pop('material')
check(expected==actual,'Committed controller prefix retains every affine, posed, painted and sculpt source channel')
for mode in ('accept','focus','unconfirmed'):
    after,report=run(mode,110)
    check(report.get('retopo_source_ready') is True and report.get('retopo_script_step',0)>=63 and report.get('tracking_losses',0)==0,
          mode+' reaches actual menu Resume without a tracking-loss shortcut')
    check(report['frames_waited']==report['frames_begun']==report['frames_ended'] and
          report['acquires']==report['image_waits']==report['releases'] and report['swapchains_destroyed']==2,
          mode+' balances actual frame/stereo-image/shutdown ownership')
    if mode=='unconfirmed':
        check(after==committed,'Unconfirmed standalone Save cancels the draft and preserves the exact committed prefix')
        check(report.get('retopo_pressure_samples',0)>=3,'Unconfirmed case contains actual pressure placement')
        continue
    check(report.get('retopo_apply_ready') is True and len(after['objects'])==2 and
          not after['objects'][0]['visible'] and after['objects'][1]['visible'],
          mode+' uses genuine Face/Apply and retains the hidden affine source')
    retained=dict(after['objects'][0]);expected=dict(source)
    retained.pop('visible');expected.pop('visible')
    check(retained==expected,mode+' preserves full original paint, pose, morphs, affine placement and source identities')
    patch=after['objects'][1]['retopology'];topology=json.loads(patch['topology'])
    positions=sorted(v[1] for v in topology['vertices'])
    want=sorted([[-.2,-.2,0],[.2,-.2,0],[0,.2,0]])
    check(patch['source']==source['id'] and len(topology['faces'])==1 and len(positions)==3 and
          all(abs(a-b)<1e-5 for p,q in zip(positions,want) for a,b in zip(p,q)),
          mode+' independently fixed controller points project onto the real evaluated affine plane')
    if mode=='focus':
        check(report.get('focus_losses')==1 and report.get('focus_regains')==1,
              'Actual focus loss and regain are observed')
        check(all(abs(p[0]-.35)>.01 or abs(p[1]-.25)>.01 for p in positions),
              'Focus loss cancels the held point without discarding earlier draft points')
check(hashlib.sha256(fixture.read_bytes()).hexdigest()==original_hash,'All real-loader cases leave the immutable fixture bytes unchanged')
