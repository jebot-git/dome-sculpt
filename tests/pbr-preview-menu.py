#!/usr/bin/env python3
"""Actual native authored/core-PBR universal menu and source preservation."""
import asyncio, json, os, signal, subprocess, sys, tempfile, time
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X, XK, display, error
from Xlib.ext import xtest
from PIL import Image

signal.pthread_sigmask(signal.SIG_SETMASK, [])
if os.environ.get('DOME_PBR_MENU_BUS') != '1':
    os.execvpe('dbus-run-session', ['dbus-run-session', '--', sys.executable, *sys.argv],
              {**os.environ, 'DOME_PBR_MENU_BUS':'1'})
root=Path(__file__).resolve().parents[1]
build=root/os.environ.get('DOME_PBR_BUILD','build-preview')
output=root/os.environ.get('DOME_PBR_OUT','artifacts/pbr-preview-menu')
output.mkdir(parents=True,exist_ok=True)
helper=build/'dome-sculpt-pbr-preview-test'
checks=[]; processes=[];compact=os.environ.get('DOME_PBR_COMPACT')=='1'
def check(ok,label):
    checks.append({'passed':bool(ok),'check':label})
    (output/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks)},indent=2)+'\n')
    print(('PASS ' if ok else 'FAIL ')+label,flush=True)
    assert ok,label
async def wait(fn,seconds=45):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        value=fn()
        if value:return value
        await asyncio.sleep(.04)
    raise TimeoutError('Native normal-detail workflow did not become ready')
