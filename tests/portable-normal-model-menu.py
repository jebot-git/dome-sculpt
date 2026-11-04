#!/usr/bin/env python3
"""Actual portable-normal authoring labels, native bake settings and reversible conversion."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_CANONICAL_GUI_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_CANONICAL_GUI_BUS':'1'})
app_path=Path(os.environ['DOME_CANONICAL_APP']);helper=Path(os.environ['DOME_CANONICAL_HELPER']);out=Path(os.environ['DOME_CANONICAL_OUT']);compact=os.environ.get('DOME_CANONICAL_COMPACT')=='1';out.mkdir(parents=True,exist_ok=True)
checks=[];processes=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'physical_headset':False},indent=2)+'\n');print(('PASS 'if ok else'FAIL ')+label,flush=True);assert ok,label
async def wait(fn,seconds=60):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  v=fn()
  if v:return v
  await asyncio.sleep(.04)
 raise TimeoutError('Actual canonical normal FLTK client did not reach expected state')
def stop():
 for p in reversed(processes):
  if p.poll()is None:
   p.terminate()
   try:p.wait(timeout=5)
   except subprocess.TimeoutExpired:p.kill();p.wait()
class PrivateHome(tempfile.TemporaryDirectory):
 def __exit__(self,*args):stop();return super().__exit__(*args)
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with PrivateHome(prefix='dome-canonical-menu-')as home:
  read,write=os.pipe();log=(out/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(write),'-screen','0','640x360x24'if compact else'1280x1000x24','-nolisten','tcp'],pass_fds=(write,),stdout=log,stderr=log));os.close(write)
  with os.fdopen(read)as f:screen=':'+f.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for name in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(name,None)
  for key,part in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[key]=home+'/'+part;Path(env[key]).mkdir(mode=0o700)
  if compact:
   p=Path(home,'config/dome/appearance.json');p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  project=Path(home,'canonical.dsculpt');subprocess.run([str(helper),'--ui-fixture',str(project)],env=env,check=True)
  def inspect():return json.loads(subprocess.check_output([str(helper),'--inspect',str(project)],env=env,text=True))
  original=inspect()['scene'];log=(out/'app.log').open('w');app=subprocess.Popen([str(app_path),str(project)],cwd=home,env=env,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
  def named():
   for w in d.screen().root.query_tree().children:
    try:
     if 'Dome Sculpt'in str(w.get_wm_name()or'')and w.get_attributes().map_state==X.IsViewable:return w
    except error.BadWindow:pass
  window=await wait(named);window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  def prop(name):
   v=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(v.value).decode()if v else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'))
  proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));menu=proxy.get_interface('com.canonical.dbusmenu')
  async def layout():
   result=await menu.call_get_layout(0,-1,[]);nodes={}
   def visit(node):
    ident,properties,children=node;properties={k:v.value for k,v in properties.items()};nodes[properties.get('label','').replace('_','')]=(ident,properties)
    for c in children:visit(c.value)
   visit(result[1]);return nodes
  async def command(label):
   for _ in range(1500):
    nodes=await layout()
    if label in nodes and nodes[label][1].get('enabled',True):break
    await asyncio.sleep(.02)
   else:raise TimeoutError(label+' remained unavailable')
   before=project.stat().st_mtime_ns;await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
   if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=before)
   await asyncio.sleep(.1)
  def chord(*keys):
   for key in keys:xtest.fake_input(d,X.KeyPress,d.keysym_to_keycode(XK.string_to_keysym(key)))
   for key in reversed(keys):xtest.fake_input(d,X.KeyRelease,d.keysym_to_keycode(XK.string_to_keysym(key)))
   d.sync()
  def capture(name):
   g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/name)
  def bake_window():
   for w in d.screen().root.query_tree().children:
    try:
     if w.get_wm_name()=='Bake surface'and w.get_attributes().map_state==X.IsViewable:return w
    except error.BadWindow:pass
  await command('Bake surface...');dialog=await wait(bake_window);dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();await asyncio.sleep(.3)
  check(dialog.get_wm_transient_for().id==window.id,'Portable normal authoring settings retain native modal and universal application ownership')
  for step in range(6):
   g=dialog.get_geometry();raw=dialog.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/('bake-settings-'+str(step)+'.png'))
   xtest.fake_input(d,X.MotionNotify,x=g.x+g.width-10,y=g.y+g.height//3)
   for _ in range(3):xtest.fake_input(d,X.ButtonPress,5);xtest.fake_input(d,X.ButtonRelease,5)
   d.sync();await asyncio.sleep(.12)
  chord('Control_L','w');await wait(lambda:bake_window()is None);await command('Save')
  check(inspect()['scene']==original,'DomeD Ctrl+W cancels native model/quality settings with the complete source scene exact')
  window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  nodes=await layout();check(nodes['Convert to portable normals'][1].get('enabled',True),'Actual DomeD universal menu exposes enabled native normal conversion')
  capture('before.png');await command('Convert to portable normals');await command('Save');accepted=inspect();scene=accepted['scene']
  check(accepted['converted']and scene['version']==33,'Real menu callback and ordered native Save publish a replayable Scene33 conversion')
  m=scene['source_materials'];anchor=scene['baked_normal_fields']['origins'][0];raw=m['paints'][anchor['paint']]
  check(raw==original['objects'][0]['painted'],'GUI conversion retains the exact complete original canonical paint')
  check(scene['objects'][1]==original['objects'][1],'Complete hidden source survives the actual native action unchanged')
  target=scene['objects'][0];old=original['objects'][0]
  check(target['painted']['layers']==old['painted']['layers']and target['deformation']==old['deformation']and target['bake_recipe']==old['bake_recipe'],'Editable sparse layers, skin/morph and saved bake recipe remain exact')
  check(all(target['painted']['baked'][name]==old['painted']['baked'][name]for name in ['color','orm','emission','coverage']),'All other baked channels and coverage survive actual GUI conversion')
  nodes=await layout();check(not nodes['Convert to portable normals'][1].get('enabled',True),'Already converted target disables duplicate conversion in its actual menu')
  capture('converted.png');chord('Control_L','z');await asyncio.sleep(.3);await command('Save');check(inspect()['scene']==original,'Standard Ctrl+Z restores the whole original project in one step')
  chord('Control_L','Shift_L','z');await asyncio.sleep(.3);await command('Save');check(inspect()['scene']==scene,'Standard Ctrl+Shift+Z restores the exact converted project')
  await command('Close Window');await wait(lambda:app.poll()is not None);check(app.returncode==0,'Native window command drains workers and closes graphics/fonts normally')
  d.close();bus.disconnect()
try:asyncio.run(main())
finally:stop()
