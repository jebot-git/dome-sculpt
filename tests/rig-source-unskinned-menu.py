#!/usr/bin/env python3
"""Actual native FLTK humanoid fitting/worker/history on private Xvfb/bus."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_RIG_REST_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_RIG_REST_BUS':'1'})
ROOT=Path(os.environ.get('DOME_SCULPT_TEST_ROOT',Path(__file__).resolve().parents[1]));BUILD=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',ROOT/'build-rig-rest-release')).resolve()
OUT=Path(os.environ.get('DOME_RIG_REST_OUT',ROOT/'artifacts/rig-rest-menu'));OUT.mkdir(parents=True,exist_ok=True)
COMPACT=os.environ.get('DOME_RIG_REST_COMPACT')=='1';checks=[];processes=[]
def check(value,message):
 checks.append({'passed':bool(value),'check':message});(OUT/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks)},indent=2)+'\n');print(('PASS ' if value else 'FAIL ')+message,flush=True);assert value,message
async def wait(fn,seconds=40):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=fn()
  if value:return value
  await asyncio.sleep(.04)
 raise TimeoutError('Native rig fitting did not reach expected state')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-rig-rest-menu-') as home:
  read,write=os.pipe();log=(OUT/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','640x360x24' if COMPACT else '1280x1000x24','-nolisten','tcp'],pass_fds=(write,),stdout=log,stderr=log));os.close(write)
  with os.fdopen(read) as stream:screen=':'+stream.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
  for key,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[key]=home+'/'+suffix;Path(env[key]).mkdir(mode=0o700)
  if COMPACT:
   path=Path(home,'config/dome/appearance.json');path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  project=Path(home,'humanoid.dsculpt');helper=str(BUILD/'dome-sculpt-rig-source-unskinned-ui-fixture');subprocess.run([helper,'--ui-fixture',str(project)],check=True,env=env)
  def inspect():return json.loads(subprocess.check_output([helper,'--inspect',str(project)],env=env))
  original=inspect()['scene'];expected=json.loads(subprocess.check_output([helper,'--expected',str(project)],env=env));(OUT/'original.json').write_text(json.dumps(original,indent=2));log=(OUT/'app.log').open('w');app=subprocess.Popen([str(BUILD/'dome-sculpt'),str(project)],cwd=home,env=env,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if title in str(w.get_wm_name() or '') and w.get_attributes().map_state==X.IsViewable:return w
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
   nodes=await layout();old=project.stat().st_mtime_ns;await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
   if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=old)
   await asyncio.sleep(.15)
  def key(name,down=None):
   for event in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,event,d.keysym_to_keycode(XK.string_to_keysym(name)))
   d.sync()
  def tap(w,x,y):
   g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
  def capture(w,name):
   g=w.get_geometry();raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(OUT/name)
  await command('Humanoid rig fitting...');dialog=await wait(lambda:named('Fit existing rig'));g=dialog.get_geometry();capture(dialog,'landmark-controls.png')
  check(g.width<=640 if COMPACT else g.width<1280,'Shared landmark and world-coordinate controls fit the native work area')
  key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:not named('Fit existing rig'));await command('Save');check(inspect()['scene']==original,'Native universal Close cancels existing rig fitting without a source or history edit')
  await command('Humanoid rig fitting...');dialog=await wait(lambda:named('Fit existing rig'));g=dialog.get_geometry();row=48 if COMPACT else 36
  if COMPACT:
   stable=0;previous=None
   for _ in range(12):
    xtest.fake_input(d,X.MotionNotify,x=g.x+g.width-8,y=g.y+(g.height-row*2)//2)
    for step in range(10):xtest.fake_input(d,X.ButtonPress,5);xtest.fake_input(d,X.ButtonRelease,5)
    d.sync();await asyncio.sleep(.1);raw=dialog.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff).data
    stable=stable+1 if raw==previous else 0;previous=raw
    if stable>=2:break
   check(stable>=2,'Compact controller/source controls reach a stable fully visible bottom scroll position')
  viewport=g.height-row*2
  tap(dialog,40,viewport-row*2.5);tap(dialog,40,viewport-row*1.5);await asyncio.sleep(.1);capture(dialog,'original-source-controls.png')
  button=min(g.width/2-24,max(140,(32 if COMPACT else 16)*8));tap(dialog,g.width/2+10+button/2,g.height-row*2+8+row*.5);await wait(lambda:not named('Fit existing rig'))
  await command('Save');await wait(lambda:inspect()['scene']==expected);restored=inspect()
  check(restored['scene']==expected,'Actual native Restore and immediately ordered Save recover the complete originally unskinned source')
  check(not restored['target_skinned'] and restored['scene']['objects'][1]['deformation']['morphs'] is not None,'Originally absent rig is removed while the actual original authored shape remains')
  await command('Undo');await command('Save');check(inspect()['scene']==original,'Universal Undo restores the exact rigged current paint material mask and source capsule')
  await command('Redo');await command('Save');check(inspect()['scene']==expected,'Universal Redo restores the exact originally unskinned source and ordered saved project')
  await command('Close Window');await wait(lambda:app.poll() is not None);check(app.returncode==0,'Native fitting closes through normal App worker GL and font teardown')
  d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:
   p.terminate()
   try:p.wait(timeout=8)
   except subprocess.TimeoutExpired:p.kill();p.wait()
