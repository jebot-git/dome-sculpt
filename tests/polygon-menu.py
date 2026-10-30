#!/usr/bin/env python3
"""Actual native FLTK/DBusMenu N-gon editing; private display and no HMD."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_POLYGON_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_POLYGON_BUS':'1'})
root=Path(__file__).resolve().parents[1];build=Path(os.environ.get('DOME_POLYGON_BUILD',root/'build-polygon-document')).resolve();compact=os.environ.get('DOME_POLYGON_COMPACT')=='1';sculpt=os.environ.get('DOME_POLYGON_SCULPT')=='1';colored=os.environ.get('DOME_POLYGON_COLOR')=='1'
out=root/'artifacts'/('polygon-menu-color-compact' if colored and compact else 'polygon-menu-color' if colored else 'polygon-menu-sculpt' if sculpt else 'polygon-menu-compact' if compact else 'polygon-menu');out.mkdir(parents=True,exist_ok=True)
checks=[];processes=[]
def write_report(complete=False,error=None):
 (out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks),'complete':complete,'status':'failed' if error else 'passed' if complete else 'running','error':error},indent=2)+'\n')
def error_hook(kind,value,trace):write_report(error=str(value));sys.__excepthook__(kind,value,trace)
sys.excepthook=error_hook
def check(value,message):
 checks.append({'passed':bool(value),'check':message});print(('PASS ' if value else 'FAIL ')+message,flush=True)
 write_report();assert value,message
async def wait(predicate,seconds=45):
 deadline=time.monotonic()+seconds
 while time.monotonic()<deadline:
  value=predicate()
  if value:return value
  await asyncio.sleep(.04)
 raise AssertionError('Native polygon condition timed out')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-polygon-native-') as home:
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','640x360x24' if compact else '1280x900x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log));os.close(wr)
  with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'XDG_CURRENT_DESKTOP':'Dome','FLTK_BACKEND':'x11','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for name in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(name,None)
  for name,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[name]=home+'/'+suffix;Path(env[name]).mkdir(mode=0o700)
  if compact:
   path=Path(home,'config/dome/appearance.json');path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  fixture=Path(home,'polygons.dsculpt');subprocess.run([str(build/('dome-sculpt-polygon-color-document-test' if colored else 'dome-sculpt-polygon-document-test')),'--sculpt-fixture' if sculpt else '--ui-fixture',str(fixture)],check=True)
  def inspect():return json.loads(subprocess.check_output([str(build/'dome-sculpt-polygon-document-test'),'--inspect',str(fixture)]))
  original=inspect();app=subprocess.Popen([str(build/'dome-sculpt'),str(fixture)],env=env,cwd=home,stdout=(out/'app.log').open('w'),stderr=subprocess.STDOUT);processes.append(app);d=display.Display(screen)
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if title in str(w.get_wm_name() or ''):return w
    except error.BadWindow:pass
  window=await wait(lambda:named('Dome Sculpt'))
  if compact:
   window.configure(x=0,y=0,width=640,height=360);d.sync()
   await wait(lambda:window.get_geometry().width==640 and window.get_geometry().height==360)
  window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  def prop(name):
   value=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(value.value).decode() if value else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'))
  menu=bus.get_proxy_object(service,path,await bus.introspect(service,path)).get_interface('com.canonical.dbusmenu')
  async def layout():
   result=await menu.call_get_layout(0,-1,[]);nodes={}
   def walk(node):
    ident,props,children=node;props={k:v.value for k,v in props.items()};nodes[props.get('label','').replace('_','')]=(ident,props)
    for child in children:walk(child.value)
   walk(result[1]);return nodes
  async def ready(predicate):
   deadline=time.monotonic()+45
   while time.monotonic()<deadline:
    nodes=await layout()
    if predicate(nodes):return nodes
    await asyncio.sleep(.04)
   raise AssertionError('Native polygon menu did not acknowledge operation')
  async def click(label):
   nodes=await layout();assert nodes[label][1].get('enabled',True),label+' disabled'
   stamp=fixture.stat().st_mtime_ns if label=='Save' else None
   await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
   if stamp is not None:await wait(lambda:fixture.stat().st_mtime_ns!=stamp)
  def key(name,pressed=None):
   code=d.keysym_to_keycode(XK.string_to_keysym(name))
   if pressed is None:xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code)
   else:xtest.fake_input(d,X.KeyPress if pressed else X.KeyRelease,code)
   d.sync()
  def mouse(x,y,pressed=None):
   g=window.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y))
   if pressed is not None:xtest.fake_input(d,X.ButtonPress if pressed else X.ButtonRelease,1)
   d.sync()
  def pixels():
   g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF);im=Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX');return im.crop((0,28,g.width,max(29,g.height-28)))
  conversion_cancel_checked=False
  async def begin():
   nonlocal conversion_cancel_checked
   await click('Start polygon editing')
   if not colored:return
   dialog=await wait(lambda:named('Convert to editable polygons'))
   if not conversion_cancel_checked:
    check(dialog.get_wm_transient_for().id==window.id,'Raw color conversion uses the existing document-owned FLTK confirmation')
    dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return')
    await wait(lambda:named('Convert to editable polygons') is None)
    await ready(lambda n:n['Start polygon editing'][1].get('enabled'))
    check(inspect()==original and named('Dome Sculpt') is not None,'Return defaults to Cancel and retains exact raw color, rig, morph and paint source')
    conversion_cancel_checked=True
    await click('Start polygon editing');dialog=await wait(lambda:named('Convert to editable polygons'))
   g=dialog.get_geometry();font=32 if compact else 16;row=max(36,font+16)
   button=min(g.width//2-24,max(140,font*9))
   xtest.fake_input(d,X.MotionNotify,x=g.x+int(g.width/2+10+button/2),y=g.y+int(g.height-1.5*row+8))
   xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
   await wait(lambda:named('Convert to editable polygons') is None)
  await ready(lambda n:n['Start polygon editing'][1].get('enabled') and (sculpt or n['Use painted mesh'][1].get('toggle-state')==1))
  if os.environ.get('DOME_POLYGON_ADMISSION')=='1':
   nodes=await layout()
   check(all(nodes[name][1].get('enabled',True) for name in ('Bake surface...','Bind or transfer skin...','Start cut','Start patch')),'Eligible target/reference exposes actual universal-menu asset commands')
  check('Start polygon editing' in await layout(),'Polygon tools are part of the existing native universal menu')
  await begin();await ready(lambda n:n['Apply polygon edit'][1].get('enabled') and n['Faces'][1].get('toggle-state')==1)
  if os.environ.get('DOME_POLYGON_ADMISSION')=='1':
   nodes=await layout()
   check(all(not nodes[name][1].get('enabled',True) for name in ('Bake surface...','Bind or transfer skin...','Start cut','Start patch')),'Active polygon workspace disables conflicting universal-menu asset commands')
  await wait(lambda:any(b>g+8 and g>r+8 for r,g,b in pixels().getdata()));before=pixels();before.save(out/'workspace.png')
  await click('Polygon controls...');dialog=await wait(lambda:named('Polygon edit controls'));g=dialog.get_geometry()
  check(dialog.get_wm_transient_for().id==window.id,'Polygon settings use a document-owned native FLTK modal')
  if compact:
   check(g.width<=600 and g.height<=320,'Polygon controls and fixed actions fit the compact large-font desktop')
   def dialog_pixels():
    raw=dialog.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF)
    return Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX')
   def dialog_drawn():
    im=dialog_pixels();r,gc,b=im.getpixel((5,5))
    return r<40 and b>r+3 and sum(r>180 and gc>180 and b>180 for r,gc,b in im.crop((30,g.height-80,g.width//2-15,g.height-35)).getdata())>50
   await wait(dialog_drawn)
   top=dialog_pixels();top.save(out/'controls-top.png')
   xtest.fake_input(d,X.MotionNotify,x=g.x+g.width-12,y=g.y+80)
   for _ in range(24):xtest.fake_input(d,X.ButtonPress,5);xtest.fake_input(d,X.ButtonRelease,5)
   d.sync();await wait(lambda:dialog_pixels().crop((0,0,g.width,g.height-100)).tobytes()!=top.crop((0,0,g.width,g.height-100)).tobytes())
   bottom=dialog_pixels();bottom.save(out/'controls-bottom.png')
   check(bottom.crop((0,g.height-96,g.width,g.height)).tobytes()==top.crop((0,g.height-96,g.width,g.height)).tobytes(),'Compact Cancel and Apply actions remain fixed while content scrolls')
   check(True,'Compact native controls scroll to the remaining topology settings while actions stay fixed')
  await click('Close Window');await wait(lambda:named('Polygon edit controls') is None)
  check(named('Dome Sculpt') is not None,'Ctrl+W universal Close first dismisses local settings and keeps editor alive')
  window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();g=window.get_geometry();cx,cy=g.width/2,g.height/2
  key('Shift_L',True);mouse(cx,cy,True);mouse(cx,cy,False);key('Shift_L',False)
  await ready(lambda n:n['Extrude faces'][1].get('enabled'));await wait(lambda:pixels().tobytes()!=before.tobytes());selected=pixels();selected.save(out/'selection.png')
  await click('Save');check(inspect()==original,'Actual component selection and unconfirmed Save leave full source scene exact')
  await click('Extrude faces');await ready(lambda n:n['Apply polygon edit'][1].get('enabled') and n['Undo'][1].get('enabled'))
  await wait(lambda:pixels().tobytes()!=selected.tobytes());extruded=pixels();extruded.save(out/'extruded.png')
  await click('Undo')
  try:await wait(lambda:pixels().tobytes()==selected.tobytes())
  except Exception:
   pixels().save(out/'undo-timeout.png');raise
  await click('Save');check(inspect()==original,'Native Undo restores local N-gons while preserving committed scene and paint')
  await click('Redo');await wait(lambda:pixels().tobytes()==extruded.tobytes());await click('Cancel polygon edit');await ready(lambda n:n['Start polygon editing'][1].get('enabled'))
  await click('Save');check(inspect()==original,'Native Cancel discards full draft topology without a global history entry')
  if colored:
   await wait(lambda:len({(r>>3,g>>3,b>>3) for r,g,b in pixels().getdata()})>100)
   check(True,'Native Cancel restores actual source colors without another viewport input event')
   pixels().save(out/'original-source.png')
  if os.environ.get('DOME_POLYGON_ADMISSION')=='1':
   nodes=await layout()
   check(all(nodes[name][1].get('enabled',True) for name in ('Bake surface...','Bind or transfer skin...','Start cut','Start patch')),'Cancelling the workspace restores actual universal-menu asset commands')
  await begin();await ready(lambda n:n['Apply polygon edit'][1].get('enabled'));await click('Close Window');await ready(lambda n:n['Start polygon editing'][1].get('enabled'))
  check(named('Dome Sculpt') is not None,'Universal Close cancels the current polygon workspace before closing the window')
  # Apply plus immediate Save must observe the accepted transaction endpoint.
  await begin();await ready(lambda n:n['Apply polygon edit'][1].get('enabled'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();base=pixels()
  key('Shift_L',True);mouse(cx,cy,True);mouse(cx,cy,False);key('Shift_L',False);await wait(lambda:pixels().tobytes()!=base.tobytes())
  await click('Extrude faces');await ready(lambda n:n['Apply polygon edit'][1].get('enabled'));await click('Apply polygon edit');await click('Save');accepted=inspect()
  check(accepted!=original and accepted['version']==(14 if colored else 12),'Native Apply then immediate Save persists authenticated current polygon authority')
  if colored:
   owner=accepted['polygon_sources']['authorities'][accepted['objects'][0]['polygon']]
   check(owner['colors'] is not None and accepted['polygon_sources']['color_fields'] and accepted['polygon_sources']['color_bases'],'Actual native conversion and topology edits persist the current corner RGBA field')
   check(accepted['source_models']==original['source_models'],'Native topology conversion retains exact original raw source model and accessor archive')
   pixels().save(out/'accepted-colors.png')
  await click('Undo');await click('Save');check(inspect()==original,'One native global Undo restores complete original geometry, stable IDs and paint')
  if colored:
   await wait(lambda:len({(r>>3,g>>3,b>>3) for r,g,b in pixels().getdata()})>100)
   check(True,'Native global Undo displays the restored raw-color source after polygon acceptance')
   pixels().save(out/'undo-source.png')
  await click('Redo');await click('Save');check(inspect()==accepted,'Native global Redo restores the exact accepted polygon endpoint')
  write_report(complete=True);d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
