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
COMPACT=os.environ.get('DOME_PROJECTION_COMPACT')=='1'
OUT=Path(os.environ.get('DOME_PROJECTION_OUT',ROOT/'artifacts/menu-projection'));OUT.mkdir(parents=True,exist_ok=True)
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
        read,write=os.pipe();log=(OUT/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','640x360x24' if COMPACT else '1280x1000x24','-nolisten','tcp'],pass_fds=(write,),stdout=log,stderr=log));os.close(write)
        with os.fdopen(read) as stream:screen=':'+stream.readline().strip()
        env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','XMODIFIERS':'@im=none'}
        for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
        for key,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[key]=home+'/'+suffix;Path(env[key]).mkdir(mode=0o700)
        if COMPACT:
            config=Path(home,'config/dome/appearance.json');config.parent.mkdir(parents=True,exist_ok=True);config.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
        project=Path(home,'native.dsculpt');asset=Path(home,'projector.png');helper=str(BUILD/'dome-sculpt-projection-native-test')
        subprocess.run([helper,'--ui-fixture',str(project)],check=True,env=env)
        Image.new('RGBA',(8,8),(255,0,0,255)).save(asset)
        log=(OUT/'app.log').open('w');app=subprocess.Popen([str(BUILD/'dome-sculpt'),str(project)],cwd=home,env=env,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
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
                if nodes['Save'][1].get('enabled',True) and (not imported or nodes['Undo'][1].get('enabled',False)):return nodes
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
        def tap(widget,x,y):
            g=widget.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
        def capture(name):
            g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(OUT/name)
        async def menu_state(label,expected):
            end=time.monotonic()+30
            while time.monotonic()<end:
                node=(await layout())[label][1]
                if node.get('toggle-state',0)==expected:return
                await asyncio.sleep(.04)
            raise TimeoutError('Projection image state not published')
        def target_center():
            g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);im=Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX');points=[(x,y) for y in range(g.height) for x in range(g.width) if (lambda p:p[2]>120 and p[0]<60 and p[1]<60)(im.getpixel((x,y)))]
            if len(points)<20:return None
            return ((min(x for x,y in points)+max(x for x,y in points))//2,(min(y for x,y in points)+max(y for x,y in points))//2)
        await ready();center=await wait(target_center);original=inspect()['scene']
        await command('Load projection image...');chooser=await wait(lambda:named('Projection image'))
        check(chooser.get_wm_transient_for().id==window.id and chooser.get_wm_class()[1]==window.get_wm_class()[1],'Projection chooser retains native document ownership and DomeD identity')
        await command('Close Window');await wait(lambda:named('Projection image') is None)
        check(inspect()['scene']==original,'Universal Close cancels image chooser with exact source/history')
        await command('Load projection image...');chooser=await wait(lambda:named('Projection image'));chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        key('Control_L',True);key('a');key('Control_L',False);text(str(asset));key('Tab')
        g=chooser.get_geometry();raw=chooser.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(OUT/'chooser-filled.png');tap(chooser,g.width-119,g.height-22)
        await wait(lambda:named('Projection image') is None);await menu_state('Use projection image',1)
        check((await layout())['Save'][1].get('enabled',True),'Detached PNG image loading preserves ordinary Save availability')
        await command('Save');check(inspect()['scene']==original,'Image loading and projector state are ephemeral and not serialized as scene edits')
        await command('Image projection options...');options=await wait(lambda:named('Image projection'));options.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();capture('options.png');key('Escape');await wait(lambda:named('Image projection') is None)
        await command('Save');check(inspect()['scene']==original,'Native projection options Cancel preserves original scene exactly')
        await command('Image projection options...');options=await wait(lambda:named('Image projection'));options.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();g=options.get_geometry()
        tap(options,g.width/2+80,g.height-(32 if COMPACT else 26));await wait(lambda:named('Image projection') is None)
        await command('Save');check(inspect()['scene']==original,'Native projection options Apply uses the same ephemeral settings without a scene/history edit')
        await command('Projection from view');capture('projector-preview.png')
        window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();g=window.get_geometry();cx,cy=center
        key('Shift_L',True);xtest.fake_input(d,X.MotionNotify,x=g.x+cx,y=g.y+cy);xtest.fake_input(d,X.ButtonPress,1);d.sync();await asyncio.sleep(.1);xtest.fake_input(d,X.ButtonRelease,1);key('Shift_L',False);d.sync()
        await command('Save');after=inspect();check(after['red']>240 and after['blue']<5 and after['coverage']>100,'Actual native Shift-drag paints image color over contrasting blue material')
        a=after['scene']['objects'][0].copy();b=original['objects'][0].copy();paint=a.pop('painted');before=b.pop('painted');check(a==b and paint['mesh']==before['mesh'],'Projection painting preserves exact original mesh/source/material/placement fields')
        await command('Undo');await command('Save');check(inspect()['scene']==original,'Actual universal Undo restores the complete pre-projection scene')
        await command('Redo');await command('Save');check(inspect()['scene']==after['scene'],'Actual universal Redo and ordered Save/reopen retain exact projected pixels')
        capture('painted.png');await command('Close Window');await wait(lambda:app.poll() is not None);check(app.returncode==0,'Projection workflow closes through normal App/worker/font teardown')
        d.close()
    bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
