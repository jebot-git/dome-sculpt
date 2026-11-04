#!/usr/bin/env python3
"""Real FLTK linked pose on completed native19; read-only App observer."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time,shutil,hashlib
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_POSE_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_POSE_BUS':'1'})
ROOT=Path(__file__).resolve().parents[1];BUILD=Path(os.environ.get('DOME_POSE_BUILD',ROOT/'artifacts/ui'));APP=Path(os.environ.get('DOME_POSE_APP',BUILD/'dome-sculpt-ui'));INSPECT=Path(os.environ.get('DOME_POSE_INSPECT',BUILD/'inspect')); OUT=Path(os.environ['DOME_POSE_OUT']);OUT.mkdir(parents=True,exist_ok=True);COMPACT=os.environ.get('DOME_POSE_COMPACT')=='1';processes=[];checks=[];events=[];started=time.monotonic()
def check(v,label):
 checks.append({'passed':bool(v),'check':label});(OUT/'report.json').write_text(json.dumps({'checks':checks},indent=2));print(('PASS ' if v else 'FAIL ')+label,flush=True);assert v,label
async def wait(fn,seconds=90):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  try:v=fn()
  except (FileNotFoundError,json.JSONDecodeError):v=None
  if v:return v
  await asyncio.sleep(.04)
 raise TimeoutError('Actual pose GUI state timeout')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='native-linked-pose-') as home:
  read,write=os.pipe();log=(OUT/'xvfb.log').open('w');processes.append(subprocess.Popen([os.environ['DOME_POSE_XVFB'],'-displayfd',str(write),'-screen','0','640x360x24' if COMPACT else '1280x1000x24','-nolisten','tcp'],pass_fds=(write,),stdout=log,stderr=log));os.close(write)
  with os.fdopen(read) as s:screen=':'+s.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none','XR_RUNTIME_JSON':'/nonexistent/no-physical-runtime.json'}
  for k in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(k,None)
  for k,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[k]=home+'/'+suffix;Path(env[k]).mkdir(mode=0o700)
  if COMPACT:
   f=Path(home,'config/dome/appearance.json');f.parent.mkdir(parents=True,exist_ok=True);f.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  project=Path(home,'actual19.dsculpt');observe=OUT/'live.json';helper=str(INSPECT);subprocess.run([helper,'--prepare',os.environ['DOME_POSE_INPUT'],str(project)],env=env,check=True)
  def inspect():return json.loads(subprocess.check_output([helper,'--inspect',str(project)],env=env))
  before=inspect();expected=json.loads(subprocess.check_output([helper,'--expected',str(project),'30'],env=env));(OUT/'before.json').write_text(json.dumps(before,indent=2));(OUT/'expected.json').write_text(json.dumps(expected,indent=2))
  def observed():return json.loads(observe.read_text())
  app=subprocess.Popen([str(APP),str(project),str(observe)],env=env,cwd=home,stdout=(OUT/'app.log').open('w'),stderr=subprocess.STDOUT);processes.append(app);d=display.Display(screen)
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if title in str(w.get_wm_name() or '') and w.get_attributes().map_state==X.IsViewable:return w
    except error.BadWindow:pass
  window=await wait(lambda:named('Dome Sculpt'));await wait(lambda:observed()['active']==before['target_ids'][0]);window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  maps=await wait(lambda:(m if 'libgallium' in (m:=Path('/proc',str(app.pid),'maps').read_text()) and 'libGLX_mesa' in m else None));(OUT/'app-maps.txt').write_text(maps);check('libgallium' in maps and 'libGLX_mesa' in maps,'Actual viewed FLTK app maps software Gallium and its GLX frontend');driver_paths=sorted({line.split()[-1] for line in maps.splitlines() if ('libgallium' in line or 'libGLX_mesa' in line) and line.split()[-1].startswith('/')});(OUT/'graphics-inputs.json').write_text(json.dumps({f:hashlib.sha256(Path(f).read_bytes()).hexdigest() for f in driver_paths},indent=2))
  def prop(name):
   p=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));menu=proxy.get_interface('com.canonical.dbusmenu')
  async def layout():
   nodes={}
   def walk(node):
    ident,props,children=node;props={k:v.value for k,v in props.items()};nodes[props.get('label','').replace('_','')]=(ident,props)
    for child in children:walk(child.value)
   walk((await menu.call_get_layout(0,-1,[]))[1]);return nodes
  async def command(label):
   deadline=time.monotonic()+90;nodes=await layout()
   while (label not in nodes or not nodes[label][1].get('enabled',True)) and time.monotonic()<deadline:
    await asyncio.sleep(.04);nodes=await layout()
   check(label in nodes and nodes[label][1].get('enabled',True),'Actual universal menu enables '+label);events.append({'label':label,'state':{k:v for k,v in observed().items() if k!='windows'}});(OUT/'commands.json').write_text(json.dumps(events,indent=2));await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0);await asyncio.sleep(.1)
  async def save():
   old=project.stat().st_mtime_ns;await command('Save');await wait(lambda:project.stat().st_mtime_ns!=old);await wait(lambda:not observed()['saving'] and not observed()['busy'] and not observed()['queued']);check(not observed()['error'],'Native Save acknowledged with no worker error')
  def key(name,down=None):
   for event in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,event,d.keysym_to_keycode(XK.string_to_keysym(name)))
   d.sync()
  def tap(w,x,y):
   g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
  def state_dialog():return next(w for w in observed()['windows'] if w['title']=='Rig and shapekeys')
  def widget(label):return next(w for w in state_dialog()['widgets'] if w['label']==label)
  def capture(w,name):
   g=w.get_geometry();raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(OUT/name)
  async def stage():
   await command('Rig and shapekeys...');dialog=await wait(lambda:named('Rig and shapekeys'));await wait(lambda:widget('Joint'));j=widget('Joint');hint=widget('Add local joint rotation or translation.\nApply is one Undo; Cancel keeps the pose.');check(j['y']>=hint['y']+hint['h'],'Measured help text and Joint control do not overlap');tap(dialog,j['x']+j['w']/2,j['y']+j['h']/2);await asyncio.sleep(.12);key('Home')
   for _ in range(before['head']):key('Down');await asyncio.sleep(.02)
   key('Return');await wait(lambda:widget('Joint')['value']==before['head']);check(widget('Joint')['value']==before['head'],'Actual native dropdown selects the authenticated Head index')
   g=dialog.get_geometry();rz=widget('Rotate Z (degrees)');row=rz['h']+4;viewport=g.height-row*2
   for _ in range(30):
    rz=widget('Rotate Z (degrees)')
    if 0<=rz['y'] and rz['y']+rz['h']<=viewport:break
    xtest.fake_input(d,X.MotionNotify,x=g.x+g.width-8,y=g.y+viewport//2);xtest.fake_input(d,X.ButtonPress,5);xtest.fake_input(d,X.ButtonRelease,5);d.sync();await asyncio.sleep(.06)
   check(0<=rz['y'] and rz['y']+rz['h']<=viewport,'Rotation input remains inside actual native scroll viewport');tap(dialog,rz['x']+rz['w']/2,rz['y']+rz['h']/2);key('Control_L',True);key('a');key('Control_L',False);key('3');key('0');key('Tab');await wait(lambda:widget('Rotate Z (degrees)')['value']==30);capture(dialog,'staged-'+str(len(checks))+'.png');return dialog,row
  dialog,row=await stage();key('Escape');await wait(lambda:not named('Rig and shapekeys'));await save();check(inspect()==before and not observed()['undo'],'Escape cancels staged linked pose without a Scene or Undo change')
  dialog,row=await stage();g=dialog.get_geometry();tap(dialog,g.width/2+80,g.height-row*2+8+row/2);await wait(lambda:not named('Rig and shapekeys'));await save();actual=inspect();(OUT/'actual.json').write_text(json.dumps(actual,indent=2));check(actual==expected,'Actual FLTK Apply plus ordered Save equals all19 independently scoped poses');check(actual['without_pose']==before['without_pose'],'All non-pose mesh paint source origin material and group values stay exact');check(sum(a['pose']!=b['pose'] for a,b in zip(actual['poses'],before['poses']))==19,'Actual GUI changes all19 native parts across both sibling groups')
  await command('Undo');await save();check(inspect()==before and not observed()['undo'],'Exactly one native Undo restores entire pre-pose Scene')
  await command('Redo');await save();check(inspect()==expected,'Redo and saved restart restore all19 exact linked poses')
  dialog,row=await stage();key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:not named('Rig and shapekeys'));await save();check(inspect()==expected,'Universal CtrlW cancels the modal pose edit without changing the accepted Scene')
  shutil.copy2(project,OUT/'accepted.dsculpt');await command('Close Window');await wait(lambda:app.poll() is not None);check(app.returncode==0,'Actual native app closes through its normal worker/GL/toolkit teardown');d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 (OUT/'timing.json').write_text(json.dumps({'elapsed':time.monotonic()-started}))
 for p in reversed(processes):
  if p.poll() is None:
   p.terminate()
   try:p.wait(timeout=8)
   except subprocess.TimeoutExpired:p.kill();p.wait()
