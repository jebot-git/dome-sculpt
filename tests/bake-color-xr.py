#!/usr/bin/env python3
"""Actual loader/stereo GL/controller color preparation workflow; synthetic devices."""
import json, os, signal, subprocess, sys, tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK, [])
binary,runtime,output=map(lambda p:Path(p).resolve(),sys.argv[1:]);output.mkdir(parents=True,exist_ok=True)
checks=[]
def check(ok,label):
    checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True)
    (output/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks)},indent=2))
    assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-bake-color-xr-') as directory:
    root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
    env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','HOME':directory,'XDG_RUNTIME_DIR':directory,'XDG_CONFIG_HOME':directory+'/config','XDG_STATE_HOME':directory+'/state'}
    helper=binary.parent/'dome-sculpt-colored-bake-workflow-test';fixture=root/'source.dsculpt';subprocess.run([str(helper),'--ui-fixture',str(fixture)],check=True,env=env)
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
            result=subprocess.run([str(binary),'--xr','--open',str(fixture),'--frames','36','--save',str(target)],env={**env,'DOME_XR_FIXTURE_REPORT':str(report),'DOME_XR_FIXTURE_BAKE_COLOR':mode},stdout=log,stderr=subprocess.STDOUT,timeout=90)
        check(result.returncode==0,'Real-loader color policy '+mode+' closes normally and saves native state')
        after=inspect(target);r=json.loads(report.read_text())
        check(r.get('acquires')==r.get('image_waits')==r.get('releases') and r.get('left_eye_nonblank',0)>0 and r.get('right_eye_nonblank',0)>0 and r.get('palette_images',0)>0 and r.get('tracking_losses',0)==0,'Color policy '+mode+' renders both real GL eyes with balanced image ownership')
        if mode=='accept':
            paint=after['objects'][0]['painted']
            check(paint['baked']['color_interpretation']=='resolved-pbr' and bool(paint['baked']['normal']['bytes']) and bool(paint['baked']['pbr_color']['bytes']),'Actual joystick color confirmation and Bake create resolved PBR/common color plus a real normal payload')
            common=lambda layers:[{k:v for k,v in layer.items() if k!='pbr_color_tiles'} for layer in layers]
            check(after['isolated_objects'][1:]==original['isolated_objects'][1:] and common(paint['layers'])==common(original['objects'][0]['painted']['layers']),'Real-loader color preparation preserves independently archived sources and every common editable layer')
        else:check(after==original,'Color policy '+mode+' preserves complete original scene exactly')
        if mode=='focus':check(r.get('focus_losses')==1 and r.get('focus_regains')==1,'Actual OpenXR focus loss cancels staged color policy settings')
print(str(len(checks))+' actual-loader color policy checks passed',flush=True)
