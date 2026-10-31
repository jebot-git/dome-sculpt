#!/usr/bin/env python3
"""Actual native FLTK/universal-menu selection state, permissions and visibility."""
import asyncio,json,os,signal,subprocess,sys,tempfile
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_SELECTION_MENU_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_SELECTION_MENU_BUS':'1'})
root=Path(__file__).resolve().parents[1];compact=os.environ.get('DOME_SELECTION_COMPACT')=='1'
out=root/'artifacts'/('selection-menu-compact' if compact else 'selection-menu');out.mkdir(parents=True,exist_ok=True)
checks=[];processes=[]
def check(ok,text):
 checks.append({'passed':bool(ok),'check':text});print(('PASS ' if ok else 'FAIL ')+text,flush=True)
 (out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks)},indent=2)+'\n');assert ok,text
async def wait(f,seconds=45):
 for _ in range(int(seconds*10)):
  result=f()
  if result:return result
  await asyncio.sleep(.1)
 raise AssertionError('Native UI condition timed out')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-held-ui-') as home:
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','640x360x24' if compact else '1280x900x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
  with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
  env={**os.environ,'HOME':home,'XDG_STATE_HOME':home+'/state','XDG_CONFIG_HOME':home+'/config','XDG_CACHE_HOME':home+'/cache','XDG_DATA_HOME':home+'/data','XDG_RUNTIME_DIR':home,'DISPLAY':screen,'XDG_CURRENT_DESKTOP':'Dome','FLTK_BACKEND':'x11','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for name in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(name,None)
  if compact:
   appearance=Path(home,'config/dome/appearance.json');appearance.parent.mkdir(parents=True);appearance.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  fixture=Path(home,'held.dsculpt');subprocess.run([str(root/'build/dome-sculpt-object-isolation-test'),'--ui-fixture',str(fixture)],check=True)
  app=subprocess.Popen([str(root/'build/dome-sculpt'),str(fixture)],env=env,stdout=(out/'app.log').open('w'),stderr=subprocess.STDOUT);processes.append(app);d=display.Display(screen)
  def named(text):
   for window in d.screen().root.query_tree().children:
    try:
     if text in str(window.get_wm_name() or ''):return window
    except error.BadWindow:pass
  window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  def prop(name):
   value=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(value.value).decode() if value else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'))
  proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));menu=proxy.get_interface('com.canonical.dbusmenu')
  async def layout():
   result=await menu.call_get_layout(0,-1,[]);nodes={}
   def walk(node):
    ident,props,children=node;props={k:v.value for k,v in props.items()};nodes[props.get('label','').replace('_','')]=(ident,props)
    for child in children:walk(child.value)
   walk(result[1]);return nodes
  async def ready(predicate):
   for _ in range(450):
    nodes=await layout()
    if predicate(nodes):return nodes
    await asyncio.sleep(.1)
   raise AssertionError('Native menu state timed out')
  async def click(label):
   nodes=await layout();stamp=fixture.stat().st_mtime_ns if label=='Save' else None
   await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
   if stamp is not None:await wait(lambda:fixture.stat().st_mtime_ns!=stamp)
   await asyncio.sleep(.25)
  def key(name):
   code=d.keysym_to_keycode(XK.string_to_keysym(name));xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code);d.sync()
  def mouse(x,y,pressed=None):
   g=window.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y))
   if pressed is not None:xtest.fake_input(d,X.ButtonPress if pressed else X.ButtonRelease,1)
   d.sync()
  def pixels():
   g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF);im=Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX');return im.crop((g.width//4,g.height//4,g.width*3//4,g.height*3//4))
  def info():return json.loads(subprocess.check_output([str(root/'build/dome-sculpt'),'--project-info',str(fixture)],env=env))
  def inspect():return json.loads(subprocess.check_output([str(root/'build/dome-sculpt-object-isolation-test'),'--inspect',str(fixture)],env=env))
  await ready(lambda n:n['Use painted mesh'][1].get('toggle-state')==1 and n['Save'][1].get('enabled'))
  await asyncio.sleep(.7);baseline=info();original=inspect();base_pixels=None if compact else pixels();
  if not compact:base_pixels.save(out/'ordinary.png')
  nodes=await layout();check(nodes['VR selection mode'][1].get('toggle-state')==0 and nodes['Focus selected objects'][1].get('toggle-state')==0,'Native menu begins with ordinary editing and explicit VR-selection toggle')
  await click('VR selection mode');nodes=await ready(lambda n:n['VR selection mode'][1].get('toggle-state')==1 and n['XR object hierarchy'][1].get('toggle-state')==1)
  check(nodes['Focus selected objects'][1].get('toggle-state')==1,'Native selection command synchronizes hierarchy and focused-selection check states')
  if not compact:
   await wait(lambda:pixels().tobytes()!=base_pixels.tobytes());pixels().save(out/'selected-with-ghost.png')
  await click('Save');check(info()['scene_hash']==baseline['scene_hash'],'Selection and ghost cues save no material, topology, paint or visibility overrides')
  await click('VR selection mode');nodes=await ready(lambda n:n['VR selection mode'][1].get('toggle-state')==0)
  check(nodes['Focus selected objects'][1].get('toggle-state')==1,'Turning VR selection off retains focused editing and transparent context')
  await click('Next object');nodes=await ready(lambda n:n['Object and material...'][1].get('enabled') is True)
  check(nodes['Use painted mesh'][1].get('enabled') is True,'Existing Next-object menu command selects an editable active member instead of a read-only ghost')
  await click('Save');check(inspect()==original,'Selecting the next member preserves all original object data')
  await click('Visible');await click('Save');changed=inspect();check(changed[0]==original[0] and changed[1]['visible'] is False,'Selected member visibility changes only its requested object')
  await click('Undo');await click('Save');check(inspect()==original,'Visibility undo restores exact original object data while selection stays focused')
  if not compact:ghosted=pixels()
  await click('Focus selected objects');nodes=await ready(lambda n:n['Focus selected objects'][1].get('toggle-state')==0)
  check(nodes['Object and material...'][1].get('enabled') is True,'All-solid focus toggle retains a valid active editing target')
  if not compact:
   await wait(lambda:pixels().tobytes()!=ghosted.tobytes());pixels().save(out/'all-solid.png')
  await click('XR object hierarchy');nodes=await ready(lambda n:n['XR object hierarchy'][1].get('toggle-state')==0)
  check(nodes['VR selection mode'][1].get('toggle-state')==0,'Hierarchy closes independently without re-entering selection')
  await click('VR selection mode');nodes=await ready(lambda n:n['VR selection mode'][1].get('toggle-state')==1 and n['XR object hierarchy'][1].get('toggle-state')==1)
  check(nodes['Focus selected objects'][1].get('toggle-state')==1,'Re-entering selection restores its synchronized hierarchy and ghost focus')
  await click('Save');check(inspect()==original,'Full native selection workflow preserves exact source objects')
  d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
