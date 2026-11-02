#!/usr/bin/env python3
"""Real native FLTK framing/orbit/picking; private display, bus and state."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_FRAME_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_FRAME_BUS':'1'})
root=Path(__file__).resolve().parents[1];build=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',root/'build'));helper=Path(os.environ.get('DOME_FRAME_HELPER',build/'dome-sculpt-desktop-framing-fixture'));out=root/'artifacts/menu-desktop-framing';out.mkdir(parents=True,exist_ok=True);checks=[];processes=[]
def report(complete=False):(out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks),'complete':complete},indent=2)+'\n')
def check(ok,label):checks.append({'passed':bool(ok),'check':label});report();print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
async def wait(fn,seconds=45):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=fn()
  if value:return value
  await asyncio.sleep(.05)
 raise TimeoutError('Native framing condition timed out')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-frame-menu-') as home:
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1280x1000x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log));os.close(wr)
  with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for k in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(k,None)
  for k,v in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[k]=home+'/'+v;Path(env[k]).mkdir(mode=0o700)
  project=Path(home,'framing.dsculpt');subprocess.run([str(helper),'--fixture',str(project)],env=env,check=True)
  def inspect():return json.loads(subprocess.check_output([str(helper),'--inspect',str(project)],env=env))
  original=inspect();log=(out/'app.log').open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],cwd=home,env=env,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
  def named():
   for w in d.screen().root.query_tree().children:
    try:
     if 'framing.dsculpt' in str(w.get_wm_name() or ''):return w
    except error.BadWindow:pass
  window=await wait(named);window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  def prop(name):
   p=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));menu=bus.get_proxy_object(service,path,await bus.introspect(service,path)).get_interface('com.canonical.dbusmenu')
  async def layout():
   nodes={};result=await menu.call_get_layout(0,-1,[])
   def walk(n):
    ident,p,children=n;p={k:v.value for k,v in p.items()};nodes[p.get('label','').replace('_','')]=(ident,p)
    for c in children:walk(c.value)
   walk(result[1]);return nodes
  async def ready(label):
   end=time.monotonic()+45
   while time.monotonic()<end:
    n=await layout()
    if label in n and n[label][1].get('enabled',True):return n
    await asyncio.sleep(.05)
   raise TimeoutError('Native frame menu not enabled: '+label)
  async def command(label):
   n=await ready(label);stamp=project.stat().st_mtime_ns;await menu.call_event(n[label][0],'clicked',Variant('i',0),0)
   if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=stamp)
   else:await asyncio.sleep(.15)
  def key(name,pressed=None):
   code=d.keysym_to_keycode(XK.string_to_keysym(name))
   for value in ([pressed] if pressed is not None else [True,False]):xtest.fake_input(d,X.KeyPress if value else X.KeyRelease,code)
   d.sync()
  def pixels():
   g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF);return Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').crop((0,0,g.width,g.height-40))
  def colors():
   data=list(pixels().getdata());return sum(b>r+25 and b>75 for r,g,b in data),sum(r>b+25 and r>90 for r,g,b in data)
  await ready('Frame all');await ready('Frame selection');check(True,'Actual DomeD universal View menu exports enabled Frame all and Frame selection commands')
  key('Home');await wait(lambda:min(colors())>100);all_counts=colors();pixels().save(out/'all.png')
  check(min(all_counts)>100,'Home frames both real off-origin visible objects in the native GL viewport')
  await command('Frame selection');await wait(lambda:colors()[0]>all_counts[0]*4);selection_counts=colors();pixels().save(out/'selection.png')
  check(selection_counts[0]>all_counts[0]*4,'Universal Frame selection fills the view with the active object while retaining the other scene objects')
  await command('Save');check(inspect()==original,'Framing changes only camera state and creates no scene/history edit')
  g=window.get_geometry();cx=g.width//2;cy=(g.height-40)//2;xtest.fake_input(d,X.MotionNotify,x=g.x+cx,y=g.y+cy);xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.MotionNotify,x=g.x+cx+120,y=g.y+cy+45);xtest.fake_input(d,X.ButtonRelease,1);d.sync();await asyncio.sleep(.2)
  key('Shift_L',True);key('f');key('Shift_L',False);await asyncio.sleep(.2);pixels().save(out/'orbited-selection.png')
  check(colors()[0]>1000,'Shift+F frames the selected mesh after actual trackball orbit around its framed pivot')
  xtest.fake_input(d,X.MotionNotify,x=g.x+cx,y=g.y+cy);key('Shift_L',True);xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.MotionNotify,x=g.x+cx+10,y=g.y+cy+4);xtest.fake_input(d,X.ButtonRelease,1);key('Shift_L',False);d.sync()
  await command('Save');painted=inspect();check(painted['objects'][0]['painted']!=original['objects'][0]['painted'],'Actual native Shift paint after framing/orbit hits the intended off-origin mesh through the corrected inverse camera ray')
  check(painted['objects'][1:]==original['objects'][1:],'Native post-framing painting preserves the visible unselected and hidden objects exactly')
  await command('Undo');await command('Save');check(inspect()==original,'One universal Undo restores the exact original scene without consuming a camera action')
  await command('Redo');await command('Save');check(inspect()==painted,'One universal Redo restores the exact accepted post-framing paint')
  key('f');await wait(lambda:sum(colors())<50);check(sum(colors())<50,'Existing F reset still restores the original origin-centered trackball camera')
  key('Home');await wait(lambda:min(colors())>100);check(min(colors())>100,'Home restores the complete visible scene after the existing reset')
  await command('Save');check(inspect()==painted and app.poll() is None,'Frame/reset/orbit commands leave accepted authoring data and the native application intact')
  report(True);d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
