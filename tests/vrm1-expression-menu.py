#!/usr/bin/env python3
"""Actual private-X11 native universal-menu expression preview workflow."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time,math
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X,XK,display,error,Xatom
from Xlib.ext import xtest
from Xlib.protocol import event
from PIL import Image,ImageChops
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_EXPRESSION_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_EXPRESSION_BUS':'1'})
root=Path(os.environ['DOME_EXPRESSION_ROOT']).resolve();build=Path(os.environ['DOME_EXPRESSION_BUILD']).resolve();fixture=Path(os.environ['DOME_EXPRESSION_FIXTURE']).resolve();expected=Path(os.environ['DOME_EXPRESSION_EXPECTED']).resolve();out=Path(os.environ['DOME_EXPRESSION_OUT']).resolve();out.mkdir();compact=os.environ.get('DOME_EXPRESSION_COMPACT')=='1';checks=[];processes=[]
def check(value,label):
 checks.append({'passed':bool(value),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['passed'] for x in checks)},indent=2)+'\n');print(('PASS ' if value else 'FAIL ')+label,flush=True);assert value,label
async def wait(fn,seconds=60):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=fn()
  if value:return value
  await asyncio.sleep(.04)
 raise TimeoutError('Expression native UI endpoint timeout')
async def main():
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-expression-menu-') as home:
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','640x360x24' if compact else '1280x1000x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
  with os.fdopen(rd) as f:screen=':'+f.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for name in ['WAYLAND_DISPLAY','WAYLAND_SOCKET','LD_PRELOAD']:env.pop(name,None)
  for name,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[name]=home+'/'+suffix;Path(env[name]).mkdir(mode=0o700)
  if compact:
   config=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';config.parent.mkdir(parents=True);config.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  project=Path(home)/'original.dsculpt';project.write_bytes(fixture.read_bytes());original=project.read_bytes();inspector=build/'dome-sculpt-native-authoring-inspect';inspect=lambda p:json.loads(subprocess.check_output([str(inspector),str(p)],env=env,text=True));baseline=inspect(project);proposed=inspect(expected)
  log=(out/'app.log').open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],cwd=home,env=env,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if title in str(w.get_wm_name() or '') and w.get_attributes().map_state==X.IsViewable:return w
    except error.BadWindow:pass
  window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  def prop(name):
   value=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(value.value).decode() if value else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));menu=proxy.get_interface('com.canonical.dbusmenu')
  async def nodes():
   result=await menu.call_get_layout(0,-1,[]);values={}
   def walk(node):
    ident,props,children=node;props={k:v.value for k,v in props.items()};values[props.get('label','').replace('_','')]=(ident,props)
    for child in children:walk(child.value)
   walk(result[1]);return values
  async def command(label):
   values=await nodes();assert label in values,label;stamp=project.stat().st_mtime_ns if label=='Save' else None;await menu.call_event(values[label][0],'clicked',Variant('i',0),0)
   if stamp is not None:await wait(lambda:project.stat().st_mtime_ns!=stamp)
   else:await asyncio.sleep(.2)
  async def enabled(label,value=True):
   for _ in range(1500):
    values=await nodes()
    if values.get(label,(0,{}))[1].get('enabled',True)==value:return values
    await asyncio.sleep(.04)
   raise TimeoutError('Menu readiness: '+label)
  def key(symbol,down=None):
   for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,d.keysym_to_keycode(XK.string_to_keysym(symbol)))
   d.sync()
  def tap(w,x,y):
   g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
  def image(w):
   g=w.get_geometry();raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);return Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX')
  def model_image(label):
   im=image(window);w,h=im.size;im.save(out/(label+'-window.png'));crop=im.crop((w//5,h//5,w*4//5,h*3//5));crop.save(out/(label+'-model.png'));return crop
  def visible_dialog(w,label):
   g=w.get_geometry();screen=d.screen();check(g.x>=0 and g.y>=0 and g.x+g.width<=screen.width_in_pixels and g.y+g.height<=screen.height_in_pixels,label+' stays inside actual viewport');check(w.get_wm_transient_for().id==window.id and w.get_wm_class()[1]==window.get_wm_class()[1],label+' has actual shared native modal ownership');image(w).save(out/(label+'.png'))
  async def weight(value,accept=True):
   await enabled('Weight...');await command('Weight...');dialog=await wait(lambda:named('Preview expression weight'));visible_dialog(dialog,'weight-'+str(value));dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
   if not accept:
    await command('Close Window');await wait(lambda:named('Preview expression weight') is None);return
   clipboard=d.intern_atom('CLIPBOARD');utf8=d.intern_atom('UTF8_STRING');targets=d.intern_atom('TARGETS');owner=d.screen().root.create_window(0,0,1,1,0,X.CopyFromParent);offered=str(value);copied=[]
   async def serve():
    while True:
     while d.pending_events():
      request=d.next_event()
      if request.type==X.MappingNotify:d.refresh_keyboard_mapping(request)
      if request.type==X.SelectionNotify and request.property!=X.NONE:
       got=request.requestor.get_full_property(request.property,utf8)
       if got:copied.append(bytes(got.value).decode())
      if request.type==X.SelectionRequest:
       p=request.property or request.target
       if request.target==targets:request.requestor.change_property(p,Xatom.ATOM,32,[utf8,Xatom.STRING,targets])
       elif request.target in [utf8,Xatom.STRING]:request.requestor.change_property(p,request.target,8,offered.encode())
       else:p=X.NONE
       request.requestor.send_event(event.SelectionNotify(time=request.time,requestor=request.requestor,selection=request.selection,target=request.target,property=p),propagate=False);d.flush()
     await asyncio.sleep(.01)
   task=asyncio.create_task(serve());owner.set_selection_owner(clipboard,X.CurrentTime);d.sync();await enabled('Select All');await command('Select All');await command('Paste');await asyncio.sleep(.2);await command('Select All');await command('Copy');mask=d.display.info.resource_id_mask;base=window.id&~mask
   def copied_by_app():
    current=d.get_selection_owner(clipboard);return current and current.id!=owner.id and current.id&~mask==base
   await wait(copied_by_app,10);reader=d.screen().root.create_window(0,0,1,1,0,X.CopyFromParent);reader.convert_selection(clipboard,utf8,d.intern_atom('DOME_WEIGHT_ACK'),X.CurrentTime);d.flush();await wait(lambda:copied,10);check(copied[0]==offered,'Actual native weight input acknowledges exact requested '+offered+' from application clipboard owner');(out/('weight-ack-'+offered+'.json')).write_text(json.dumps({'offered':offered,'actual':copied[0],'application_base':base,'owner':d.get_selection_owner(clipboard).id},indent=2)+'\n');reader.destroy();task.cancel();await asyncio.gather(task,return_exceptions=True);owner.destroy();row=46 if compact else 34;g=dialog.get_geometry();tap(dialog,g.width*.75,g.height-row/2-12);await wait(lambda:named('Preview expression weight') is None);await enabled('Apply static pose')
  await enabled('Start preview');await command('Frame all');await asyncio.sleep(.5);before=model_image('before');(out/'runtime-maps.txt').write_text(Path('/proc/'+str(app.pid)+'/maps').read_text());await command('Start preview');await enabled('Apply static pose');values=await nodes();check(not values['Save'][1].get('enabled',True) and not values['Open...'][1].get('enabled',True),'Actual pending preview disables implicit Save/Open');check(project.read_bytes()==original,'Starting real expression preview preserves project bytes');
  await command('Choose expression...');choice=await wait(lambda:named('Choose VRM1 expression'));visible_dialog(choice,'choose-expression');await command('Close Window');await wait(lambda:named('Choose VRM1 expression') is None);check(project.read_bytes()==original,'Native Close cancels expression selection without a source edit');
  # Use actual original catalog ordering, never manufactured menu IDs.
  source=Path(os.environ.get('DOME_EXPRESSION_ORIGINAL_SOURCE',str(fixture.parent/'source.vrm'))).read_bytes();import struct;program=json.loads(source[20:20+struct.unpack_from('<I',source,12)[0]])['extensions']['VRMC_vrm']['expressions'];names=[(custom,name) for custom,table in [(False,'preset'),(True,'custom')] for name in sorted(program.get(table,{}))];index=names.index((False,'happy'))
  async def select_happy():
   await command('Choose expression...');w=await wait(lambda:named('Choose VRM1 expression'));g=w.get_geometry();row=46 if compact else 34;tap(w,g.width*.5,row*3+row/2);await asyncio.sleep(.15)
   for _ in names:key('Up')
   for _ in range(index):key('Down')
   key('Return');await asyncio.sleep(.15);tap(w,g.width*.75,g.height-row/2-12);await wait(lambda:named('Choose VRM1 expression') is None)
  await select_happy();await weight('0.35',False);check(project.read_bytes()==original,'Closing native weight controls does not apply or author a value');await weight('0.35');await asyncio.sleep(.4);changed=model_image('preview');difference=max(ImageChops.difference(before,changed).getextrema()[k][1] for k in range(3));check(difference>2,'Actual central model pixels change under real expression input, excluding status/menu text');check(project.read_bytes()==original,'Live rendered expression leaves native project bytes unchanged');await command('Cancel preview');await enabled('Start preview');await asyncio.sleep(.4);cancelled=model_image('cancelled');check(cancelled.tobytes()==before.tobytes() and inspect(project)==baseline,'Cancel restores exact model pixels and complete native source/paint');
  await command('Start preview');await enabled('Apply static pose');await select_happy();await weight('0.35');await command('Apply static pose');await command('Save');await enabled('Start preview');await wait(lambda:project.read_bytes()!=original);check(inspect(project)==proposed,'Native Apply plus universal Save matches complete genuine worker-derived expected Scene');await command('Undo');await command('Save');await enabled('Start preview');check(inspect(project)==baseline,'One universal Undo restores complete original source/paint/history values');await command('Redo');await command('Save');await enabled('Start preview');check(inspect(project)==proposed,'One universal Redo restores exact accepted expression pose');
  await command('Close Window');await wait(lambda:app.poll() is not None,60);check(app.returncode==0,'Actual native app closes through normal teardown');text=(out/'app.log').read_text();check(not any(x in text for x in ['ERROR: AddressSanitizer','ERROR: LeakSanitizer','runtime error:']),'No sanitizer/runtime report or suppression on native exit');d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=10)
  except subprocess.TimeoutExpired:p.kill();p.wait()
