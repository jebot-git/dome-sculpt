#!/usr/bin/env python3
"""Actual FLTK default heat binding on private Xvfb/bus; no physical XR."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time,shutil,hashlib
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_HEAT_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_HEAT_BUS':'1'})
ROOT=Path(__file__).resolve().parents[1];BUILD=Path(os.environ['DOME_SCULPT_TEST_BUILD']).resolve();OUT=Path(os.environ['DOME_HEAT_OUT']).resolve();OUT.mkdir(parents=True,exist_ok=True);COMPACT=os.environ.get('DOME_HEAT_COMPACT')=='1';checks=[];processes=[]
def check(v,text):
 checks.append({'passed':bool(v),'check':text});(OUT/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['passed'] for x in checks)},indent=2)+'\n');print(('PASS ' if v else 'FAIL ')+text,flush=True);assert v,text
async def wait(fn,seconds=60):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  v=fn()
  if v:return v
  await asyncio.sleep(.04)
 raise TimeoutError('Actual FLTK heat workflow did not reach state')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-heat-ui-') as home:
  rd,wr=os.pipe();log=(OUT/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','640x360x24' if COMPACT else '1280x1000x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log));os.close(wr)
  with os.fdopen(rd) as f:screen=':'+f.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none','XR_RUNTIME_JSON':'/nonexistent/no-physical-runtime.json'}
  for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
  for key,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[key]=home+'/'+suffix;Path(env[key]).mkdir(mode=0o700)
  if COMPACT:
   path=Path(home,'config/dome/appearance.json');path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  project=Path(home,'heat.dsculpt');baseline=Path(home,'original.dsculpt');helper=str(BUILD/'surface-heat-ui');subprocess.run([helper,'--ui-fixture',str(project)],check=True,env=env);shutil.copyfile(project,baseline)
  def inspect():return json.loads(subprocess.check_output([helper,'--inspect',str(project),str(baseline)],env=env))
  initial=inspect();original=initial['scene'];(OUT/'original-witness.json').write_text(json.dumps({'scene_sha256':hashlib.sha256(json.dumps(original,sort_keys=True,separators=(',',':')).encode()).hexdigest()},indent=2));shutil.copyfile(baseline,OUT/'original.dsculpt');app_log=(OUT/'app.log').open('w');app=subprocess.Popen([str(BUILD/'dome-sculpt'),str(project)],cwd=home,env=env,stdout=app_log,stderr=app_log);processes.append(app);d=display.Display(screen)
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if title in str(w.get_wm_name() or '') and w.get_attributes().map_state==X.IsViewable:return w
    except error.BadWindow:pass
  window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  if os.environ.get('DOME_HEAT_EXPECT_DRIVER'):await wait(lambda:os.environ['DOME_HEAT_EXPECT_DRIVER'] in Path('/proc/'+str(app.pid)+'/maps').read_text())
  maps=Path('/proc/'+str(app.pid)+'/maps').read_text();(OUT/'app-maps.txt').write_text(maps)
  for key in ('DOME_HEAT_EXPECT_EGL','DOME_HEAT_EXPECT_DRIVER','DOME_HEAT_EXPECT_TOOLKIT'):
   if os.environ.get(key):check(os.environ[key] in maps,'Actual native application maps '+key)
  def prop(name):
   p=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));menu=proxy.get_interface('com.canonical.dbusmenu')
  async def layout():
   nodes={};result=await menu.call_get_layout(0,-1,[])
   def walk(n):
    ident,props,children=n;props={k:v.value for k,v in props.items()};nodes[props.get('label','').replace('_','')]=(ident,props)
    for c in children:walk(c.value)
   walk(result[1]);return nodes
  async def command(label):
   nodes=await layout();old=project.stat().st_mtime_ns;await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
   if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=old)
   await asyncio.sleep(.15)
  def key(name,down=None):
   for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,d.keysym_to_keycode(XK.string_to_keysym(name)))
   d.sync()
  def tap(w,x,y):
   g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
  def picture(w,name):
   g=w.get_geometry();raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(OUT/name)
  async def ready():
   for _ in range(1500):
    nodes=await layout()
    if nodes['Bind or transfer skin...'][1].get('enabled',True):return
    await asyncio.sleep(.02)
   raise TimeoutError('Native bind remained busy')
  check(not initial['skinned'] and initial['active_morph'] and initial['active_pose'] and initial['authored_paint'],'Genuine current native target starts unskinned with active key/pose and actual colour/PBR paint')
  await ready();await command('Bind or transfer skin...');editor=await wait(lambda:named('Bind or transfer skin'));editor.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();picture(editor,'default-options.png');nodes=await layout()
  check(editor.get_wm_transient_for().id==window.id and not nodes['Bind or transfer skin...'][1].get('enabled',True),'Actual FLTK options retain parent identity and modal scene authority')
  if COMPACT:
   g=editor.get_geometry();check(g.width<=600 and g.height<=320,'Compact large-BDF bind controls fit actual640x360 work area')
   for _ in range(22):xtest.fake_input(d,X.MotionNotify,x=g.x+g.width-10,y=g.y+80);xtest.fake_input(d,X.ButtonPress,5);xtest.fake_input(d,X.ButtonRelease,5)
   d.sync();await asyncio.sleep(.2);picture(editor,'scrolled-options.png')
  key('Return');await wait(lambda:named('Bind or transfer skin') is None);await command('Save');check(inspect()['scene']==original,'Initial Return cancels default options with complete Scene retained')
  await command('Bind or transfer skin...');editor=await wait(lambda:named('Bind or transfer skin'));editor.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();g=editor.get_geometry();tap(editor,g.width*.7,g.height-40);await wait(lambda:named('Bind or transfer skin') is None);await command('Save');await ready();bound=inspect();(OUT/'bound-witness.json').write_text(json.dumps({**{k:v for k,v in bound.items() if k!='scene'},'scene_sha256':hashlib.sha256(json.dumps(bound['scene'],sort_keys=True,separators=(',',':')).encode()).hexdigest()},indent=2));shutil.copyfile(project,OUT/'bound.dsculpt')
  check(bound['skinned'] and bound['foreign_weight']==0 and bound['field_matches'],'Actual default FLTK Apply executes surface heat with the independently qualified exact current component field')
  check(bound['max_pose_m']<3e-5,'Actual saved default matches independent LBS/IBM/current-pose and new-pose active-morph oracle')
  if initial['case']=='mixed':check(bound['blended_rows']==81,'Actual combined Head/Eye plus generic actor retains all81 genuinely blended heat rows')
  check(bound['retained_exact'],'Actual bind preserves complete reference/paint/UV/morph/placement and adds only rig/skin/capsule')
  await command('Undo');await command('Save');check(inspect()['scene']==original,'Exactly one universal Undo restores complete unskinned original')
  nodes=await layout();check(not nodes['Undo'][1].get('enabled',True),'Bind contributes no extra hidden Undo operation')
  await command('Redo');await command('Save');check(inspect()['scene']==bound['scene'],'Universal Redo restores exact complete accepted field')
  await command('Bind or transfer skin...');editor=await wait(lambda:named('Bind or transfer skin'));editor.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Escape');await wait(lambda:named('Bind or transfer skin') is None);await command('Save');check(inspect()['scene']==bound['scene'],'Escape cancels real existing-skin options without changing rows or capsule')
  await command('Bind or transfer skin...');editor=await wait(lambda:named('Bind or transfer skin'));editor.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:named('Bind or transfer skin') is None);check(app.poll() is None,'Ctrl+W closes only the modal bind options')
  await command('Save');check(inspect()['scene']==bound['scene'],'Modal close retains accepted native source/history and file')
  await command('Close Window');await wait(lambda:app.poll() is not None);check(app.returncode==0,'Actual worker/application teardown closes gracefully')
  app_log=(OUT/'reopened-app.log').open('w');app=subprocess.Popen([str(BUILD/'dome-sculpt'),str(project)],cwd=home,env=env,stdout=app_log,stderr=app_log);processes.append(app)
  window=await wait(lambda:named('Dome Sculpt'));service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));menu=proxy.get_interface('com.canonical.dbusmenu');await ready();await command('Save')
  check(inspect()['scene']==bound['scene'],'Actual application reopen and subsequent Save preserve the complete accepted skin/source field')
  await command('Close Window');await wait(lambda:app.poll() is not None);check(app.returncode==0,'Reopened native worker/application closes gracefully')
  d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:
   p.terminate()
   try:p.wait(timeout=10)
   except subprocess.TimeoutExpired:p.kill();p.wait()
