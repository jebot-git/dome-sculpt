#!/usr/bin/env python3
"""Focused existing real-loader retopo controls on a genuine affine fixture.

The runtime uses synthetic controller tracking and hardware stereo rendering.
"""
import hashlib,json,signal,struct,subprocess,sys
from pathlib import Path
from xr_test_graphics import hardware_environment, hardware_renderer
signal.pthread_sigmask(signal.SIG_SETMASK, [])
binary,runtime,inspector,fixture,out=map(lambda p:Path(p).resolve(),sys.argv[1:])
out.mkdir(parents=True,exist_ok=True)
checks=[]
def check(ok,message):
    checks.append({'passed':bool(ok),'check':message})
    (out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['passed'] for x in checks)},indent=2)+'\n')
    print(('PASS ' if ok else 'FAIL ')+message,flush=True)
    assert ok,message
def scene_payload(value):
    return value.get('payload', value)
def scene(path):
    value=json.loads(subprocess.check_output([str(inspector),str(path)],text=True))
    for obj in scene_payload(value)['objects']:
        volume=obj['volume'];volume['samples'].sort()
        for layer in volume.get('sculpt_layers',[]):layer['delta'].sort()
        if 'sculpt_mask' in volume:volume['sculpt_mask']['values'].sort()
    return value
manifest=out/'runtime.json';manifest.write_text(json.dumps({'file_format_version':'1.0.0','runtime':{'library_path':str(runtime)}}))
home=out/'home';home.mkdir(exist_ok=True)
env={**hardware_environment(),'XR_RUNTIME_JSON':str(manifest),'LP_NUM_THREADS':'2',
     'HOME':str(home),'XDG_RUNTIME_DIR':str(home),'XDG_CONFIG_HOME':str(home/'config'),'XDG_STATE_HOME':str(home/'state')}
