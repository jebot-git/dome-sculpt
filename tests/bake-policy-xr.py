#!/usr/bin/env python3
"""Actual loader/stereo GL/controller projection policy workflow; synthetic devices."""
import json, os, signal, subprocess, sys, tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK, [])
binary,runtime,output=map(lambda p:Path(p).resolve(),sys.argv[1:]);output.mkdir(parents=True,exist_ok=True)
checks=[]
def check(ok,label):
    checks.append({'passed':bool(ok),'check':label});print(('PASS ' if ok else 'FAIL ')+label,flush=True)
    (output/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks)},indent=2))
    assert ok,label
with tempfile.TemporaryDirectory(prefix='dome-bake-policy-xr-') as directory:
    root=Path(directory);manifest=root/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
    env={**os.environ,'XR_RUNTIME_JSON':str(manifest),'LIBGL_ALWAYS_SOFTWARE':'1','HOME':directory,'XDG_RUNTIME_DIR':directory,'XDG_CONFIG_HOME':directory+'/config','XDG_STATE_HOME':directory+'/state'}
    helper=binary.parent/'dome-sculpt-bake-projection-policy-test';fixture=root/'source.dsculpt';subprocess.run([str(helper),'--ui-fixture',str(fixture)],check=True,env=env)
    def inspect(path):
        scene=json.loads(subprocess.check_output([str(helper),'--inspect',str(path)],env=env))
        for obj in scene['objects']:
            v=obj['volume'];v['samples'].sort()
            for layer in v.get('sculpt_layers',[]):layer['delta'].sort()
            if 'sculpt_mask' in v:v['sculpt_mask']['values'].sort()
        return scene
    topology_fixture=root/'matching-source.dsculpt'
    subprocess.run([str(helper),'--ui-topology-fixture',str(topology_fixture)],check=True,env=env)
    for mode in ('accept','cancel','focus','topology'):
        source=topology_fixture if mode=='topology' else fixture
        original=inspect(source)
        target=output/(mode+'.dsculpt');report=output/(mode+'.json')
        with (output/(mode+'.log')).open('w') as log:
            result=subprocess.run([str(binary),'--xr','--open',str(source),'--frames','44','--save',str(target)],env={**env,'DOME_XR_FIXTURE_REPORT':str(report),'DOME_XR_FIXTURE_BAKE_POLICY':mode},stdout=log,stderr=subprocess.STDOUT,timeout=90)
        check(result.returncode==0,'Real-loader projection policy '+mode+' closes normally and saves native state')
        after=inspect(target);r=json.loads(report.read_text())
        check(r.get('acquires')==r.get('image_waits')==r.get('releases') and r.get('left_eye_nonblank',0)>0 and r.get('right_eye_nonblank',0)>0 and r.get('palette_images',0)>0 and r.get('tracking_losses',0)==0,'Projection policy '+mode+' renders both real GL eyes with balanced image ownership')
        if mode=='accept':
            old=original['objects'][0]['painted']['baked'];new=after['objects'][0]['painted']['baked']
            right=all(new[channel]['bytes'][4*(y*64+x):4*(y*64+x)+4]==old[channel]['bytes'][4*(y*64+x):4*(y*64+x)+4] for channel in ('color','orm','emission','normal') for y in range(64) for x in range(32,64))
            left=all(new['color']['bytes'][4*(y*64+x)]==0 and new['color']['bytes'][4*(y*64+x)+2]==255 for y in range(64) for x in range(32))
            recipe=after['objects'][0]['bake_recipe']
            check(recipe['source']['object']==after['objects'][1]['id'] and recipe['settings']['allow_partial'],'Actual controller explicitly selects the live source rather than rebinding its unavailable historical recipe')
            check(right and left,'Actual joystick policy confirmation and Bake update hits while preserving every missed channel')
            check(after['objects'][1:]==original['objects'][1:] and after['objects'][0]['painted']['layers']==original['objects'][0]['painted']['layers'],'Real-loader partial bake retains exact sources and editable layers')
        elif mode=='topology':
            part=after['objects'][0]
            recipe=part['bake_recipe']
            colors=part['painted']['baked']['color']['bytes']
            check(recipe['version']==2 and recipe['settings']['corresponding_topology'] and recipe['source']['object']==after['objects'][1]['id'],
                  'The ordered save records the real controller-selected topology recipe and live source')
            check(all(colors[i:i+4]==[0,0,255,255] for i in range(0,len(colors),4)) and after['objects'][1:]==original['objects'][1:] and part['painted']['layers']==original['objects'][0]['painted']['layers'],
                  'Matching topology paints all source corners and preserves original source and target layers')
        else:check(after==original,'Projection policy '+mode+' preserves complete original scene exactly')
        if mode=='focus':check(r.get('focus_losses')==1 and r.get('focus_regains')==1,'Actual OpenXR focus loss cancels staged projection policy settings')
print(str(len(checks))+' actual-loader projection policy checks passed',flush=True)
