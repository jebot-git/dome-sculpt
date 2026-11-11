#!/usr/bin/env python3
"""Actual public universal-menu GPU blur, ordered Save and exact Undo/Redo.
Original texture-set GUI81 checks are separate and unchanged. Requires actual
hardware worker; private Xvfb is only the desktop display, not a fake editor.
"""
import asyncio,hashlib,json,os,signal,subprocess,sys,time,struct,math
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_BLUR_MENU_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_BLUR_MENU_BUS':'1'})
if len(sys.argv)!=5:raise SystemExit('usage: gpu-color-blur-menu.py APP INSPECTOR FIXTURE_MANIFEST OUTPUT')
app_path,inspector,manifest_path,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
manifest=json.loads(manifest_path.read_text());fixtures={k:(manifest_path.parent/v).resolve() for k,v in manifest['files'].items()};fixture=fixtures['original']
processes=[];checks=[];app=None
diagnostic_snapshot=lambda why:None
public_acks=[];last_action='startup'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins={str(p):sha(p) for p in [app_path,inspector,manifest_path,Path(__file__).resolve(),*fixtures.values()]}
def report(complete=False,problem=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':problem,'checks':checks,'count':len(checks),'inputs':pins,'drift':[p for p,v in pins.items() if sha(p)!=v],'public_acknowledgments':public_acks,'last_action':last_action,'scope':'Actual native FLTK universal Paint menu/worker/ordered Save/standard UndoRedo/fresh reopen; public X11/DBus acknowledgments and exact native files, no product observer or physical headset claim'},indent=2)+'\n')
def check(value,label):
 checks.append({'passed':bool(value),'label':label});report();print(('PASS ' if value else 'FAIL ')+label,flush=True);assert value,label
async def wait(fn,seconds=50):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=fn()
  if value:return value
  if app is not None and app.poll() is not None:raise RuntimeError(('ordinary app exited',app.returncode))
  await asyncio.sleep(.05)
 diagnostic_snapshot('wait-timeout')
 raise TimeoutError('Actual FLTK workflow did not reach its expected state')
async def main():
 global app,diagnostic_snapshot,last_action
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 home=out/'home';home.mkdir();rd,wr=os.pipe();xlog=(out/'xvfb.log').open('w');xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0',os.environ.get('DOME_BLUR_MENU_SCREEN','1100x800x24'),'-nolisten','tcp'],pass_fds=(wr,),stdout=xlog,stderr=xlog);processes.append(xvfb);os.close(wr)
 with os.fdopen(rd) as p:screen=':'+p.readline().strip()
 env={**os.environ,'HOME':str(home),'DISPLAY':screen,'XDG_CURRENT_DESKTOP':'Dome','FLTK_BACKEND':'x11','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none','MESA_SHADER_CACHE_DISABLE':'true'}
 for name in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(name,None)
 if os.environ.get('DOME_BLUR_APP_PRELOAD'):env['LD_PRELOAD']=os.environ['DOME_BLUR_APP_PRELOAD']
 for name,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[name]=str(home/suffix);Path(env[name]).mkdir(mode=0o700)
 project=home/'actual.dsculpt';project.write_bytes(fixture.read_bytes());d=display.Display(screen)
 def focus_id():
  value=d.get_input_focus().focus;return getattr(value,'id',value)
 def visible(window):
  try:return window.get_attributes().map_state==X.IsViewable
  except error.BadWindow:return False
 def image(window):
  g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff)
  return Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX')
 def window_info(window):
  g=window.get_geometry();a=window.get_attributes()
  return {'id':window.id,'parent':window.query_tree().parent.id,'name':str(window.get_wm_name() or ''),'class':str(window.get_wm_class()),'x':g.x,'y':g.y,'width':g.width,'height':g.height,'map_state':a.map_state,'override_redirect':a.override_redirect}
 def acknowledge(kind,**values):
  row={'kind':kind,'time':time.monotonic(),'focus':focus_id(),**values};public_acks.append(row)
  with (out/'public-acks.jsonl').open('a') as f:f.write(json.dumps(row)+'\n')
 def diagnostic_snapshot(why):
  prefix='failure-'+str(len(list(out.glob('failure-*.json')))).zfill(3)
  evidence={'reason':why,'last_action':last_action,'checks':len(checks),'app_poll':app.poll() if app else None,'windows':[]}
  try:
   evidence['focus']=focus_id()
   for window in d.screen().root.query_tree().children:
    try:
     info=window_info(window);evidence['windows'].append(info)
     if info['map_state']==X.IsViewable:image(window).save(out/(prefix+'-window-'+str(window.id)+'.png'))
    except (error.BadWindow,error.BadMatch):pass
   image(d.screen().root).save(out/(prefix+'-root.png'))
  except Exception as failure:evidence['capture_error']=repr(failure)
  (out/(prefix+'.json')).write_text(json.dumps(evidence,indent=2)+'\n')
 def inspect():return json.loads(subprocess.check_output([str(inspector),str(project)],env=env))
 def named(title):
  for w in d.screen().root.query_tree().children:
   try:
    if visible(w) and title in str(w.get_wm_name() or ''):return w
   except error.BadWindow:pass
 def key(name,down=None):
  code=d.keysym_to_keycode(XK.string_to_keysym(name))
  for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,code)
  d.sync()
 async def focus(window):
  window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();await wait(lambda:focus_id()==window.id)
 def mouse(w,x,y,down=None):
  g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y))
  if down is not None:xtest.fake_input(d,X.ButtonPress if down else X.ButtonRelease,1)
  d.sync()
 async def launch(number):
  global app
  log=(out/('app-'+str(number)+'.log')).open('w');app=subprocess.Popen([str(app_path),str(project)],env=env,cwd=home,stdout=log,stderr=log);processes.append(app);w=await wait(lambda:named('Dome Sculpt'));await focus(w)
  await wait(lambda:project.name in str(w.get_wm_name() or ''));acknowledge('document-title-visible',window=window_info(w))
  def prop(name):
   p=w.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));menu=bus.get_proxy_object(service,path,await bus.introspect(service,path)).get_interface('com.canonical.dbusmenu')
  async def layout():
   r=await menu.call_get_layout(0,-1,[]);values={}
   def walk(n):
    ident,props,children=n;props={k:v.value for k,v in props.items()};values[props.get('label','').replace('_','')]=(ident,props)
    for c in children:walk(c.value)
   walk(r[1]);return values
  async def command(label,pause=.15):
   global last_action
   last_action='universal menu '+label
   for _ in range(1000):
    nodes=await layout()
    if label in nodes and nodes[label][1].get('enabled',True):break
    await asyncio.sleep(.05)
   else:raise AssertionError(('Actual universal menu command disabled',label))
   with (out/'commands.jsonl').open('a') as f:f.write(json.dumps({'label':label,'time':time.monotonic()})+'\n')
   await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
   acknowledge('public-menu-command',label=label,id=nodes[label][0],properties=nodes[label][1]);
   if pause:await asyncio.sleep(pause)
  (out/('runtime-maps-'+str(number)+'.txt')).write_text(Path('/proc/'+str(app.pid)+'/maps').read_text());return w,layout,command
 async def save(command,immediate=False):
  stamp=project.stat().st_mtime_ns;await command('Save',pause=0 if immediate else .15);await wait(lambda:project.stat().st_mtime_ns!=stamp);return inspect()
 def chord(*names):
  for name in names:key(name,True)
  for name in reversed(names):key(name,False)
 def picture(window,name):image(window).save(out/name)
 async def menu_state(layout,label,enabled):
  last={}
  async def read():
   nonlocal last
   nodes=await layout();last=nodes
   return label in nodes and bool(nodes[label][1].get('enabled',True))==enabled
  deadline=time.monotonic()+50
  while time.monotonic()<deadline:
   if await read():acknowledge('public-menu-state',label=label,enabled=enabled,properties=last[label][1]);return last
   await asyncio.sleep(.05)
  diagnostic_snapshot('menu-state-timeout');raise TimeoutError((label,enabled))
 original=json.loads(subprocess.check_output([str(inspector),str(fixtures['original'])],env=env));expected=json.loads(subprocess.check_output([str(inspector),str(fixtures['accepted'])],env=env))
 check(manifest['version']==1 and manifest['resolution']==64 and manifest['flow']==1 and manifest['independent_pixel_oracle'] and expected!=original,'Actual native producer provides nonuniform strictUV color/alpha/PBR and independent pixel-oracle accepted endpoint')
 window,layout,command=await launch(0);check(inspect()==original,'Actual ordinary app opens the complete original native fixture')
 nodes=await menu_state(layout,'Blur active color layer on GPU',True);check('Paint'in nodes,'Actual DomeD universal Paint menu exports the enabled GPU blur command')
 await menu_state(layout,'Undo',False);await menu_state(layout,'Redo',False);picture(window,'before.png')
 await command('Blur active color layer on GPU',pause=0)
 accepted=await save(command,immediate=True)
 check(accepted==expected,'Actual public menu and immediate ordered Save publish the exact independently checked native GPU endpoint')
 await menu_state(layout,'Blur active color layer on GPU',True);await menu_state(layout,'Undo',True);await menu_state(layout,'Redo',False);picture(window,'accepted.png')
 await focus(window);chord('Control_L','z');await menu_state(layout,'Redo',True)
 check(await save(command,immediate=True)==original,'Standard Ctrl+Z and ordered native Save restore every original field/source/layer byte in one step')
 await menu_state(layout,'Undo',False);picture(window,'undone.png')
 chord('Control_L','Shift_L','z');await menu_state(layout,'Undo',True);await menu_state(layout,'Redo',False)
 check(await save(command,immediate=True)==expected,'Standard Ctrl+Shift+Z restores the exact full accepted native endpoint in one step')
 picture(window,'redone.png');await command('Close Window',pause=0);await wait(lambda:app.poll()is not None);check(app.returncode==0,'Actual document closes after workers, graphics and ordered file work drain');app=None
 window,layout,command=await launch(1);check(inspect()==expected,'Fresh ordinary application reopens the accepted editable native payload exactly');picture(window,'reopened.png')
 await menu_state(layout,'Undo',False);await menu_state(layout,'Redo',False)
 await command('Close Window',pause=0);await wait(lambda:app.poll()is not None);check(app.returncode==0,'Fresh application closes normally without creating spurious history');app=None
 check(not[p for p,v in pins.items()if sha(p)!=v],'All application/inspector/fixture/expected/script inputs remain unchanged')
 d.close();bus.disconnect();report(True)
try:asyncio.run(asyncio.wait_for(main(),900))
except BaseException as failure:
 diagnostic_snapshot('exception');report(problem=repr(failure));raise
finally:
 for process in reversed(processes):
  if process.poll()is None:
   process.terminate()
   try:process.wait(timeout=10)
   except subprocess.TimeoutExpired:process.kill();process.wait()
