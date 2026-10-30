#!/usr/bin/env python3
"""Actual native chooser/import/save/history flow on a private bus and Xvfb."""
import asyncio, hashlib, json, os, signal, struct, subprocess, sys, tempfile, time
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X, XK, display, error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK, [])
if os.environ.get('DOME_IMPORT_TEST_BUS')!='1':
    os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_IMPORT_TEST_BUS':'1'})
ROOT=Path(__file__).resolve().parents[1]
BUILD=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',ROOT/'build'))
MODE=os.environ.get('DOME_IMPORT_FORMAT','glb').lower()
FLAT=os.environ.get('DOME_IMPORT_FLAT')=='1'
OUT=ROOT/('artifacts/menu-import-flat-'+MODE if FLAT else 'artifacts/menu-import-'+MODE if MODE!='glb' else 'artifacts/menu-import-skin' if os.environ.get('DOME_IMPORT_HELPER') else 'artifacts/menu-import-authoring');OUT.mkdir(parents=True,exist_ok=True)
checks=[];processes=[]
def check(value,message):
    checks.append({'passed':bool(value),'check':message});(OUT/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks)},indent=2)+'\n')
    print(('PASS ' if value else 'FAIL ')+message,flush=True);assert value,message
async def wait(operation,seconds=45):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        value=operation()
        if value:return value
        await asyncio.sleep(.04)
    raise TimeoutError('Native import workflow did not reach expected state')
