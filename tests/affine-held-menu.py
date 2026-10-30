#!/usr/bin/env python3
"""Actual Scene13 affine FLTK/universal-menu hold, cancel, commit and Undo flow."""
import asyncio,json,os,signal,subprocess,sys,tempfile
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_HELD_MENU_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_HELD_MENU_BUS':'1'})
root=Path(__file__).resolve().parents[1];BUILD=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',str(root/'build-held')));compact=os.environ.get('DOME_HELD_COMPACT')=='1'
out=root/'artifacts'/('affine-held-menu-compact' if compact else 'affine-held-menu');out.mkdir(parents=True,exist_ok=True)
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
  fixture=Path(home,'held.dsculpt');subprocess.run([str(BUILD/'dome-sculpt-affine-held-test'),'--ui-fixture',str(fixture)],check=True)
  app=subprocess.Popen([str(BUILD/'dome-sculpt'),str(fixture)],env=env,stdout=(out/'app.log').open('w'),stderr=subprocess.STDOUT);processes.append(app);d=display.Display(screen)
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
  def info():return json.loads(subprocess.check_output([str(BUILD/'dome-sculpt'),'--project-info',str(fixture)],env=env))
  def inspect():return json.loads(subprocess.check_output([str(BUILD/'dome-sculpt-affine-held-test'),'--inspect',str(fixture)],env=env))
  await ready(lambda n:n['Use painted mesh'][1].get('toggle-state')==1 and n['Save'][1].get('enabled'))
  await asyncio.sleep(.7);baseline=info();original=inspect();base_pixels=None if compact else pixels();
  if not compact:base_pixels.save(out/'ordinary.png')
  nodes=await layout();check(nodes['Object grab mode'][1].get('toggle-state')==0,'Ordinary desktop/workspace interaction remains the initial mode')
  await click('Ghost other objects');await click('Hold selected object');await ready(lambda n:n['Release held object'][1].get('enabled'))
  if not compact:
   await wait(lambda:pixels().tobytes()!=base_pixels.tobytes());pixels().save(out/'ghost.png')
  await click('Save')
  check(info()['scene_hash']==baseline['scene_hash'],'Ghost rendering and holding save no material, visibility, lock or source overrides')
  await click('Next object');nodes=await ready(lambda n:n['Object and material...'][1].get('enabled') is False)
  check(nodes['Visible'][1].get('enabled') is True and nodes['Use painted mesh'][1].get('enabled') is False,'Read-only ghost cannot open an editing target while its individual visibility remains usable')
  await click('Visible');await click('Save');hidden=inspect();check(hidden[0]==original[0] and hidden[1]['visible'] is False,'Hiding an individual ghost changes only its requested saved visibility')
  await click('Visible');await click('Previous object');await click('Release held object');await click('Save')
  check(info()['scene_hash']==baseline['scene_hash'],'Releasing isolation preserves restored user visibility and the exact source scene')
  await click('Objects visibility...');dialog=await wait(lambda:named('Objects visibility'));dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();g=dialog.get_geometry();row=44 if compact else 28
  check(dialog.get_wm_transient_for().id==window.id,'Individual object visibility list is a native document-owned modal')
  if compact:check(g.width<=600 and g.height<=300,'Large-font individual visibility controls fit the compact desktop')
  xtest.fake_input(d,X.MotionNotify,x=g.x+28,y=g.y+row);xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync();await asyncio.sleep(.2)
  await click('Close Window');await wait(lambda:named('Objects visibility') is None);await click('Save');check(inspect()[0]['visible'] is False,'Native per-object checkbox hides its own mesh without changing another object')
  await click('Visible');await click('Save')
  if not compact:await wait(lambda:pixels().tobytes()==base_pixels.tobytes())
  if compact:
   check(info()['scene_hash']==baseline['scene_hash'],'Compact visibility edits restore the exact original scene');d.close();bus.disconnect();return
  await click('Object grab mode');await ready(lambda n:n['Object grab mode'][1].get('toggle-state')==1);window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();await asyncio.sleep(.2);g=window.get_geometry();cx,cy=g.width*.5,g.height*.5
  mouse(cx,cy,True);await ready(lambda n:n['Release held object'][1].get('enabled'));held=pixels();mouse(cx+60,cy);await wait(lambda:pixels().tobytes()!=held.tobytes());pixels().save(out/'held-preview.png')
  key('Escape');mouse(cx+60,cy,False);await wait(lambda:pixels().tobytes()==base_pixels.tobytes());await click('Save');check(info()['scene_hash']==baseline['scene_hash'],'Actual held mouse preview cancels to exact source geometry and ordinary pixels')
  mouse(cx,cy,True);await ready(lambda n:n['Release held object'][1].get('enabled'));mouse(cx+60,cy);await asyncio.sleep(.3);mouse(cx+60,cy,False);await ready(lambda n:n['Save'][1].get('enabled'));await click('Save')
  accepted=inspect();check(accepted[0]['x']>.15 and accepted[1]==original[1],'Actual affine object-mode drag commits only the grabbed complete world transform')
  await click('Undo');await click('Save');check(info()['scene_hash']==baseline['scene_hash'],'One native Undo restores the entire accepted held transform')
  await click('Redo');await click('Save');check(inspect()==accepted,'Native Redo restores that exact held transform')
  d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
