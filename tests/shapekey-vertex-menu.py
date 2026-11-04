#!/usr/bin/env python3
"""Actual FLTK shape authoring; private display/bus/HOME, no HMD."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image,ImageChops
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_SHAPE_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_SHAPE_BUS':'1'})
root=Path(__file__).resolve().parents[1];build=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',root/'build')).resolve();compact=os.environ.get('DOME_SHAPE_COMPACT')=='1';strokes=os.environ.get('DOME_SHAPE_STROKES')=='1';prepared=os.environ.get('DOME_SHAPE_PREPARED')=='1'
out=root/'artifacts'/os.environ.get('DOME_SHAPE_MENU_OUT',(('prepared-' if prepared else '')+('shape-menu-compact' if compact else 'shape-menu')));out.mkdir(parents=True,exist_ok=True);checks=[];processes=[]
def report(done=False):(out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks),'complete':done},indent=2)+'\n')
def check(ok,message):checks.append({'passed':bool(ok),'check':message});report();print(('PASS ' if ok else 'FAIL ')+message,flush=True);assert ok,message
async def wait(fn,seconds=45):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=fn()
  if value:return value
  await asyncio.sleep(.04)
 raise TimeoutError('Native shape condition timed out')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-shape-menu-') as home:
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','640x360x24' if compact else '1280x900x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log));os.close(wr)
  with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for k in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(k,None)
  for k,s in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[k]=home+'/'+s;Path(env[k]).mkdir(mode=0o700)
  if compact:
   p=Path(home,'config/dome/appearance.json');p.parent.mkdir(parents=True);p.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  project=Path(home,'shapes.dsculpt');helper=str(build/os.environ.get('DOME_SHAPE_HELPER',('dome-sculpt-prepared-normal-shapes-test' if prepared else 'dome-sculpt-shapekey-authoring-test')));subprocess.run([str(build/'dome-sculpt-shapekey-grid-fixture'),str(project)] if strokes and not os.environ.get('DOME_SHAPE_HELPER') else [helper,'--ui-fixture',str(project)],env=env,check=True)
  def inspect():return json.loads(subprocess.check_output([helper,'--inspect',str(project)],env=env))
  original=inspect();initial_targets=len(original['objects'][0]['deformation']['morphs']['targets']) if original['objects'][0].get('deformation',{}).get('morphs') else 0;log=(out/'app.log').open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],env=env,cwd=home,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if title in str(w.get_wm_name() or ''):return w
    except error.BadWindow:pass
  window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  def prop(name):
   p=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));menu=bus.get_proxy_object(service,path,await bus.introspect(service,path)).get_interface('com.canonical.dbusmenu')
  async def layout():
   nodes={};result=await menu.call_get_layout(0,-1,[])
   def walk(n):
    ident,p,children=n;p={k:v.value for k,v in p.items()};nodes[p.get('label','').replace('_','')]=(ident,p)
    for c in children:walk(c.value)
   walk(result[1]);return nodes
  async def ready(label,enabled=True):
   end=time.monotonic()+45
   while time.monotonic()<end:
    n=await layout()
    if n[label][1].get('enabled',True)==enabled:return n
    await asyncio.sleep(.04)
   raise TimeoutError('Native shape menu not ready: '+label)
  async def command(label):
   n=await layout();assert n[label][1].get('enabled',True),label+' disabled';stamp=project.stat().st_mtime_ns
   await menu.call_event(n[label][0],'clicked',Variant('i',0),0)
   if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=stamp)
  def key(name):
   code=d.keysym_to_keycode(XK.string_to_keysym(name));xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code);d.sync()
  def tap(w,x,y):
   g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
  def pixels(w):
   g=w.get_geometry();raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF);return Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX')
  await ready('Mirror shapekey...');await command('Mirror shapekey...');dialog=await wait(lambda:named('Mirror shapekey'))
  check(dialog.get_wm_transient_for().id==window.id,'Default Mirror uses the actual document-owned FLTK modal')
  dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return');await wait(lambda:named('Mirror shapekey') is None);await command('Save')
  check(inspect()==original,'Default Return cancels Mirror and preserves the complete native project')
  prior=out/'mirror-before.dsculpt';prior.write_bytes(project.read_bytes())
  await command('Mirror shapekey...');dialog=await wait(lambda:named('Mirror shapekey'));dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();g=dialog.get_geometry();row=46 if compact else 32
  check(g.width<=600 and g.height<=320 if compact else g.height<=860,'Mirror controls fit the normal or large-bitmap compact display')
  await asyncio.sleep(.15);pixels(dialog).save(out/'mirror-top.png');before=pixels(dialog)
  xtest.fake_input(d,X.MotionNotify,x=g.x+g.width-8,y=g.y+(g.height-row*2)//2)
  previous=before.tobytes();stable=0;reached=False
  for _ in range(24):
   for _ in range(8):xtest.fake_input(d,X.ButtonPress,5);xtest.fake_input(d,X.ButtonRelease,5)
   d.sync();await asyncio.sleep(.08);current=pixels(dialog).tobytes()
   stable=stable+1 if current==previous else 0;previous=current
   if stable>=2 and current!=before.tobytes():reached=True;break
  pixels(dialog).save(out/'mirror-method.png')
  check(reached,'Actual Mirror controls reach the stable scroll bottom with the method visible')
  check(pixels(dialog).crop((0,g.height-row*2,g.width,g.height)).tobytes()==before.crop((0,g.height-row*2,g.width,g.height)).tobytes(),'Cancel and Apply remain fixed while Mirror controls scroll')
  tap(dialog,g.width*.75,g.height-row*2+24);await wait(lambda:named('Mirror shapekey') is None);await ready('New shapekey...');await command('Save');mirrored=inspect();saved=out/'mirror-after.dsculpt';saved.write_bytes(project.read_bytes())
  subprocess.run([helper,'--verify-ui-vertex',str(prior),str(saved)],env=env,check=True)
  check(len(mirrored['objects'][0]['deformation']['morphs']['targets'])==initial_targets+1,'Actual default Apply accepts one nonzero vertex-position Mirror')
  await command('Undo');await command('Save');check(inspect()==original,'One universal Undo restores the exact pre-Mirror project')
  await command('Redo');await command('Save');check(inspect()==mirrored,'One universal Redo restores the exact accepted Mirror')
  await command('Close Window');await wait(lambda:app.poll() is not None)
  check(app.returncode==0,'The actual native application closes cleanly after saving Mirror')
  report(True);d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