async def main():
    bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
    with tempfile.TemporaryDirectory(prefix='dome-import-menu-') as home:
        read,write=os.pipe();log=(OUT/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1280x1000x24','-nolisten','tcp'],pass_fds=(write,),stdout=log,stderr=log));os.close(write)
        with os.fdopen(read) as stream:screen=':'+stream.readline().strip()
        env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','XMODIFIERS':'@im=none'}
        for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
        for key,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[key]=home+'/'+suffix;Path(env[key]).mkdir(mode=0o700)
        project=Path(home,'native.dsculpt');asset=Path(home,'source.'+MODE);helper=str(BUILD/os.environ.get('DOME_IMPORT_HELPER','dome-sculpt-import-authoring-test'))
        subprocess.run([helper,'--empty-project',str(project)],check=True,env=env)
        if MODE in ('obj','fbx'):asset.write_bytes((ROOT/'tests/fixtures'/('import-twins.'+MODE)).read_bytes())
        elif MODE=='gltf':
            intermediary=Path(home,'fixture.glb');subprocess.run([helper,'--ui-flat-fixture' if FLAT else '--ui-fixture',str(intermediary)],check=True,env=env)
            raw=intermediary.read_bytes();count=struct.unpack_from('<I',raw,12)[0];source=json.loads(raw[20:20+count]);binary=raw[28+count:]
            source['buffers'][0]['uri']='geometry.bin';Path(home,'geometry.bin').write_bytes(binary);asset.write_text(json.dumps(source))
        else:subprocess.run([helper,'--ui-flat-fixture' if FLAT else '--ui-fixture',str(asset)],check=True,env=env)
        original_asset=hashlib.sha256(asset.read_bytes()).digest();log=(OUT/'app.log').open('w');app=subprocess.Popen([str(BUILD/'dome-sculpt'),str(project)],cwd=home,env=env,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
        def named(title):
            for w in d.screen().root.query_tree().children:
                try:
                    if title in str(w.get_wm_name() or ''):return w
                except error.BadWindow:pass
        window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        def mapped_windows():
            result=set()
            for child in d.screen().root.query_tree().children:
                try:
                    if child.get_attributes().map_state==X.IsViewable:result.add(child.id)
                except error.BadWindow:pass
            return result
        def prop(name):
            p=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
        service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));menu=proxy.get_interface('com.canonical.dbusmenu')
        async def layout():
            result=await menu.call_get_layout(0,-1,[]);nodes={}
            def walk(node):
                ident,properties,children=node;properties={k:v.value for k,v in properties.items()};nodes[properties.get('label','').replace('_','')]=(ident,properties)
                for child in children:walk(child.value)
            walk(result[1]);return nodes
        async def command(label):
            nodes=await layout();before=project.stat().st_mtime_ns
            await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
            if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=before)
            await asyncio.sleep(.2)
        async def ready(imported=False):
            for _ in range(1000):
                nodes=await layout()
                if nodes['Import mesh...'][1].get('enabled',True) and (not imported or nodes['Undo'][1].get('enabled',False)):return nodes
                await asyncio.sleep(.04)
            raise TimeoutError('Import worker did not become ready')
        def key(symbol,down=None):
            for event in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,event,d.keysym_to_keycode(XK.string_to_keysym(symbol)))
            d.sync()
        def text(value):
            for c in value:
                sym=ord(c);code=d.keysym_to_keycode(sym);shift=d.keycode_to_keysym(code,0)!=sym
                if shift:key('Shift_L',True)
                xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code)
                if shift:key('Shift_L',False)
            d.sync()
        def inspect():return json.loads(subprocess.check_output([helper,'--inspect',str(project)],env=env))
        await ready();original=inspect();await command('Import mesh...');chooser=await wait(lambda:named('Import model'))
        check(chooser.get_wm_transient_for().id==window.id and chooser.get_wm_class()[1]==window.get_wm_class()[1],'Import uses the real native chooser with DomeD identity and document ownership')
        await command('Close Window');await wait(lambda:named('Import model') is None);check(inspect()==original,'Universal Close cancels native import without touching source or scene')
        await command('Import mesh...');chooser=await wait(lambda:named('Import model'));chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        key('Control_L',True);key('a');key('Control_L',False);text(str(asset));key('Return')
        await wait(lambda:named('Import model') is None);options=await wait(lambda:named('Import options'))
        check(options.get_wm_transient_for().id==window.id and options.get_wm_class()[1]==window.get_wm_class()[1],'Import options use DomeD native modal ownership and appearance identity')
        options.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return')
        await wait(lambda:named('Import options') is None);check(inspect()==original,'Return on the default Cancel discards native import options without submitting asset work')
        await command('Import mesh...');chooser=await wait(lambda:named('Import model'));chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        key('Control_L',True);key('a');key('Control_L',False);text(str(asset));key('Return')
        options=await wait(lambda:named('Import options'));options.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        g=options.get_geometry();raw=options.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(OUT/'options.png')
        if MODE in ('obj','fbx'):
            key('Shift_L',True)
            for _ in range(5):key('Tab')
            key('Shift_L',False);before_popup=mapped_windows();key('space')
            popup=await wait(lambda:mapped_windows()-before_popup)
            key('Home')
            for _ in range(3):key('Down')
            key('Return');await wait(lambda:not (mapped_windows()&popup))
        else:key('Tab')
        g=options.get_geometry();raw=options.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(OUT/'options-after-keys.png')
        if MODE in ('obj','fbx'):
            # Fixture font is the isolated default16px: click the actual Import
            # button after observing the native popup unmap, independent of
            # remaining focus traversal events in the native toolkit queue.
            g=options.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+g.width//2+80,y=g.y+g.height-46)
            xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
        else:key('Return')
        await wait(lambda:named('Import options') is None)
        nodes=await ready(True);check(nodes['Undo'][1].get('enabled',False),'Explicit native options Import submits the source-retaining model on a background worker as one history operation')
        await command('Save');accepted=inspect();check(len(accepted['objects'])==3 and len(accepted['groups'])==int(os.environ.get('DOME_IMPORT_GROUPS','5' if MODE=='fbx' else '4')) and sum(o['mesh_active'] for o in accepted['objects'])==2,'Actual native import and Save preserve all objects and editable node hierarchy')
        check(accepted['version']==int(os.environ.get('DOME_IMPORT_VERSION','11' if MODE!='glb' else '9')) and accepted['source_models'] and accepted['source_materials']['bindings'],'Native Save includes canonical source, frame, image, material and deformation archives')
        if FLAT:
            raw=bytes(accepted['source_models'][0]['original_glb']['bytes']);count=struct.unpack_from('<I',raw,12)[0];source=json.loads(raw[20:20+count])
            frames=accepted['source_materials']['normal_frames']['frames']
            check('NORMAL' not in source['meshes'][0]['primitives'][0]['attributes'] and frames and all(f['generated'] and f['generated_uv']==0 for f in frames),'Real native flat-normal import retains raw absence and saves generated original-frame Mikk authority')
            if MODE=='glb':check(raw==asset.read_bytes(),'Native flat import saves the exact original GLB without inserting generated attributes into it')
        if MODE!='glb':
            receipt=accepted['source_models'][0]['original_source']
            check(receipt['format']=={'gltf':2,'obj':3,'fbx':4}[MODE] and bytes(receipt['root']['bytes']['bytes'])==asset.read_bytes(),'Native Save keeps original format bytes separately from canonical GLB')
            check(receipt['options']['units']==(3 if MODE in ('obj','fbx') else 0),'Real native unit choice reaches the background importer and persisted receipt')
        geometry=window.get_geometry();raw=window.get_image(0,0,geometry.width,geometry.height,X.ZPixmap,0xFFFFFFFF);Image.frombytes('RGB',(geometry.width,geometry.height),raw.data,'raw','BGRX').save(OUT/'imported.png')
        await command('Undo');await ready();await command('Save');check(inspect()==original,'One universal Undo restores exact pre-import scene with no partial groups')
        await command('Redo');await ready();await command('Save');check(inspect()==accepted,'One universal Redo restores exact imported editable source state')
        check(hashlib.sha256(asset.read_bytes()).digest()==original_asset and app.poll() is None,'The original input file remains byte-exact and the native application remains responsive')
        d.close()
    bus.disconnect()
try:asyncio.run(main())
finally:
    for p in reversed(processes):
        if p.poll() is None:p.terminate()
