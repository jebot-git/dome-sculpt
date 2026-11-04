#!/usr/bin/env python3
"""Real FLTK cage choices/callbacks through native input and universal menu."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_CAGE_MENU_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_CAGE_MENU_BUS':'1'})
r=Path(__file__).resolve().parents[1];app_path=Path(os.environ['DOME_CAGE_APP']);helper=Path(os.environ['DOME_CAGE_HELPER']);probe=Path(os.environ['DOME_CAGE_PROBE']);out=Path(os.environ['DOME_CAGE_OUT']);compact=os.environ.get('DOME_CAGE_COMPACT')=='1';out.mkdir(parents=True,exist_ok=True);checks=[];processes=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks},indent=2)+'\n');print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
async def wait(fn,seconds=30):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=fn()
  if value:return value
  await asyncio.sleep(.04)
 raise TimeoutError('Actual native cage client did not reach expected state')
def stop_processes():
 for process in reversed(processes):
  if process.poll() is None:
   process.terminate()
   try:process.wait(timeout=5)
   except subprocess.TimeoutExpired:process.kill();process.wait()
class PrivateHome(tempfile.TemporaryDirectory):
 def __exit__(self,*args):
  stop_processes()
  return super().__exit__(*args)
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with PrivateHome(prefix='dome-cage-menu-') as home:
  read,write=os.pipe();log=(out/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','640x360x24' if compact else '1280x1000x24','-nolisten','tcp'],pass_fds=(write,),stdout=log,stderr=log));os.close(write)
  with os.fdopen(read) as f:screen=':'+f.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for k in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(k,None)
  for k,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[k]=home+'/'+suffix;Path(env[k]).mkdir(mode=0o700)
  if compact:
   p=Path(home,'config/dome/appearance.json');p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  project=Path(home,'cage.dsculpt');subprocess.run([str(helper),'--ui-fixture',str(project)],check=True,env=env)
  def inspect():return json.loads(subprocess.check_output([str(helper),'--inspect',str(project)],env=env))
  original=inspect()['scene'];report=Path(home,'widgets.json');log=(out/'app.log').open('w')
  app=subprocess.Popen([str(app_path),str(project)],cwd=home,env={**env,'LD_PRELOAD':((os.environ['DOME_CAGE_ASAN_RUNTIME']+':') if os.environ.get('DOME_CAGE_ASAN_RUNTIME') else '')+str(probe),'DOME_CAGE_WIDGET_REPORT':str(report)},stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if title in str(w.get_wm_name() or '') and w.get_attributes().map_state==X.IsViewable:return w
    except error.BadWindow:pass
  window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  def prop(name):
   value=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(value.value).decode() if value else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));menu=proxy.get_interface('com.canonical.dbusmenu')
  async def layout():
   result=await menu.call_get_layout(0,-1,[]);nodes={}
   def walk(node):
    ident,properties,children=node;properties={k:v.value for k,v in properties.items()};nodes[properties.get('label','').replace('_','')]=(ident,properties)
    for child in children:walk(child.value)
   walk(result[1]);return nodes
  async def command(label):
   for _ in range(500):
    nodes=await layout()
    if nodes[label][1].get('enabled',True):break
    await asyncio.sleep(.02)
   else:raise TimeoutError(label+' remained disabled')
   old=project.stat().st_mtime_ns;await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
   if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=old)
   await asyncio.sleep(.1)
  def widgets():
   try:return json.loads(report.read_text())
   except (FileNotFoundError,json.JSONDecodeError):return []
  def find(label):
   def walk(w):
    if w['label']==label:return w
    for c in w.get('children',[]):
     result=walk(c)
     if result:return result
   for w in widgets():
    if w['label']=='Bake surface':
     found=walk(w)
     if found:return found
  def tap(w,x,y,button=1):
   g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,button);xtest.fake_input(d,X.ButtonRelease,button);d.sync()
  def key(name,down=None):
   for event in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,event,d.keysym_to_keycode(XK.string_to_keysym(name)))
   d.sync()
  def capture(w,name):
   g=w.get_geometry();raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/name)
  async def open_options():
   await command('Bake surface...');dialog=await wait(lambda:named('Bake surface'));dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();await wait(lambda:find('Cage mesh'));return dialog
  async def visible(dialog,label):
   for _ in range(35):
    item=await wait(lambda:find(label));g=dialog.get_geometry();viewport=g.height-(48 if compact else 36)*2
    if item['y']>=2 and item['y']+item['h']<=viewport-2:return item
    tap(dialog,5,viewport/2,5 if item['y']+item['h']>viewport else 4);await asyncio.sleep(.1)
   raise TimeoutError('Native scroll never exposed '+label)
  async def choose_cage(dialog):
   item=await visible(dialog,'Cage mesh');tap(dialog,item['x']+item['w']/2,item['y']+item['h']/2);await asyncio.sleep(.15);key('Down');await asyncio.sleep(.08);key('Down');await asyncio.sleep(.08);key('Return');selected=await wait(lambda:find('Cage mesh') if find('Cage mesh') and find('Cage mesh').get('choice')==2 else None)
   check('envelope' in selected.get('selected',''),'Actual native FLTK choice selects the hidden Unicode cage mesh')
  dialog=await open_options();g=dialog.get_geometry();check(g.width<=600 if compact else g.width<1280,'Cage options fit the native work area');check(find('Cage mesh')['choice']==0,'Normal-offset projection remains the explicit default')
  await choose_cage(dialog);capture(dialog,'cage-choice.png');key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:not named('Bake surface'));await command('Save');check(inspect()['scene']==original,'Universal Close cancels local cage selection with complete Scene unchanged')
  dialog=await open_options();check(find('Cage mesh')['choice']==0,'Cancelled native cage choice was never committed');await choose_cage(dialog)
  item=await visible(dialog,'Front (mm)');old=item['value']
  for _ in range(18):tap(dialog,item['x']+item['w']*.75,item['y']+item['h']/2,4)
  await wait(lambda:find('Front (mm)')['value']>old+.02);check(find('Front (mm)')['value']>old+.02,'Actual slider wheel input expands the projection bound without scrolling a hidden control')
  capture(dialog,'cage-ready.png');button=await wait(lambda:find('Bake'));tap(dialog,button['x']+button['w']/2,button['y']+button['h']/2)
  await wait(lambda:'complete' in str(named('Bake surface').get_wm_name()).lower() if named('Bake surface') else False,90)
  check(True,'Actual native cage Bake completes through the guarded background worker');capture(dialog,'cage-complete.png');key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:not named('Bake surface'));await command('Save');after=inspect();check(after['baked'],'Native cage result persists through the immediately ordered Save')
  scene=after['scene'];check(scene['objects'][1:]==original['objects'][1:] and scene['objects'][0]['painted']['layers']==original['objects'][0]['painted']['layers'],'Complete native source/cage and editable target paint remain exact')
  color=scene['objects'][0]['painted']['baked']['color']['bytes'];check(all(color[i:i+4]==[0,0,255,255] for i in range(0,len(color),4)),'Every native baked pixel matches the independent off-axis source')
  await command('Undo');await command('Save');check(inspect()['scene']==original,'Universal Undo restores the complete original scene in one step');await command('Redo');await command('Save');check(inspect()['scene']==scene,'Universal Redo restores the exact custom-cage endpoint')
  await command('Close Window');await wait(lambda:app.poll() is not None);check(app.returncode==0,'Native app closes through normal worker, graphics and font teardown');d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:
   p.terminate()
   try:p.wait(timeout=5)
   except subprocess.TimeoutExpired:p.kill();p.wait()