for key in ('DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
original_hash=hashlib.sha256(fixture.read_bytes()).hexdigest()
before=scene(fixture)
def run(mode,frames):
    path=out/(mode+'.dsculpt');report=out/(mode+'.json')
    path.unlink(missing_ok=True);report.unlink(missing_ok=True)
    e={**env,'DOME_XR_FIXTURE_RETOPO':mode,'DOME_XR_FIXTURE_REPORT':str(report),
       'DOME_XR_FIXTURE_RETOPO_EXIT_AFTER_RESUME':'1'}
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
    detail=json.loads(report.read_text())
    check(hardware_renderer(detail),'Actual '+mode+' uses hardware OpenGL')
    return scene(path),detail
# Allow the three component edits, their trigger releases, and Apply to finish
# before saving the committed comparison scene.
committed,_=run('committed',22)
source=scene_payload(committed)['objects'][0]
check(source['material']['color'][2]>.7 and source['material']['color'][0]<.1,
      'Real controller commits its material change before the retopo draft')
expected=dict(scene_payload(before)['objects'][0]);actual=dict(source);expected.pop('material');actual.pop('material')
check(expected==actual,'Committed controller prefix retains every affine, posed, painted and sculpt source channel')
for mode in ('accept','focus','unconfirmed'):
    # Wait for actual menu readiness on cold device compilation, then stop
    # through the runtime once the final action and its release are observed.
    after,report=run(mode,1800)
    check(report.get('retopo_source_ready') is True and report.get('retopo_script_step',0)>=72 and report.get('tracking_losses',0)==0,
          mode+' reaches actual menu Resume without a tracking-loss shortcut')
    check(report['frames_waited']==report['frames_begun']==report['frames_ended'] and
          report['acquires']==report['image_waits']==report['releases'] and report['swapchains_destroyed']==2,
          mode+' balances actual frame/stereo-image/shutdown ownership')
    if mode=='unconfirmed':
        check(after==committed,'Unconfirmed standalone Save cancels the draft and preserves the exact committed prefix')
        check(report.get('retopo_pressure_samples',0)>=3,'Unconfirmed case contains actual pressure placement')
        continue
    objects=scene_payload(after)['objects']
    check(report.get('retopo_face_ready') is True and report.get('retopo_apply_ready') is True and len(objects)==2 and
          not objects[0]['visible'] and objects[1]['visible'],
          mode+' uses genuine Face/Apply and retains the hidden affine source')
    retained=dict(objects[0]);expected=dict(source)
    retained.pop('visible');expected.pop('visible')
    check(retained==expected,mode+' preserves full original paint, pose, morphs, affine placement and source identities')
    guide_paint=source['painted'].get('payload',source['painted'])
    replacement=objects[1]['painted']
    replacement=replacement.get('payload',replacement)
    def metadata(layers):
        return [{key:layer[key] for key in ('id','name','visible','opacity')} for layer in layers]
    check(replacement['resolution']==guide_paint['resolution'] and replacement['active']==guide_paint['active'] and
          metadata(replacement['layers'])==metadata(guide_paint['layers']),
          mode+' keeps the writable layers on the new native patch')
    base = guide_paint.get('baked')
    bindings=scene_payload(after).get('source_materials',{}).get('bindings',[])
    binding_id=source.get('material_bindings')
    typed_guide=binding_id is not None and bindings[binding_id].get('material')
    if base and not typed_guide:
        check(bool(replacement['layers'][0]['physical_emission_tiles']) and bool(replacement['layers'][1]['tiles']),
              mode+' preserves FLOAT paint and its hidden mask')
        baked = replacement.get('baked', {})
        expected_prepared = base.get('normal_interpretation') == 'independent-baked-field-v1'
        normal_preserved = (
            baked.get('base_version') == 5 and
            baked.get('normal_interpretation') == 'independent-baked-field-v1' and
            'normal_field' in baked
        ) if expected_prepared else bool(baked.get('normal', {}).get('bytes'))
        check(baked.get('version') == 6 and bool(baked.get('float_emission', {}).get('bytes')) and
              normal_preserved and 255 in baked.get('coverage', {}).get('bytes', []),
              mode+' publishes the separate FLOAT Bake and covered normal map')
    else:
        old_binding, new_binding = [bindings[obj['material_bindings']] for obj in objects]
        check(old_binding['material'] and new_binding['material'] and bool(replacement.get('baked'))==bool(base) and
              all(old['image'] != new['image'] and new['image'] is not None and new['wrap'] == [33071,33071]
                  for old,new in zip(old_binding['roles'][:4],new_binding['roles'][:4])),
              mode+' captures the original MToon color, shade, normal and emission as independent patch maps')
        painted=any(layer['visible'] and layer['tiles'] for layer in guide_paint['layers'])
        if painted:
            check(all(layer['tiles'] for layer in replacement['layers']),
                  mode+' keeps the visible and hidden paint on the standalone MToon patch')
        physical=any(layer.get('physical_emission_tiles') for layer in guide_paint['layers'])
        if physical:
            check(replacement['layers'][0]['physical_emission_tiles'],
                  mode+' retains the authored FLOAT paint layer')
        if base:
            baked=replacement['baked']
            coverage=baked['coverage']['bytes']
            floating=bool(base.get('float_emission'))
            emission=baked.get('float_emission') if floating else baked.get('emission')
            check(baked['normal']['bytes'] and emission['bytes'] and
                  any(0<value<255 for value in coverage) and max(coverage)>128,
                  mode+' retains partially covered native normal and emission Bake maps')
        if mode=='accept':
            glb=out/'mtoon-portable.glb'
            subprocess.run([str(binary),'--export-project',str(out/'accept.dsculpt'),str(glb)],
                           env=env,check=True,timeout=60)
            payload=glb.read_bytes()
            length=struct.unpack_from('<I',payload,12)[0]
            exported=json.loads(payload[20:20+length])
            primitive=exported['meshes'][0]['primitives'][0]
            variant=primitive['extensions']['KHR_materials_variants']['mappings'][0]['material']
            material=exported['materials'][variant]
            check('TANGENT' in primitive['attributes'] and 'normalTexture' in material and
                  'emissiveTexture' in material and 'baseColorTexture' in material['pbrMetallicRoughness'] and
                  'shadeMultiplyTexture' in material['extensions']['VRMC_materials_mtoon'],
                  'Standard GLB includes the authored MToon variant and its four sampled roles')
            if painted:
                bounds=material['extras']['domeSculptMToonPaint']
                emission_limit=.008*bounds['emissiveStrength']
                check(bounds['errorDomain']=='mesh-uv-triangles' and
                      all(bounds[key]<=.008 for key in ('maximumLinearColorError','maximumLinearShadeError')) and
                      bounds['maximumLinearEmissionError']<=emission_limit,
                      'Standard GLB certifies color, shared shade and emission from the visible paint layers')
                core=exported['materials'][primitive['material']]
                if base:
                    check(0<bounds['maximumNormalErrorRadians']<.02 and
                          core['normalTexture']['index']!=material['normalTexture']['index'],
                          'Standard GLB certifies the independent covered MToon normal Bake')
                if physical:
                    proof=material['extras']['domeSculptPortableEmission']
                    check(proof['evaluation']=='finite-current-authored-mtoon-retopology' and
                          0<proof['betweenTexelErrorBound']<=emission_limit and
                          core['emissiveTexture']['index']!=material['emissiveTexture']['index'],
                          'Standard GLB keeps separate Core and authored MToon HDR textures')
                if 'mixed-float' in fixture.name:
                    transform=material['emissiveTexture']['extensions']['KHR_texture_transform']
                    check(transform['offset']==[.5/256,.5/256],
                          'Standard GLB aligns the emitted HDR texture to both source and paint centers')
                if 'mixed-float-normal' in fixture.name:
                    transform=material['normalTexture']['extensions']['KHR_texture_transform']
                    check(transform['offset']==[.5/256,.5/256],
                          'Standard GLB aligns the covered normal to both source and Bake centers')
    patch=objects[1]['retopology'];topology=json.loads(patch['topology'])
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
