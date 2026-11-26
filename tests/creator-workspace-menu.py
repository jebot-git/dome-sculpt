#!/usr/bin/env python3
"""Actual native FLTK creator recolour/worker/history with private Xvfb and bus."""
import asyncio,hashlib,json,os,signal,subprocess,sys,time
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_CREATOR_PRIVATE_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_CREATOR_PRIVATE_BUS':'1'})
if len(sys.argv)!=5:raise SystemExit('usage: creator-workspace-menu.py APP INSPECTOR INPUT OUTPUT')
app,inspector,source,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True);checks=[];processes=[];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();pins={str(p):sha(p)for p in [app,inspector,source,Path(__file__).resolve()]}
def report(complete=False,error=None):(out/'report.json').write_text(json.dumps({'complete':complete,'error':error,'checks':checks,'count':len(checks),'pins':pins,'drift':[p for p,h in pins.items()if sha(p)!=h],'scope':'Actual desktop native controls and ordered creation; no physical headset or complete creator claim'},indent=2)+'\n')
def check(ok,label):checks.append({'passed':bool(ok),'label':label});report();print(('PASS 'if ok else'FAIL ')+label,flush=True);assert ok,label
async def wait(fn,seconds=45):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  result=fn()
  if result:return result
  await asyncio.sleep(.04)
 raise TimeoutError('Native creator condition timed out')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar');home=out/'home';home.mkdir();read,write=os.pipe()
 with(out/'xvfb.log').open('w')as log:processes.append(subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','1100x800x24','-nolisten','tcp','-noreset'],pass_fds=(write,),stdout=log,stderr=subprocess.STDOUT))
 os.close(write)
 with os.fdopen(read)as stream:screen=':'+stream.readline().strip()
 env=os.environ.copy();env.update(DISPLAY=screen,FLTK_BACKEND='x11',XDG_CURRENT_DESKTOP='Dome',LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',XMODIFIERS='@im=none')
 for key in ['WAYLAND_DISPLAY','WAYLAND_SOCKET','LD_PRELOAD']:env.pop(key,None)
 for key,leaf in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[key]=str(home/leaf);Path(env[key]).mkdir(mode=0o700)
 appearance=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';appearance.parent.mkdir();appearance.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':1}))
 project=out/'author.dsculpt';project.write_bytes(source.read_bytes())
 with(out/'app.log').open('w')as log:processes.append(subprocess.Popen([str(app),str(project)],env=env,stdout=log,stderr=subprocess.STDOUT))
 d=display.Display(screen)
 def named(title):
  for w in d.screen().root.query_tree().children:
   try:
    if title in str(w.get_wm_name()or'')and w.get_attributes().map_state==X.IsViewable:return w
   except error.BadWindow:pass
 window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
 def prop(name):
  value=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(value.value).decode()if value else None
 service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));menu=proxy.get_interface('com.canonical.dbusmenu')
 async def layout():
  result=await menu.call_get_layout(0,-1,[]);nodes={}
  def walk(node):
   ident,properties,children=node;properties={k:v.value for k,v in properties.items()};nodes[properties.get('label','').replace('_','')]=(ident,properties)
   for child in children:walk(child.value)
  walk(result[1]);return nodes
 async def command(label):
  deadline=time.monotonic()+45
  while True:
   nodes=await layout()
   if label in nodes and nodes[label][1].get('enabled',True):break
   if time.monotonic()>=deadline:
    (out/'menu-unavailable.json').write_text(json.dumps({'requested':label,'actual_nodes':nodes},indent=2)+'\n')
    raise TimeoutError('Native universal command remained unavailable: '+label)
   await asyncio.sleep(.04)
  check(label in nodes and nodes[label][1].get('enabled',True),'Actual native universal command available: '+label);stamp=project.stat().st_mtime_ns;await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
  if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=stamp)
  await asyncio.sleep(.15)
 def key(name,down=None):
  for event in ([X.KeyPress,X.KeyRelease]if down is None else[X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,event,d.keysym_to_keycode(XK.string_to_keysym(name)))
  d.sync()
 def tap(w,x,y,button=1):
  g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,button);xtest.fake_input(d,X.ButtonRelease,button);d.sync()
 def capture(w,name):
  g=w.get_geometry();raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/name)
 def text(value):
  key('Control_L',True);key('a');key('Control_L',False)
  for character in value:key('period'if character=='.'else character)
  key('Tab')
 def verify(mode):
  with(out/(mode+'-endpoint.log')).open('w')as log:code=subprocess.run([str(inspector),'--verify-xr',str(source),str(project),mode],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=60).returncode
  check(code==0,'Native endpoint retains exact original authoring and requested creator result: '+mode)
 await command('Create humanoid...');dialog=await wait(lambda:named('Create humanoid'));check(dialog.get_wm_transient_for().id==window.id,'Creator uses the actual native modal owned by the document');capture(dialog,'creator-before.png');key('Escape');await wait(lambda:not named('Create humanoid'));await command('Save');verify('cancel')
 await command('Create humanoid...');dialog=await wait(lambda:named('Create humanoid'));g=dialog.get_geometry();row=28
 tap(dialog,g.width*.75,row*3.5);await asyncio.sleep(.1);key('Down');key('Down');key('Return');await asyncio.sleep(.15);capture(dialog,'creator-anime-preset.png')
 tap(dialog,g.width*.75,row*4.5);text('1.9');tap(dialog,g.width*.75,row*6.5);text('0.4')
 for _ in range(32):tap(dialog,g.width*.5,g.height*.5,5)
 await asyncio.sleep(.2);capture(dialog,'creator-hair-source.png')
 # The source content is exactly33 rows, and the visible scroll area22 rows.
 # Native scroll-to-end leaves the explicit colour checkbox on row16.
 tap(dialog,30,row*16.5);tap(dialog,g.width*.75,row*17.5);text('0.7');capture(dialog,'creator-red-recolour.png')
 tap(dialog,g.width*.5+80,g.height-row*1.5+8);await wait(lambda:not named('Create humanoid'));await command('Save');verify('accept-desktop');capture(window,'created-avatar.png')
 await command('Undo');await command('Save');verify('cancel');await command('Redo');await command('Save');verify('accept-desktop')
 key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:processes[-1].poll()is not None);check(processes[-1].returncode==0,'Actual desktop app closes cleanly after native creator Save/Undo/Redo');d.close();bus.disconnect();report(True)
try:asyncio.run(main())
except BaseException as error:report(error=repr(error));raise
finally:
 for process in reversed(processes):
  if process.poll()is None:
   process.terminate()
   try:process.wait(timeout=10)
   except subprocess.TimeoutExpired:process.kill();process.wait()
