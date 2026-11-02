#!/usr/bin/env python3
"""Actual native Keep-normal-detail control, universal history and restart."""
import asyncio, json, os, signal, subprocess, sys, tempfile, time
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X, XK, display, error
from Xlib.ext import xtest
from PIL import Image

signal.pthread_sigmask(signal.SIG_SETMASK, [])
if os.environ.get('DOME_NORMAL_MENU_BUS') != '1':
    os.execvpe('dbus-run-session', ['dbus-run-session', '--', sys.executable, *sys.argv],
              {**os.environ, 'DOME_NORMAL_MENU_BUS':'1'})
root=Path(__file__).resolve().parents[1]
build=root/os.environ.get('DOME_NORMAL_BUILD','build-ui')
output=root/os.environ.get('DOME_NORMAL_OUT','artifacts/normal-domain-menu')
output.mkdir(parents=True,exist_ok=True)
helper=build/'dome-sculpt-normal-domain-document-test'
checks=[]; processes=[]
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
        processes.append(subprocess.Popen(['Xvfb','-displayfd',str(w),'-screen','0','1280x1000x24','-nolisten','tcp'],pass_fds=(w,),stdout=log,stderr=log));os.close(w)
        with os.fdopen(r) as stream:screen=':'+stream.readline().strip()
        env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
        for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
        for key,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:
            env[key]=home+'/'+suffix;Path(env[key]).mkdir(mode=0o700)
        fixture=Path(home)/'normal.dsculpt'
        subprocess.run([str(helper),'--ui-fixture',str(fixture)],env=env,check=True)
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
        edit=await editor();g=edit.get_geometry();initial=capture(edit,'default.png')
        tap(edit,g.width/2,80);await asyncio.sleep(.3);selected=capture(edit,'selected.png')
        check(initial.tobytes()!=selected.tobytes(),'Actual native Keep normal detail toggle changes its visible state and actionable status')
        check(edit.get_wm_transient_for().id==window.id,'Keep-detail controls retain native modal parent and universal application identity')
        key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:named('UV editor') is None);await save()
        check(inspect()==baseline,'DomeD Ctrl+W cancels the choice and preserves all original normal, paint, pose and key sources')
        edit=await editor();g=edit.get_geometry();tap(edit,g.width/2,80);tap(edit,g.width/12,16);await asyncio.sleep(.3)
        tap(edit,g.width/2+80,g.height-16);await wait(lambda:named('UV editor') is None);await save();accepted=inspect()
        check(accepted['version']==21 and accepted['source_materials']['version']==7 and bool(accepted['normal_sampling']['domains']),'Actual native toggle plus Scale and Apply saves the owning retained sampling graph')
        check(accepted['source_materials']['normal_frames']==baseline['source_materials']['normal_frames'],'Native retained normal UV changes preserve every original and current key-frame attribute')
        await command('Undo');await save();check(inspect()==baseline,'Universal Undo restores the exact complete pre-conversion native project')
        await command('Redo');await save();check(inspect()==accepted,'Universal Redo restores exact retained-domain paint, source and detail ownership')
        app.terminate();app.wait(timeout=15);await wait(lambda:named('Dome Sculpt') is None)
        app,window,menu=await launch('restart');edit=await editor();g=edit.get_geometry();before=capture(edit,'restart.png')
        tap(edit,g.width/2,80);await asyncio.sleep(.2);after=capture(edit,'required.png')
        check(before.tobytes()==after.tobytes(),'Restarted retained domain keeps its required toggle visibly selected and disabled')
        key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:named('UV editor') is None);await save()
        check(inspect()==accepted,'Reopening and cancelling required retained mode preserves every saved owner exactly')
        check(app.poll() is None,'Native normal-detail workflow leaves the application and desktop session running')
        connection.close()
    bus.disconnect()
try:asyncio.run(main())
finally:
    for process in reversed(processes):
        if process.poll() is None:
            process.terminate()
            try:process.wait(timeout=10)
            except subprocess.TimeoutExpired:process.kill();process.wait()