async def main():
    bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
    with tempfile.TemporaryDirectory(prefix='dome-normal-menu-') as home:
        r,w=os.pipe();log=(output/'xvfb.log').open('w')
        processes.append(subprocess.Popen(['Xvfb','-displayfd',str(w),'-screen','0','640x360x24' if compact else '1280x1000x24','-nolisten','tcp'],pass_fds=(w,),stdout=log,stderr=log));os.close(w)
        with os.fdopen(r) as stream:screen=':'+stream.readline().strip()
        env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
        for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
        for key,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:
            env[key]=home+'/'+suffix;Path(env[key]).mkdir(mode=0o700)
        if compact:
            config=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';config.parent.mkdir(parents=True,exist_ok=True);config.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
        fixture=Path(home)/'normal.dsculpt'
        subprocess.run([str(helper),'--fixture',str(fixture)],env=env,check=True)
        def inspect():return json.loads(subprocess.check_output([str(helper),'--inspect',str(fixture)],env=env,text=True))
        baseline=inspect();connection=display.Display(screen)
        def named(title):
            for window in connection.screen().root.query_tree().children:
                try:
                    if title in str(window.get_wm_name() or ''):return window
                except error.BadWindow:pass
        def tap(window,x,y):
            g=window.get_geometry();xtest.fake_input(connection,X.MotionNotify,x=g.x+int(x),y=g.y+int(y))
            xtest.fake_input(connection,X.ButtonPress,1);xtest.fake_input(connection,X.ButtonRelease,1);connection.sync()
        def key(name,down=None):
            for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):
                xtest.fake_input(connection,kind,connection.keysym_to_keycode(XK.string_to_keysym(name)));connection.sync()
        def capture(window,name):
            g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff)
            image=Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX');image.save(output/name);return image
        async def launch(suffix):
            log=(output/('app-'+suffix+'.log')).open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(fixture)],cwd=home,env=env,stdout=log,stderr=log);processes.append(app)
            window=await wait(lambda:named('Dome Sculpt'),90);window.set_input_focus(X.RevertToParent,X.CurrentTime);connection.sync()
            def property(name):
                p=window.get_full_property(connection.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
            service=await wait(lambda:property('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:property('_KDE_NET_WM_APPMENU_OBJECT_PATH'))
            proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));return app,window,proxy.get_interface('com.canonical.dbusmenu')
        app,window,menu=await launch('initial')
        async def nodes():
            result=await menu.call_get_layout(0,-1,[]);values={}
            def walk(node):
                ident,props,children=node;props={k:v.value for k,v in props.items()};values[props.get('label','').replace('_','')]=(ident,props)
                for child in children:walk(child.value)
            walk(result[1]);return values
        async def command(label):
            values=await nodes();await menu.call_event(values[label][0],'clicked',Variant('i',0),0);await asyncio.sleep(.2)
        async def save():
            for _ in range(1000):
                if (await nodes())['Save'][1].get('enabled',True):break
                await asyncio.sleep(.02)
            else:raise TimeoutError('Keep-detail Apply never enabled Save')
            await command('Save')
        async def editor():
            await command('UV editor...');value=await wait(lambda:named('UV editor'));value.set_input_focus(X.RevertToParent,X.CurrentTime);connection.sync();await asyncio.sleep(.25);return value
        modes=('Authored material','PBR','Toon preview')
        async def checked(label):
            return (await nodes())[label][1].get('toggle-state')==1
        await wait(lambda:window.get_geometry().width>300)
        await asyncio.sleep(.6)
        initial=capture(window,'authored.png')
        check(await checked('Authored material') and not await checked('PBR'),'Universal menu starts in authored mode without rewriting the saved material')
        values=await nodes()
        check(all(values[label][1].get('enabled',True) for label in modes),'All three actual shared material preview commands are available')
        await command('PBR')
        viewport=(initial.width//4,initial.height//4,initial.width*3//4,initial.height*3//4)
        async def changed_pixels():
            for _ in range(250):
                value=capture(window,'pbr.png')
                if initial.crop(viewport).tobytes()!=value.crop(viewport).tobytes():return value
                await asyncio.sleep(.04)
            return value
        pbr=await changed_pixels()
        check(await checked('PBR') and not await checked('Authored material'),'Native PBR command publishes the actual mutually exclusive core-PBR mode')
        viewport=(initial.width//4,initial.height//4,initial.width*3//4,initial.height*3//4)
        check(initial.size==pbr.size and initial.crop(viewport).tobytes()!=pbr.crop(viewport).tobytes(),'Actual native viewport pixels change with the PBR material policy')
        await save();check(inspect()==baseline,'Saving from core-PBR preview retains exact source frames, paint, original material and scene history')
        await command('Authored material');await asyncio.sleep(.5);restored=capture(window,'restored.png')
        check(await checked('Authored material') and not await checked('PBR'),'Native authored command restores the original checked material policy')
        check(restored.crop(viewport).tobytes()==initial.crop(viewport).tobytes(),'Actual viewport restores the exact authored pixels independently of status text')
        await command('Toon preview');check(await checked('Toon preview') and not await checked('PBR'),'Native toon preview remains a separate shared command')
        await command('Authored material')
        edit=await editor();values=await nodes()
        check(all(not values[label][1].get('enabled',True) for label in modes),'Actual UV modal owns editing and disables all material preview commands')
        key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:named('UV editor') is None)
        values=await nodes()
        check(all(values[label][1].get('enabled',True) for label in modes),'DomeD Ctrl+W cancellation restores preview command availability')
        await save();check(inspect()==baseline,'Preview changes and modal cancellation add no source or material mutation')
        app.terminate();app.wait(timeout=15);await wait(lambda:named('Dome Sculpt') is None)
        app,window,menu=await launch('restart');await asyncio.sleep(.5)
        check(await checked('Authored material') and not await checked('PBR'),'Project restart uses the authored default without serializing the ephemeral preview')
        check(app.poll() is None,'Real FLTK preview workflow leaves the application and desktop session running')
        connection.close()
    bus.disconnect()
try:asyncio.run(main())
finally:
    for process in reversed(processes):
        if process.poll() is None:
            process.terminate()
            try:process.wait(timeout=10)
            except subprocess.TimeoutExpired:process.kill();process.wait()
