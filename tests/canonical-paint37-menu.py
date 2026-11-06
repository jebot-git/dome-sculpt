#!/usr/bin/env python3
"""Actual canonical37 float-grid UV control, history and restart; no physical session."""
import asyncio, json, os, signal, subprocess, sys, tempfile, time
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X, XK, display, error
from Xlib.ext import xtest
from PIL import Image

signal.pthread_sigmask(signal.SIG_SETMASK, [])
if os.environ.get('DOME_PAINT_MENU_BUS') != '1':
    os.execvpe('dbus-run-session', ['dbus-run-session', '--', sys.executable, *sys.argv],
              {**os.environ, 'DOME_PAINT_MENU_BUS':'1'})
root=Path(__file__).resolve().parents[1]
build=Path(os.environ['DOME_PAINT_BUILD'])
output=root/os.environ.get('DOME_PAINT_OUT','artifacts/paint-sampling-menu')
output.mkdir(parents=True,exist_ok=True)
helper=build/'dome-sculpt-native-authoring-inspect'
fixture_source=Path(os.environ['DOME_NATIVE32_FIXTURE'])
checks=[]; processes=[]
compact=os.environ.get('DOME_NATIVE32_COMPACT')=='1'
row=46 if compact else 32
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
    raise TimeoutError('Native retained-paint workflow did not become ready')
