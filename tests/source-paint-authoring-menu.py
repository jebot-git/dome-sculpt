#!/usr/bin/env python3
"""Actual FLTK portable-policy/chooser/export/native-recovery workflow."""
import asyncio,json,os,signal,struct,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_SOURCE_AUTHORING_BUS')!='1':
    os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_SOURCE_AUTHORING_BUS':'1'})
root=Path(__file__).resolve().parents[1]
build=root/os.environ.get('DOME_SOURCE_AUTHORING_BUILD','build')
inspector=root/os.environ.get('DOME_SOURCE_AUTHORING_INSPECT',str(build/'dome-sculpt-native-authoring-inspect'))
fixture=root/os.environ.get('DOME_SOURCE_AUTHORING_FIXTURE',str(build/'source-paint-authoring/recovered0.dsculpt'))
out=root/os.environ.get('DOME_SOURCE_AUTHORING_OUT','artifacts/source-paint-authoring-menu');out.mkdir(parents=True,exist_ok=True)
compact=os.environ.get('DOME_SOURCE_AUTHORING_COMPACT')=='1'
checks=[];processes=[]
def check(ok,label):
    checks.append({'passed':bool(ok),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['passed'] for x in checks)},indent=2)+'\n');print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
async def wait(fn,seconds=45):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        value=fn()
        if value:return value
        await asyncio.sleep(.04)
    raise TimeoutError('Native painted-export workflow did not reach expected state')
