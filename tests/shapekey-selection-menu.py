#!/usr/bin/env python3
"""Actual FLTK shape authoring; private display/bus/HOME, no HMD."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image,ImageChops
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_SHAPE_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_SHAPE_BUS':'1'})
root=Path(__file__).resolve().parents[1];build=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',root/'build')).resolve();compact=os.environ.get('DOME_SHAPE_COMPACT')=='1';strokes=os.environ.get('DOME_SHAPE_STROKES')=='1';prepared=os.environ.get('DOME_SHAPE_PREPARED')=='1'
out=root/'artifacts'/os.environ.get('DOME_SHAPE_MENU_OUT','shapekey-selection-menu-compact' if compact else 'shapekey-selection-menu');out.mkdir(parents=True,exist_ok=True);checks=[];processes=[]
def report(done=False):(out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks),'complete':done},indent=2)+'\n')
def check(ok,message):checks.append({'passed':bool(ok),'check':message});report();print(('PASS ' if ok else 'FAIL ')+message,flush=True);assert ok,message
async def wait(fn,seconds=45):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=fn()
  if value:return value
  await asyncio.sleep(.04)
 raise TimeoutError('Native shape condition timed out')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-shape-menu-') as home:
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','640x360x24' if compact else '1280x900x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log));os.close(wr)
  with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for k in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(k,None)
  for k,s in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[k]=home+'/'+s;Path(env[k]).mkdir(mode=0o700)
  if compact:
   p=Path(home,'config/dome/appearance.json');p.parent.mkdir(parents=True);p.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  project=Path(home,'shapes.dsculpt');helper=str(build/'dome-sculpt-shapekey-selection-test')
  if os.environ.get('DOME_SHAPE_SOURCE'):project.write_bytes(Path(os.environ['DOME_SHAPE_SOURCE']).read_bytes())
  else:subprocess.run([helper,'--ui-fixture',str(project)],env=env,check=True)
  def inspect():return json.loads(subprocess.check_output([helper,'--inspect',str(project)],env=env))
  original=inspect();active_index=next(i for i,o in enumerate(original['objects']) if o['id']==original['active']);initial_targets=len(original['objects'][active_index]['deformation']['morphs']['targets']);log=(out/'app.log').open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],env=env,cwd=home,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if title in str(w.get_wm_name() or ''):return w
    except error.BadWindow:pass
  window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  def prop(name):
   p=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));menu=bus.get_proxy_object(service,path,await bus.introspect(service,path)).get_interface('com.canonical.dbusmenu')
  async def layout():
   nodes={};result=await menu.call_get_layout(0,-1,[])
   def walk(n):
    ident,p,children=n;p={k:v.value for k,v in p.items()};nodes[p.get('label','').replace('_','')]=(ident,p)
    for c in children:walk(c.value)
   walk(result[1]);return nodes
  async def ready(label,enabled=True):
   end=time.monotonic()+45
   while time.monotonic()<end:
    n=await layout()
    if n[label][1].get('enabled',True)==enabled:return n
    await asyncio.sleep(.04)
   raise TimeoutError('Native shape menu not ready: '+label)
  async def command(label):
   n=await layout();assert n[label][1].get('enabled',True),label+' disabled';stamp=project.stat().st_mtime_ns
   await menu.call_event(n[label][0],'clicked',Variant('i',0),0)
   if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=stamp)
  def key(name):
   code=d.keysym_to_keycode(XK.string_to_keysym(name));xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code);d.sync()
  def tap(w,x,y):
   g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
  def pixels(w):
   g=w.get_geometry();raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF);return Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX')
  await ready('Mirror shapekey...');await command('Mirror shapekey...');dialog=await wait(lambda:named('Mirror shapekey'))
  check(dialog.get_wm_transient_for().id==window.id,'Explicit Mirror uses actual document-owned native FLTK modal')
  dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return');await wait(lambda:named('Mirror shapekey') is None);await command('Save');check(inspect()==original,'Default Return cancels Mirror and preserves complete retained project')
  await command('Mirror shapekey...');dialog=await wait(lambda:named('Mirror shapekey'));dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();g=dialog.get_geometry();row=46 if compact else 32
  check(g.width<=600 and g.height<=320 if compact else g.height<=860,'Mirror dialog fits actual normal/large-bitmap compact desktop')
  pixels(dialog).save(out/'mirror-top.png');before=pixels(dialog);xtest.fake_input(d,X.MotionNotify,x=g.x+g.width//2,y=g.y+g.height//2)
  for _ in range(40):xtest.fake_input(d,X.ButtonPress,5);xtest.fake_input(d,X.ButtonRelease,5)
  d.sync();await asyncio.sleep(.15);pixels(dialog).save(out/'mirror-bottom.png');check(pixels(dialog).tobytes()!=before.tobytes(),'Actual plane/frame controls scroll into view while Apply remains fixed')
  tap(dialog,g.width*.5,g.height-row*4-16+(row-4)/2);await asyncio.sleep(.15);key('Home');key('Down');key('Down');await asyncio.sleep(.1);key('Return');await asyncio.sleep(.1);pixels(dialog).save(out/'mirror-relative.png')
  tap(dialog,g.width*.75,g.height-row*2+24);await wait(lambda:named('Mirror shapekey') is None);await ready('New shapekey...');await command('Save');mirrored=inspect();(out/'mirror-after.dsculpt').write_bytes(project.read_bytes());(out/'mirror-after.json').write_text(json.dumps(mirrored,indent=2))
  def walk(x):
   if isinstance(x,dict):
    yield x
    for y in x.values():yield from walk(y)
   elif isinstance(x,list):
    for y in x:yield from walk(y)
  nodes=list(walk(mirrored));actual=[x for x in nodes if x.get('version')==4 and any(isinstance(t,dict) and t.get('frame_policy')=='base-relative-linear-v1' for t in x.get('targets',[]))]
  check(mirrored['version']==29 and bool(actual),'Actual native Artist frames choice publishes owning Scene29 and genuine relative record4')
  check(any(x.get('version')==12 and x.get('base_version')==10 for x in nodes),'Actual native worker owns material12 rather than relabelling old Mirror record3')
  old=original['objects'][active_index];new=mirrored['objects'][active_index];check(len(new['deformation']['morphs']['targets'])==initial_targets+1,'Native relative Mirror creates exactly one stable new target')
  check(new['painted']==old['painted'] and new['deformation']['skin']==old['deformation']['skin'] and new['deformation']['pose']==old['deformation']['pose'],'Native relative Mirror keeps every current Paint byte, skin and pose exact')
  await command('Undo');await command('Save');check(inspect()==original,'One universal Undo restores exact complete pre-Mirror source graph')
  await command('Redo');await command('Save');check(inspect()==mirrored,'One universal Redo restores exact explicit conversion and source graph')
  await command('Mirror shapekey...');await wait(lambda:named('Mirror shapekey'));await command('Close Window');await wait(lambda:named('Mirror shapekey') is None);await command('Save');check(inspect()==mirrored and app.poll() is None,'Universal Close cancels native Mirror without altering restored conversion/history')
  await command('Choose shapekey...');dialog=await wait(lambda:named('Choose shapekey'));check(dialog.get_wm_transient_for().id==window.id,'Existing-target selector is an actual document-owned native modal');dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return');await wait(lambda:named('Choose shapekey') is None);await command('Save');check(inspect()==mirrored,'Default Return cancels existing-target selection without editing Scene')
  await command('Choose shapekey...');dialog=await wait(lambda:named('Choose shapekey'));g=dialog.get_geometry();row=46 if compact else 32;pixels(dialog).save(out/'choose-target.png');check(g.width<=600 and g.height<=320 if compact else g.height<=860,'Target selector fits the normal or large-bitmap compact desktop')
  tap(dialog,g.width*.5,row*3+(row-4)/2);await asyncio.sleep(.15)
  for _ in range(initial_targets):key('Down')
  key('Return');await asyncio.sleep(.1);pixels(dialog).save(out/'choose-selected-target.png');tap(dialog,g.width*.75,g.height-row*2+24);await wait(lambda:named('Choose shapekey') is None);await command('Save');check(inspect()==mirrored,'Choosing the existing converted target changes no saved geometry, weights, source or history')
  await command('Sculpt selected shapekey');await command('Save');check(inspect()==mirrored,'Entering actual selected converted-key sculpt preserves the persisted mix and graph')
  await asyncio.sleep(.2);g=window.get_geometry();before=pixels(window).crop((0,0,g.width,g.height-40));before.save(out/'stroke-before.png');bounds=ImageChops.difference(before,Image.new('RGB',before.size,before.getpixel((0,0)))).getbbox();assert bounds,'Native selected surface is not visible';stroke_x=bounds[2]-2;background=before.getpixel((0,0));column=[y for y in range(before.height) if max(abs(before.getpixel((stroke_x,y))[k]-background[k]) for k in range(3))>10];assert column,'Native target edge column is not visible';stroke_y=min(column)+1;print('ACTUAL_SURFACE_STROKE_PIXEL',stroke_x,stroke_y,'bounds',bounds,flush=True)
  shift=d.keysym_to_keycode(XK.string_to_keysym('Shift_L'));xtest.fake_input(d,X.KeyPress,shift);xtest.fake_input(d,X.MotionNotify,x=g.x+stroke_x,y=g.y+stroke_y);xtest.fake_input(d,X.ButtonPress,1);d.sync();await ready('New shapekey...',False)
  xtest.fake_input(d,X.MotionNotify,x=g.x+stroke_x,y=g.y+stroke_y+60);xtest.fake_input(d,X.ButtonRelease,1);xtest.fake_input(d,X.KeyRelease,shift);d.sync();await ready('New shapekey...');pixels(window).save(out/'stroke-result-before-save.png');await command('Save');sculpted=inspect();(out/'stroke-after.dsculpt').write_bytes(project.read_bytes());(out/'stroke-after.json').write_text(json.dumps(sculpted,indent=2));selected=initial_targets
  old=mirrored['objects'][active_index];obj=sculpted['objects'][active_index];check(obj['deformation']['morphs']['targets'][selected]!=old['deformation']['morphs']['targets'][selected] and obj['deformation']['morphs']['targets'][:selected]==old['deformation']['morphs']['targets'][:selected],'Actual native freehand changes only the selected converted-Mirror key target')
  check(obj['painted']==old['painted'] and obj['deformation']['morph_weights']==old['deformation']['morph_weights'],'Actual freehand keeps all retained Paint and saved mixed weights exact')
  await command('Undo');await command('Save');check(inspect()==mirrored,'Universal Undo restores complete pre-freehand converted-key endpoint')
  await command('Redo');await command('Save');check(inspect()==sculpted,'Universal Redo restores exact accepted converted-key freehand attributes')
  await command('Sculpt selected shapekey');await command('Save');check(inspect()==sculpted,'Finishing native converted-key sculpt retains its accepted geometry and source history')
  report(True);d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
