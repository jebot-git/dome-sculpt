#!/usr/bin/env python3
"""Actual loader/stereo GL/controller procedural workflow; synthetic devices."""
import json, os, signal, subprocess, sys, tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK, [])
binary,runtime,output=map(lambda p:Path(p).resolve(),sys.argv[1:]);output.mkdir(parents=True,exist_ok=True)
checks=[]
def check(ok,label):
    checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True)
    (output/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks)},indent=2))
    assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-procedural-xr-') as directory:
    root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
    env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','HOME':directory,'XDG_RUNTIME_DIR':directory,'XDG_CONFIG_HOME':directory+'/config','XDG_STATE_HOME':directory+'/state'}
    helper=binary.parent/'dome-sculpt-procedural-document-test';fixture=root/'source.dsculpt';subprocess.run([str(helper),'--ui-fixture',str(fixture)],check=True,env=env)
    def inspect(path):
        scene=json.loads(subprocess.check_output([str(helper),'--inspect',str(path)],env=env))
        for obj in scene['objects']:
            v=obj['volume'];v['samples'].sort()
            for layer in v.get('sculpt_layers',[]):layer['delta'].sort()
            if 'sculpt_mask' in v:v['sculpt_mask']['values'].sort()
        return scene
    original=inspect(fixture)
    for mode in ('accept','cancel','focus'):
        target=output/(mode+'.dsculpt');report=output/(mode+'.json')
        with (output/(mode+'.log')).open('w') as log:
            result=subprocess.run([str(binary),'--xr','--open',str(fixture),'--frames','36','--save',str(target)],env={**env,'DOME_XR_FIXTURE_REPORT':str(report),'DOME_XR_FIXTURE_PROCEDURAL':mode},stdout=log,stderr=subprocess.STDOUT,timeout=90)
        check(result.returncode==0,'Real-loader procedural '+mode+' closes normally and saves native state')
        after=inspect(target);r=json.loads(report.read_text())
        check(r.get('acquires')==r.get('image_waits')==r.get('releases') and r.get('left_eye_nonblank',0)>0 and r.get('right_eye_nonblank',0)>0 and r.get('palette_images',0)>0 and r.get('tracking_losses',0)==0,'Procedural '+mode+' renders both real GL eyes with balanced image ownership')
        if mode=='accept':
            v=after['objects'][0]['volume'];base=original['objects'][0]['volume']
            check(len(v['sculpt_layers'])==len(base['sculpt_layers'])+1 and v['samples']!=base['samples'],'Actual XR joystick Apply creates real procedural solid and a separate sculpt layer')
            check(v['sculpt_layers'][:-1]==base['sculpt_layers'] and v['voxel_size']==base['voxel_size'],'XR procedural detail retains original layer identities and source sampling')
        else:check(after==original,'Procedural '+mode+' preserves complete original scene exactly')
        if mode=='focus':check(r.get('focus_losses')==1 and r.get('focus_regains')==1,'Actual OpenXR focus loss cancels staged procedural settings')
print(str(len(checks))+' actual-loader procedural checks passed',flush=True)