async def main():
    bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
    with tempfile.TemporaryDirectory(prefix='dome-painted-menu-') as home:
        rd,wr=os.pipe();log=(out/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','640x360x24' if compact else '1280x1000x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log));os.close(wr)
        with os.fdopen(rd) as stream:screen=':'+stream.readline().strip()
        env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
        for key in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(key,None)
        for key,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[key]=home+'/'+suffix;Path(env[key]).mkdir(mode=0o700)
        if compact:
            config=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';config.parent.mkdir(parents=True,exist_ok=True);config.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
        project=Path(home)/'original.dsculpt';project.write_bytes(fixture.read_bytes());original=project.read_bytes();package=Path(home)/'editable.glb';saved=Path(home)/'recovered.dsculpt'
        def inspect(path):return json.loads(subprocess.check_output([str(inspector),str(path)],env=env,text=True))
        baseline=inspect(project);log=(out/'app.log').open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],cwd=home,env=env,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
        def named(title):
            for child in d.screen().root.query_tree().children:
                try:
                    if title in str(child.get_wm_name() or ''):return child
                except error.BadWindow:pass
        window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        def prop(name):
            value=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(value.value).decode() if value else None
        service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));menu=proxy.get_interface('com.canonical.dbusmenu')
        async def nodes():
            result=await menu.call_get_layout(0,-1,[]);values={}
            def walk(node):
                ident,properties,children=node;properties={k:v.value for k,v in properties.items()};values[properties.get('label','').replace('_','')]=(ident,properties)
                for child in children:walk(child.value)
            walk(result[1]);return values
        async def command(label):
            values=await nodes();await menu.call_event(values[label][0],'clicked',Variant('i',0),0);await asyncio.sleep(.2)
        async def ready():
            for _ in range(1000):
                if (await nodes())['Import mesh...'][1].get('enabled',True):return
                await asyncio.sleep(.04)
            raise TimeoutError('Native asset worker did not finish')
        def key(symbol,down=None):
            for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,d.keysym_to_keycode(XK.string_to_keysym(symbol)))
            d.sync()
        def text(value):
            for c in value:
                code=d.keysym_to_keycode(ord(c));shift=d.keycode_to_keysym(code,0)!=ord(c)
                if shift:key('Shift_L',True)
                xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code)
                if shift:key('Shift_L',False)
            d.sync()
        def choose(chooser,target):
            chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Control_L',True);key('a');key('Control_L',False);text(str(target));key('Return')
        def tap(window,x,y):
            g=window.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
        await ready();values=await nodes();label='Export editable project (.glb)...'
        check(values[label][1].get('enabled',True),'Editable project export is an actual enabled DomeD universal-menu command')
        await command(label);options=await wait(lambda:named('Editable GLB export'))
        check(options.get_wm_transient_for().id==window.id and options.get_wm_class()[1]==window.get_wm_class()[1],'Portable policy uses a native FLTK modal with shared document identity')
        values=await nodes();check(not values[label][1].get('enabled',True),'Actual policy modal disables duplicate document export commands')
        options.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return');await wait(lambda:named('Editable GLB export') is None)
        check(not package.exists() and project.read_bytes()==original,'Return on default Cancel submits no export and preserves the project')
        await command(label);await wait(lambda:named('Editable GLB export'));await command('Close Window');await wait(lambda:named('Editable GLB export') is None)
        check(project.read_bytes()==original,'Universal Close cancels the policy modal without a model edit')
        await command(label);options=await wait(lambda:named('Editable GLB export'));g=options.get_geometry();raw=options.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/'options.png')
        row=48 if compact else 36;tap(options,g.width*.65,g.height-row/2-12)
        await wait(lambda:named('Editable GLB export') is None);chooser=await wait(lambda:named('Export editable project'))
        check(chooser.get_wm_transient_for().id==window.id,'Confirmed portable policy opens the actual owned GLB file chooser')
        choose(chooser,package);await wait(lambda:named('Export editable project') is None)
        try:await wait(lambda:package.exists(),90)
        except TimeoutError:
            g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff)
            Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/'export-timeout.png')
            (out/'export-timeout.json').write_text(json.dumps({'window_title':str(window.get_wm_name()),'files':[str(p.relative_to(home)) for p in Path(home).rglob('*') if p.is_file()],'menu':await nodes()},indent=2)+'\n')
            raise
        emitted=package.read_bytes();n=struct.unpack_from('<I',emitted,12)[0];metadata=json.loads(emitted[20:20+n]);native=metadata['extras']['domeSculptAuthoring']
        check(native['scope']=='wholeScene' and native['fallback']['lossless'] is False,'Actual GUI output declares complete native project scope and an approximate portable view')
        binary=emitted[28+n:];sizes=[]
        for image in metadata.get('images',[]):
            view=metadata['bufferViews'][image['bufferView']];offset=view.get('byteOffset',0);blob=binary[offset:offset+view['byteLength']]
            if blob.startswith(b'\x89PNG\r\n\x1a\n'):sizes.append(struct.unpack_from('>II',blob,16))
        check((512,512) in sizes,'Default native policy really emits the selected512-pixel portable paint images')
        check(inspect(package)==baseline and project.read_bytes()==original,'Actual GUI file worker preserves all editable source, paint, metadata and the original project file')
        await command('Open...');chooser=await wait(lambda:named('Open project or editable package'));choose(chooser,package)
        await wait(lambda:named('Open project or editable package') is None);await wait(lambda:'Untitled' in str(window.get_wm_name()) and '*' in str(window.get_wm_name()));await ready()
        check(package.read_bytes()==emitted,'Actual Open restores the exported native project as unsaved and preserves the interchange source')
        await command('Save');chooser=await wait(lambda:named('Save sculpt project'));choose(chooser,saved);await wait(lambda:saved.exists());await wait(lambda:'recovered.dsculpt' in str(window.get_wm_name()) and '*' not in str(window.get_wm_name()))
        check(inspect(saved)==baseline and package.read_bytes()==emitted,'Actual subsequent Save writes the original editable project independently of the portable GLB')
        await command('Close Window');await wait(lambda:app.poll() is not None)
        check(app.returncode==0,'Native export/recovery/Save workflow exits cleanly without sanitizer suppression')
        (out/'editable.glb').write_bytes(emitted);(out/'recovered.dsculpt').write_bytes(saved.read_bytes());d.close()
    bus.disconnect()
try:asyncio.run(main())
finally:
    for process in reversed(processes):
        if process.poll() is None:
            process.terminate()
            try:process.wait(timeout=15)
            except subprocess.TimeoutExpired:process.kill();process.wait()
