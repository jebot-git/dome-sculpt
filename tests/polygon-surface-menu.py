#!/usr/bin/env python3
"""Ordinary FLTK paint→owning polygon→Save/reopen/Restore; private X11 display."""
import asyncio,hashlib,json,os,signal,subprocess,sys,time
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_FIELD_MENU_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_FIELD_MENU_BUS':'1'})
root=Path(__file__).resolve().parents[1]
build=Path(os.environ['DOME_FIELD_MENU_BUILD']).resolve();inspector=Path(os.environ['DOME_FIELD_MENU_INSPECTOR']).resolve();fixture=Path(os.environ['DOME_FIELD_MENU_FIXTURE']).resolve();out=Path(os.environ['DOME_FIELD_MENU_OUT']).resolve();out.mkdir(parents=True,exist_ok=True)
processes=[];checks=[];app=None
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def report(complete=False,problem=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':problem,'checks':checks,'count':len(checks),'inputs':{str(p):sha(p) for p in [build/'dome-sculpt',inspector,fixture]},'scope':'Actual ordinary FLTK/universal-menu and pointer events on private X11; no physical headset'},indent=2)+'\n')
def check(value,label):
 checks.append({'passed':bool(value),'label':label});report();print(('PASS ' if value else 'FAIL ')+label,flush=True);assert value,label
async def wait(fn,seconds=50):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=fn()
  if value:return value
  if app is not None and app.poll() is not None:raise RuntimeError(('ordinary app exited',app.returncode))
  await asyncio.sleep(.05)
 raise TimeoutError('Actual FLTK workflow did not reach its expected state')
async def main():
 global app
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 home=out/'home';home.mkdir();rd,wr=os.pipe();xlog=(out/'xvfb.log').open('w');xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1100x800x24','-nolisten','tcp'],pass_fds=(wr,),stdout=xlog,stderr=xlog);processes.append(xvfb);os.close(wr)
 with os.fdopen(rd) as p:screen=':'+p.readline().strip()
 env={**os.environ,'HOME':str(home),'DISPLAY':screen,'XDG_CURRENT_DESKTOP':'Dome','FLTK_BACKEND':'x11','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
 for name in ['WAYLAND_DISPLAY','WAYLAND_SOCKET','__EGL_VENDOR_LIBRARY_FILENAMES','LIBGL_DRIVERS_PATH','MESA_LOADER_DRIVER_OVERRIDE']:env.pop(name,None)
 for name,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[name]=str(home/suffix);Path(env[name]).mkdir(mode=0o700)
 project=home/'actual.dsculpt';project.write_bytes(fixture.read_bytes());d=display.Display(screen)
 def inspect():return json.loads(subprocess.check_output([str(inspector),str(project)],env=env))
 def named(title):
  for w in d.screen().root.query_tree().children:
   try:
    if title in str(w.get_wm_name() or ''):return w
   except error.BadWindow:pass
 def key(name,down=None):
  code=d.keysym_to_keycode(XK.string_to_keysym(name))
  for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,code)
  d.sync()
 def mouse(w,x,y,down=None):
  g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y))
  if down is not None:xtest.fake_input(d,X.ButtonPress if down else X.ButtonRelease,1)
  d.sync()
 async def launch(number):
  global app
  log=(out/('app-'+str(number)+'.log')).open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],env=env,cwd=home,stdout=log,stderr=log);processes.append(app);w=await wait(lambda:named('Dome Sculpt'));w.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
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
   for _ in range(1000):
    nodes=await layout()
    if label in nodes and nodes[label][1].get('enabled',True):break
    await asyncio.sleep(.05)
   else:raise AssertionError(('Actual universal menu command disabled',label))
   with (out/'commands.jsonl').open('a') as f:f.write(json.dumps({'label':label,'time':time.monotonic()})+'\n')
   await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0);await asyncio.sleep(pause)
  (out/('runtime-maps-'+str(number)+'.txt')).write_text(Path('/proc/'+str(app.pid)+'/maps').read_text());return w,layout,command
 async def save(command):
  stamp=project.stat().st_mtime_ns;await command('Save');await wait(lambda:project.stat().st_mtime_ns!=stamp);return inspect()
 async def begin(w,command,cancel=False):
  await command('Start polygon editing');await asyncio.sleep(.3);dialog=named('Convert to editable polygons')
  if dialog:
   check(dialog.get_wm_transient_for().id==w.id,'Actual conversion confirmation belongs to its native document')
   dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
   if cancel:key('Return')
   else:
    g=dialog.get_geometry();row=36;button=min(g.width//2-24,max(140,16*9));mouse(dialog,g.width/2+10+button/2,g.height-1.5*row+8,True);mouse(dialog,g.width/2+10+button/2,g.height-1.5*row+8,False)
   await wait(lambda:named('Convert to editable polygons') is None)
  elif cancel:await command('Cancel polygon edit')
 w,layout,command=await launch(0);original=inspect();await command('Frame all');await command('Base color');await command('Add layer');layered=await save(command);check(layered!=original,'Actual FLTK Add layer publishes a writable native paint endpoint')
 g=w.get_geometry();raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);im=Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX');im.save(out/'before-paint.png');points=[]
 for y in range(int(g.height*.25),int(g.height*.75)):
  for x in range(int(g.width*.3),int(g.width*.7)):
   rgb=im.getpixel((x,y))
   if max(rgb)>110 and max(rgb)-min(rgb)>35 and all(max(im.getpixel((x+dx,y+dy)))>100 for dx,dy in [(3,0),(-3,0),(0,3),(0,-3)]):points.append(((x-g.width/2)**2+(y-g.height/2)**2,x,y))
 check(bool(points),'Native source is actually visible at the chosen pointer paint location');_,x,y=min(points);w.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Shift_L',True);mouse(w,x,y,True);await asyncio.sleep(.15)
 for dx in [2,4,6]:mouse(w,x+dx,y);await asyncio.sleep(.05)
 mouse(w,x+6,y,False);key('Shift_L',False);await asyncio.sleep(.5);painted=await save(command);check(painted!=layered,'Actual ordinary FLTK pointer gesture changes paint before polygon editing')
 before=next(o for o in layered['objects'] if o['id']==layered['active']);after=next(o for o in painted['objects'] if o['id']==painted['active']);check(before['painted']!=after['painted'] and all(before.get(k)==after.get(k) for k in ['material_bindings','appearance','deformation','polygon','shape_source','rig_source']),'Pointer paint retains its independent normal/key/source/rig context');check(painted['source_models']==layered['source_models'],'Pointer paint retains exact original model bytes and resources')
 await begin(w,command,True);check(await save(command)==painted,'Return-default conversion cancellation retains complete painted state')
 await begin(w,command);await command('Select all components');await command('Flip faces');await command('Cancel polygon edit');check(await save(command)==painted,'Real draft Flip Cancel preserves complete paint and original normal graph')
 await begin(w,command);await command('Select all components');await command('Flip faces');await command('Apply polygon edit',0);accepted=await save(command);active=next(o for o in accepted['objects'] if o['id']==accepted['active']);check(accepted['version']==39 and accepted['source_materials']['version']==21 and active['painted']['version']==6 and accepted['polygon_surfaces']['nodes'],'Actual FLTK Apply followed by Save emits the owning Scene39/material21/Paint6 graph')
 await command('Undo');check(await save(command)==painted,'One native global Undo restores complete pre-polygon paint and source state');await command('Redo');check(await save(command)==accepted,'One native global Redo restores the exact accepted field graph')
 await command('Close Window');await asyncio.sleep(.2);check(app.wait(timeout=15)==0,'Ordinary native application closes cleanly after Save');app=None
 w,layout,command=await launch(1);check(await save(command)==accepted,'Fresh ordinary application opens and saves the exact current field graph');await command('Restore polygon source');check(await save(command)==painted,'Restarted FLTK source Restore recovers exact painted normal/key/source state');await command('Undo');check(await save(command)==accepted,'Restarted Restore Undo recovers exact current field');await command('Redo');check(await save(command)==painted,'Restarted Restore Redo recovers exact original source')
 await command('Close Window');await asyncio.sleep(.2);check(app.wait(timeout=15)==0,'Reopened ordinary application closes cleanly');app=None;report(True);d.close();bus.disconnect()
try:asyncio.run(main())
except BaseException as e:report(problem=repr(e));raise
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
