#!/usr/bin/env python3
"""Actual native chooser/import/save/history flow on a private bus and Xvfb."""
import asyncio, hashlib, json, os, signal, subprocess, sys, tempfile, time
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
OUT=ROOT/('artifacts/menu-import-skin' if os.environ.get('DOME_IMPORT_HELPER') else 'artifacts/menu-import-authoring');OUT.mkdir(parents=True,exist_ok=True)
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
        project=Path(home,'native.dsculpt');asset=Path(home,'source.glb');helper=str(BUILD/os.environ.get('DOME_IMPORT_HELPER','dome-sculpt-import-authoring-test'))
        subprocess.run([helper,'--empty-project',str(project)],check=True,env=env);subprocess.run([helper,'--ui-fixture',str(asset)],check=True,env=env)
        original_asset=hashlib.sha256(asset.read_bytes()).digest();log=(OUT/'app.log').open('w');app=subprocess.Popen([str(BUILD/'dome-sculpt'),str(project)],cwd=home,env=env,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
        def named(title):
            for w in d.screen().root.query_tree().children:
                try:
                    if title in str(w.get_wm_name() or ''):return w
                except error.BadWindow:pass
        window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
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
        async def ready():
            for _ in range(1000):
                nodes=await layout()
                if nodes['Import mesh...'][1].get('enabled',True):return nodes
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
        await ready();original=inspect();await command('Import mesh...');chooser=await wait(lambda:named('Import static mesh'))
        check(chooser.get_wm_transient_for().id==window.id and chooser.get_wm_class()[1]==window.get_wm_class()[1],'Import uses the real native chooser with DomeD identity and document ownership')
        await command('Close Window');await wait(lambda:named('Import static mesh') is None);check(inspect()==original,'Universal Close cancels native import without touching source or scene')
        await command('Import mesh...');chooser=await wait(lambda:named('Import static mesh'));chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        key('Control_L',True);key('a');key('Control_L',False);text(str(asset));key('Return')
        await wait(lambda:named('Import static mesh') is None);nodes=await ready();check(nodes['Undo'][1].get('enabled',False),'Native chooser submits preserved GLB on background worker as one history operation')
        await command('Save');accepted=inspect();check(len(accepted['objects'])==3 and len(accepted['groups'])==int(os.environ.get('DOME_IMPORT_GROUPS','4')) and sum(o['mesh_active'] for o in accepted['objects'])==2,'Actual native import and Save preserve both instances and editable node hierarchy')
        check(accepted['version']==int(os.environ.get('DOME_IMPORT_VERSION','9')) and accepted['source_models'] and accepted['source_materials']['bindings'],'Native Save includes original GLB, frame, image, material and deformation archives')
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