async def main():
    bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
    with tempfile.TemporaryDirectory(prefix='dome-paint-menu-') as home:
        r,w=os.pipe();log=(output/'xvfb.log').open('w')
        processes.append(subprocess.Popen(['Xvfb','-displayfd',str(w),'-screen','0',os.environ.get('DOME_PAINT_SCREEN','1280x1000')+'x24','-nolisten','tcp'],pass_fds=(w,),stdout=log,stderr=log));os.close(w)
        with os.fdopen(r) as stream:screen=':'+stream.readline().strip()
        env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
        for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
        for key,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:
            env[key]=home+'/'+suffix;Path(env[key]).mkdir(mode=0o700)
        if os.environ.get('DOME_NATIVE32_COMPACT')=='1':
            config=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';config.parent.mkdir(parents=True,exist_ok=True);config.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
        fixture=Path(home)/'paint.dsculpt';fixture.write_bytes(fixture_source.read_bytes())
        def inspect():return json.loads(subprocess.check_output([str(helper),str(fixture)],env=env,text=True))
        baseline=inspect();check(baseline['version'] in (33,34,35,36) and 'canonical_paint' not in baseline and 'baked_normal_fields' in baseline and baseline['active']!=0,'Actual canonical float input retains original normal ownership before writable UV adoption');connection=display.Display(screen)
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
            before=fixture.stat()
            for _ in range(1000):
                if (await nodes())['Save'][1].get('enabled',True):break
                await asyncio.sleep(.02)
            else:raise TimeoutError('Keep-detail Apply never enabled Save')
            await command('Save')
            # Client processing is acknowledged by its actual atomic file
            # replacement, not XSync or a fixed cross-channel delay.
            await wait(lambda:fixture.stat().st_ino!=before.st_ino or fixture.stat().st_mtime_ns!=before.st_mtime_ns)
        async def editor():
            await command('UV editor...');value=await wait(lambda:named('UV editor'));value.set_input_focus(X.RevertToParent,X.CurrentTime);connection.sync();await asyncio.sleep(.25);return value
        async def keep_paint(edit):
            g=edit.get_geometry();tap(edit,g.width*(13/16 if compact else 5/6),row*(.5 if compact else 2.5));await asyncio.sleep(.2)
        async def tool(edit,last=False):
            g=edit.get_geometry()
            if compact:
                tap(edit,g.width/8,row/2);await asyncio.sleep(.1)
                # Canonical FLTK popup navigation implements Backspace/Tab,
                # not Home/End. Backspace wraps to the last active item;
                # Tab wraps forward to Scale when that is the requested tool.
                key('BackSpace')
                if not last:key('Tab')
                await asyncio.sleep(.1);capture(connection.screen().root,'restore-popup.png' if last else 'scale-popup.png');key('Return')
            else:tap(edit,g.width/2 if last else g.width/12,row*3.5 if last else row/2)
            await asyncio.sleep(.2)
        edit=await editor();g=edit.get_geometry();initial=capture(edit,'default.png')
        await keep_paint(edit);selected=capture(edit,'selected.png')
        check(initial.tobytes()==selected.tobytes(),'Actual native canonical paint and normal detail start required, selected and disabled')
        check(edit.get_wm_transient_for().id==window.id,'Keep-detail controls retain native modal parent and universal application identity')
        key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:named('UV editor') is None);await save()
        check(inspect()==baseline,'Native FLTK Ctrl+W cancels the choice and preserves the complete original native float project')
        edit=await editor();g=edit.get_geometry();await keep_paint(edit);await tool(edit)
        tap(edit,g.width/2+80,g.height-row/2);await wait(lambda:named('UV editor') is None);await save();accepted=inspect()
        check(accepted['version']==37 and accepted['source_materials']['version']==19 and bool(accepted['canonical_paint']['nodes']) and bool(accepted['paint_sampling']['domains']),'Actual native required-paint Scale and Apply saves genuine canonical37/material19 ownership')
        a=next(o for o in accepted['objects'] if o['id']==accepted['active']);b=next(o for o in baseline['objects'] if o['id']==baseline['active']);material=accepted['source_materials']['bindings'][a['material_bindings']];node=accepted['canonical_paint']['nodes'][material['canonical_paint_sampling']];before=accepted['polygon_sources']['origins'][node['before_origin']]
        check(accepted['polygon_sources']['paints'][before['painted']]==b['painted'],'Actual canonical owner retains every original float/layer/baked-normal-grid byte')
        ap=a['painted'].get('payload',a['painted']);bp=b['painted'].get('payload',b['painted']);check(ap['mesh']!=bp['mesh'] and {k:v for k,v in ap.items() if k!='mesh'}=={k:v for k,v in bp.items() if k!='mesh'},'Actual canonical UV changes coordinates while all current float/layer and independent normal fields stay exact')
        check(a['material']==b['material'] and all(a.get(k)==b.get(k) for k in ['appearance','deformation','polygon','shape_source','rig_source']) and material['canonical_attributes'] is not None and material['canonical_uv'] is not None and material['frames'] is not None,'Actual canonical UV retains original normal frames, model, rig and shape authorities')
        await command('Undo');await save();check(inspect()==baseline,'Universal Undo restores the exact complete pre-conversion native project')
        await command('Redo');await save();check(inspect()==accepted,'Universal Redo restores exact retained-domain paint, source and detail ownership')
        await command('Close Window');await wait(lambda:app.poll() is not None);check(app.returncode==0,'Actual canonical37 first session closes normally without sanitizer suppression');await wait(lambda:named('Dome Sculpt') is None)
        app,window,menu=await launch('restart');edit=await editor();g=edit.get_geometry();before=capture(edit,'restart.png')
        await keep_paint(edit);after=capture(edit,'required.png')
        check(before.tobytes()==after.tobytes(),'Restarted retained domain keeps its required toggle visibly selected and disabled')
        key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:named('UV editor') is None);await save()
        check(inspect()==accepted,'Reopening and cancelling required retained mode preserves every saved owner exactly')
        # Invoke the real exposed Restore action, then inspect the actual
        # saved project and ordinary global history in the restarted app.
        (output/'accepted.dsculpt').write_bytes(fixture.read_bytes())
        edit=await editor();await tool(edit,last=True);await wait(lambda:named('UV editor') is None);await save()
        check(inspect()==baseline,'Native Restore command returns exact original float paint/material/UV project after restart')
        await command('Undo');await save();check(inspect()==accepted,'Native Restore is one exact global Undo restoring the complete domain owner')
        await command('Redo');await save();check(inspect()==baseline,'Native Restore Redo and following Save retain exact original project')
        check(app.poll() is None,'Native retained-paint workflow leaves the private native application usable')
        await command('Close Window');await wait(lambda:app.poll() is not None);check(app.returncode==0,'Actual canonical37 restart closes normally without sanitizer suppression')
        (output/'accepted.json').write_text(json.dumps(accepted,indent=2)+'\n');(output/'baseline.json').write_text(json.dumps(baseline,indent=2)+'\n')
        connection.close()
    bus.disconnect()
try:asyncio.run(main())
finally:
    for process in reversed(processes):
        if process.poll() is None:
            process.terminate()
            try:process.wait(timeout=10)
            except subprocess.TimeoutExpired:process.kill();process.wait()
