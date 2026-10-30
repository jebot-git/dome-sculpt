#!/usr/bin/env python3
"""Actual FLTK export chooser/Cancel/retry, with a test-only durable-write gate."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_EXPORT_MENU_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_EXPORT_MENU_BUS':'1'})
root=Path(__file__).resolve().parents[1];build=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',root/'build')).resolve()
out=root/'artifacts/menu-export-cancellation';out.mkdir(parents=True,exist_ok=True)
checks=[];processes=[]
def report(complete=False):
 (out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['passed'] for x in checks),'complete':complete},indent=2)+'\n')
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});report();print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
async def wait(predicate,seconds=45):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  result=predicate()
  if result:return result
  await asyncio.sleep(.04)
 raise TimeoutError('Native export did not reach expected state')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-export-menu-') as home:
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1280x900x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log));os.close(wr)
  with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for name in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(name,None)
  for name,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[name]=home+'/'+suffix;Path(env[name]).mkdir(mode=0o700)
  helper=str(build/'dome-sculpt-export-cancellation-test');project=Path(home,'native.dsculpt');target=Path(home,'Sculpt.glb');gate=Path(home,'gate');gate.mkdir()
  subprocess.run([helper,'--ui-fixture',str(project)],check=True,env=env)
  def inspect():return json.loads(subprocess.check_output([helper,'--inspect',str(project)],env=env))
  original=inspect();env.update(LD_PRELOAD=str(build/'libdome-sculpt-export-cancel-gate.so'),DOME_TEST_EXPORT_GATE_DIR=str(gate),DOME_TEST_EXPORT_OUTPUT=str(target))
  log=(out/'app.log').open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],env=env,cwd=home,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if title in str(w.get_wm_name() or ''):return w
    except error.BadWindow:pass
  window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  def prop(name):
   p=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'))
  menu=bus.get_proxy_object(service,path,await bus.introspect(service,path)).get_interface('com.canonical.dbusmenu')
  async def layout():
   result=await menu.call_get_layout(0,-1,[]);nodes={}
   def walk(node):
    ident,p,children=node;p={k:v.value for k,v in p.items()};nodes[p.get('label','').replace('_','')]=(ident,p)
    for child in children:walk(child.value)
   walk(result[1]);return nodes
  async def ready(label,enabled):
   end=time.monotonic()+45
   while time.monotonic()<end:
    nodes=await layout()
    if bool(nodes[label][1].get('enabled',True))==enabled:return nodes
    await asyncio.sleep(.04)
   raise TimeoutError('Native menu capability did not settle: '+label)
  async def command(label):
   nodes=await layout();assert nodes[label][1].get('enabled',True),label+' disabled'
   await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
  def key(name):
   code=d.keysym_to_keycode(XK.string_to_keysym(name));xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code);d.sync()
  def capture(name):
   g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/name)
  await ready('Cancel background work',False)
  check(True,'Native universal menu initially disables Cancel background work without an export')
  await command('Export glTF (.glb)...');chooser=await wait(lambda:named('Export glTF scene'))
  check(chooser.get_wm_transient_for().id==window.id and chooser.get_wm_class()[1]==window.get_wm_class()[1],'Actual export chooser retains document ownership and DomeD application identity')
  await command('Close Window');await wait(lambda:named('Export glTF scene') is None)
  check(not target.exists() and inspect()==original,'Universal Close cancels the native export chooser before submitting any output')
  await command('Export glTF (.glb)...');chooser=await wait(lambda:named('Export glTF scene'));chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return')
  await wait(lambda:(gate/'entered').exists());nodes=await ready('Cancel background work',True);capture('export-running.png')
  check(not target.exists() and nodes['Duplicate object'][1].get('enabled') and nodes['Save'][1].get('enabled'),'A real export pauses after temporary fsync while native authoring and ordered Save remain available')
  await command('Duplicate object');await ready('Undo',True);await command('Undo');await ready('Redo',True)
  stamp=project.stat().st_mtime_ns;await command('Save');await command('Cancel background work');await ready('Cancel background work',False)
  (gate/'release').touch();await wait(lambda:project.stat().st_mtime_ns!=stamp)
  check(not target.exists() and not list(Path(home).glob('Sculpt.glb.tmp-*')),'Actual universal Cancel stops replacement and removes the incomplete export temporary')
  check(inspect()==original and (await layout())['Redo'][1].get('enabled') and app.poll() is None,'The following Save completes with exact original scene and preserved authoring Redo')
  await command('Export glTF (.glb)...');chooser=await wait(lambda:named('Export glTF scene'));chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return')
  await wait(lambda:target.exists());await ready('Cancel background work',False)
  subprocess.run([helper,'--validate-glb',str(target)],check=True,env={k:v for k,v in env.items() if k!='LD_PRELOAD'})
  check(inspect()==original and app.poll() is None,'Retry through the actual native chooser produces an independently parsed GLB and preserves the project')
  capture('export-complete.png');report(True);d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
