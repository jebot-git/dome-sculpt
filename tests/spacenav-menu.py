#!/usr/bin/env python3
"""Native FLTK + real Unix stream fixture. No physical-device claim."""
import asyncio,json,os,signal,socket,struct,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image,ImageChops
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_SPACENAV_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_SPACENAV_BUS':'1'})
root=Path(__file__).resolve().parents[1];build=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',root/'build'))
out=Path(os.environ.get('DOME_SPACENAV_OUT',root/'artifacts/menu-spacenav'));out.mkdir(parents=True,exist_ok=True);checks=[];processes=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'complete':False},indent=2)+'\n');print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
async def wait(fn,seconds=45):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=fn()
  if value:return value
  await asyncio.sleep(.04)
 raise TimeoutError('Native 3D mouse fixture condition timed out')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-spnav-ui-') as home:
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1280x1000x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log));os.close(wr)
  with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none','SPNAV_SOCKET':home+'/spnav.sock'}
  for k in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(k,None)
  for k,v in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[k]=home+'/'+v;Path(env[k]).mkdir(mode=0o700)
  listener=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);listener.bind(env['SPNAV_SOCKET']);listener.listen(4);listener.setblocking(False)
  helper=build/'dome-sculpt-desktop-framing-fixture';project=Path(home,'navigation.dsculpt');subprocess.run([str(helper),'--fixture',str(project)],env=env,check=True)
  def inspect():return json.loads(subprocess.check_output([str(helper),'--inspect',str(project)],env=env))
  original=inspect();log=(out/'app.log').open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],cwd=home,env=env,stdout=log,stderr=log);processes.append(app)
  peer,_=await asyncio.wait_for(asyncio.get_running_loop().sock_accept(listener),45);peer.setblocking(False)
  d=display.Display(screen)
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if title in str(w.get_wm_name() or ''):return w
    except error.BadWindow:pass
  window=await wait(lambda:named('navigation.dsculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  def prop(name):
   p=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));menu=bus.get_proxy_object(service,path,await bus.introspect(service,path)).get_interface('com.canonical.dbusmenu')
  async def layout():
   nodes={};result=await menu.call_get_layout(0,-1,[])
   def walk(n):
    ident,p,children=n;p={k:v.value for k,v in p.items()};nodes[p.get('label','').replace('_','')]=(ident,p)
    for c in children:walk(c.value)
   walk(result[1]);return nodes
  async def command(label):
   end=time.monotonic()+45
   while time.monotonic()<end:
    nodes=await layout()
    if label in nodes and nodes[label][1].get('enabled',True):break
    await asyncio.sleep(.04)
   else:raise TimeoutError('Menu unavailable: '+label)
   stamp=project.stat().st_mtime_ns;await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
   if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=stamp)
   else:await asyncio.sleep(.15)
  def key(name,pressed=None):
   code=d.keysym_to_keycode(XK.string_to_keysym(name))
   for value in ([pressed] if pressed is not None else [True,False]):xtest.fake_input(d,X.KeyPress if value else X.KeyRelease,code)
   d.sync()
  def focus():
   g=window.get_geometry();window.set_input_focus(X.RevertToParent,X.CurrentTime);xtest.fake_input(d,X.MotionNotify,x=g.x+g.width//2,y=g.y+g.height//2);xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
  def pixels():
   g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF);return Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').crop((0,0,g.width,g.height-40))
  def difference(a,b):
   delta=ImageChops.difference(a,b);return sum(any(c>8 for c in p) for p in (delta.get_flattened_data() if hasattr(delta,'get_flattened_data') else delta.getdata()))
  async def event(words):await asyncio.get_running_loop().sock_sendall(peer,struct.pack('=8i',*words))
  async def motion(axes=(0,0,0,0,0,0),duration=.12,period=0):
   end=time.monotonic()+duration
   while time.monotonic()<end:
    await event((0,*axes,period));await asyncio.sleep(.012)
  def blue_center():
   image=pixels();data=image.get_flattened_data() if hasattr(image,'get_flattened_data') else image.getdata();locations=[i%image.width for i,(r,g,b) in enumerate(data) if b>r+25 and b>75];assert locations,'Selected blue surface missing';return sum(locations)/len(locations)
  def tap_dialog(dialog,x,y):
   g=dialog.get_geometry();xtest.fake_input(d,X.MotionNotify,x=int(g.x+x),y=int(g.y+y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
  async def frame():
   await command('Frame selection');focus();await motion();await asyncio.sleep(.08)
  await command('Frame all');focus();await motion();nodes=await layout()
  check(nodes['3D mouse navigation'][1].get('toggle-state')==1 and '3D mouse options...' in nodes,'Universal View menu exposes enabled-by-default 3D mouse navigation and native options')
  for axis,name in enumerate(('pan X','pan Y','dolly','orbit X','orbit Y','orbit Z')):
   await frame();before=pixels();axes=[0]*6;axes[axis]=280;await motion(axes,.35,0 if axis%2 else 8);await motion();await asyncio.sleep(.08);after=pixels();after.save(out/(name.replace(' ','-')+'.png'))
   check(difference(before,after)>100,'Actual native Unix '+name+' changes the GL camera without mouse emulation')
  before=pixels();await asyncio.sleep(.35);check(difference(before,pixels())==0,'Neutral input leaves the complete native viewport stationary')
  await command('Save');check(inspect()==original,'All six navigation axes leave exact saved scene and authoring history unchanged')
  await frame();before=pixels();await command('3D mouse options...');dialog=await wait(lambda:named('3D mouse navigation'));await motion((280,0,0,0,0,0),.2);key('Escape');await wait(lambda:not named('3D mouse navigation'));await asyncio.sleep(.1)
  check(difference(before,pixels())==0,'Native modal options fence incoming navigation and Escape cancels staged changes')
  # Deflection which remains held after focus restoration must not resume.
  focus();await motion((280,0,0,0,0,0),.2);check(difference(before,pixels())==0,'Returning from a modal requires a fresh neutral before held axes can move the view')
  await motion();await motion((280,0,0,0,0,0),.2);await motion();check(difference(before,pixels())>100,'Fresh neutral then deflection resumes actual native navigation')
  await frame();normal_center=blue_center();await motion((280,0,0,0,0,0),.18);await motion();normal_delta=blue_center()-normal_center
  await command('3D mouse options...');dialog=await wait(lambda:named('3D mouse navigation'));g=dialog.get_geometry();row=g.height/17
  raw=dialog.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/'options.png')
  tap_dialog(dialog,30,row*9.5);await asyncio.sleep(.08);key('Escape');await wait(lambda:not named('3D mouse navigation'))
  await frame();cancel_center=blue_center();await motion((280,0,0,0,0,0),.18);await motion();cancel_delta=blue_center()-cancel_center
  check(normal_delta>20 and cancel_delta>20,'Cancel in native options discards the staged axis inversion')
  await command('3D mouse options...');dialog=await wait(lambda:named('3D mouse navigation'));g=dialog.get_geometry();row=g.height/17;tap_dialog(dialog,30,row*9.5);tap_dialog(dialog,g.width/2+80,g.height-row*1.5);await wait(lambda:not named('3D mouse navigation'))
  await frame();invert_center=blue_center();await motion((280,0,0,0,0,0),.18);await motion();invert_delta=blue_center()-invert_center
  check(invert_delta < -20,'Actual native options Apply reverses only the selected navigation axis')
  await frame();before=pixels();xtest.fake_input(d,X.ButtonPress,3);d.sync();await motion((280,0,0,0,0,0),.2);xtest.fake_input(d,X.ButtonRelease,3);d.sync();await asyncio.sleep(.08)
  check(difference(before,pixels())==0,'A held desktop mouse gesture owns the view and fences incoming 3D axes')
  await frame();before=pixels();xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonPress,3);xtest.fake_input(d,X.ButtonRelease,3);d.sync();await motion();await motion((280,0,0,0,0,0),.2);xtest.fake_input(d,X.ButtonRelease,1);d.sync();await asyncio.sleep(.08)
  check(difference(before,pixels())==0,'Releasing one mouse button cannot give camera ownership away while another remains held')
  await motion();await event((2,1,0,0,0,0,0,0));await event((1,1,0,0,0,0,0,0));await asyncio.sleep(.2);all_image=pixels();check(difference(before,all_image)>100,'Real mapped second-button event reuses Frame all')
  await event((2,1,0,0,0,0,0,0));await motion();await event((2,0,0,0,0,0,0,0));await event((1,0,0,0,0,0,0,0));await asyncio.sleep(.2);check(difference(all_image,pixels())>100,'Real mapped first-button event reuses Frame selection')
  await event((2,0,0,0,0,0,0,0));await motion();before=pixels();await command('3D mouse navigation');await wait(lambda:app.poll() is None)
  check(await asyncio.wait_for(asyncio.get_running_loop().sock_recv(peer,1),10)==b'','View toggle off closes the daemon connection without changing daemon settings')
  peer.close();await command('3D mouse navigation');peer,_=await asyncio.wait_for(asyncio.get_running_loop().sock_accept(listener),45);peer.setblocking(False);focus();await motion((280,0,0,0,0,0),.2)
  check(difference(before,pixels())==0,'Reconnect discards held axes until the newly connected device establishes neutral')
  await motion();await motion((280,0,0,0,0,0),.2);await motion();check(difference(before,pixels())>100,'Reconnected native stream resumes after its explicit release fence')
  await command('Save');check(inspect()==original and app.poll() is None and not (await layout())['Undo'][1].get('enabled',True),'Options, reconnect and frame buttons preserve exact scene state and do not consume Undo')
  await frame();await motion((60,40,0,40,60,30),.16);await motion();await command('Red paint');g=window.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+g.width//2,y=g.y+(g.height-28)//2);key('Shift_L',True);xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);key('Shift_L',False);d.sync();await command('Save');painted=inspect()
  check(painted['objects'][0]['painted']!=original['objects'][0]['painted'] and painted['objects'][1:]==original['objects'][1:],'Native paint after composed 3D-mouse camera motion hits the selected source through the inverse perspective ray')
  await command('Undo');await command('Save');check(inspect()==original,'One native Undo reverses only the accepted post-navigation paint')
  await command('Redo');await command('Save');check(inspect()==painted,'Native Redo restores exact post-navigation paint without consuming camera actions')
  pixels().save(out/'final.png');await command('Close Window');await wait(lambda:app.poll() is not None);check(app.returncode==0,'Native Close Window drains workers and exits cleanly after navigation/authoring');(out/'report.json').write_text(json.dumps({'checks':checks,'complete':True,'physical_device':False,'transport':'real private AF_UNIX fixture','backend':'canonical FLTK X11 test display'},indent=2)+'\n')
  peer.close();listener.close();d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
