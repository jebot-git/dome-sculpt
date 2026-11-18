#!/usr/bin/env python3
"""Create Rasterize/RetainSource normal sources through the actual FLTK UV editor.
Independent producer, wrong-method native endpoint and finite normal decoder
authorize every durable phase; existing Source8 scenarios stay unchanged.
"""
import asyncio,hashlib,json,os,signal,subprocess,sys,time,struct,math
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_BAKED_EMISSION_MENU_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_BAKED_EMISSION_MENU_BUS':'1'})
if len(sys.argv)!=6:raise SystemExit('usage: source-advanced-uv-menu.py APP INSPECTOR CASE_MANIFEST LABEL_REFERENCE OUTPUT')
app_path,inspector,manifest_path,label_reference,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
manifest=json.loads(manifest_path.read_text());fixtures={'original':manifest_path.parent/manifest['before'],'desktop-accepted':manifest_path.parent/manifest['routes']['desktop']['expected'],'wrong-method':manifest_path.parent/manifest['routes']['desktop']['wrong_method']};fixture=fixtures['original'];labels=json.loads(label_reference.read_text());font_file=Path(labels['font_file'])
processes=[];checks=[];app=None;started=time.monotonic()
diagnostic_snapshot=lambda why:None
public_acks=[];last_action='startup'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins={str(p):sha(p) for p in [app_path,inspector,manifest_path,label_reference,font_file,Path(__file__).resolve(),*fixtures.values()]}
def report(complete=False,problem=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':problem,'checks':checks,'count':len(checks),'inputs':pins,'drift':[p for p,v in pins.items() if sha(p)!=v],'public_acknowledgments':public_acks,'last_action':last_action,'scope':'Actual native FLTK universal UV editor/Keep normal/Turn/Apply/worker/ordered Save/standard UndoRedo/fresh reopen; public X11/DBus acknowledgments and exact native files, no product observer or physical headset claim'},indent=2)+'\n')
def check(value,label):
 checks.append({'passed':bool(value),'label':label});report();print(('PASS ' if value else 'FAIL ')+label,flush=True);assert value,label
async def wait(fn,seconds=50):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=fn()
  if value:return value
  if app is not None and app.poll() is not None:raise RuntimeError(('ordinary app exited',app.returncode))
  await asyncio.sleep(.05)
 diagnostic_snapshot('wait-timeout')
 raise TimeoutError('Actual FLTK workflow did not reach its expected state')
async def main():
 global app,diagnostic_snapshot,last_action
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 task_root=out/'home';task_root.mkdir();rd,wr=os.pipe();xlog=(out/'xvfb.log').open('w');xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0',os.environ.get('DOME_BAKED_EMISSION_MENU_SCREEN','1100x800x24'),'-nolisten','tcp'],pass_fds=(wr,),stdout=xlog,stderr=xlog);processes.append(xvfb);os.close(wr)
 with os.fdopen(rd) as p:screen=':'+p.readline().strip()
 env={**os.environ,'DISPLAY':screen,'XDG_CURRENT_DESKTOP':'Dome','FLTK_BACKEND':'x11','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none','MESA_SHADER_CACHE_DIR':str(out/'mesa-shader-code-cache'),'MESA_SHADER_CACHE_SHOW_STATS':'1'}
 env.pop('MESA_SHADER_CACHE_DISABLE',None)
 for name in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(name,None)
 if os.environ.get('DOME_BAKED_EMISSION_APP_PRELOAD'):env['LD_PRELOAD']=os.environ['DOME_BAKED_EMISSION_APP_PRELOAD']
 for name,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[name]=str(task_root/suffix);Path(env[name]).mkdir(mode=0o700)
 appearance=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';appearance.parent.mkdir();appearance.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':1}))
 project=task_root/'actual.dsculpt';project.write_bytes(fixture.read_bytes());d=display.Display(screen)
 def focus_id():
  value=d.get_input_focus().focus;return getattr(value,'id',value)
 def visible(window):
  try:return window.get_attributes().map_state==X.IsViewable
  except error.BadWindow:return False
 def image(window):
  g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff)
  return Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX')
 def window_info(window):
  g=window.get_geometry();a=window.get_attributes()
  return {'id':window.id,'parent':window.query_tree().parent.id,'name':str(window.get_wm_name() or ''),'class':str(window.get_wm_class()),'x':g.x,'y':g.y,'width':g.width,'height':g.height,'map_state':a.map_state,'override_redirect':a.override_redirect}
 def acknowledge(kind,**values):
  row={'kind':kind,'time':time.monotonic(),'focus':focus_id(),**values};public_acks.append(row)
  with (out/'public-acks.jsonl').open('a') as f:f.write(json.dumps(row)+'\n')
 def diagnostic_snapshot(why):
  prefix='failure-'+str(len(list(out.glob('failure-*.json')))).zfill(3)
  evidence={'reason':why,'last_action':last_action,'checks':len(checks),'app_poll':app.poll() if app else None,'windows':[]}
  try:
   evidence['focus']=focus_id()
   for window in d.screen().root.query_tree().children:
    try:
     info=window_info(window);evidence['windows'].append(info)
     if info['map_state']==X.IsViewable:image(window).save(out/(prefix+'-window-'+str(window.id)+'.png'))
    except (error.BadWindow,error.BadMatch):pass
   image(d.screen().root).save(out/(prefix+'-root.png'))
  except Exception as failure:evidence['capture_error']=repr(failure)
  (out/(prefix+'.json')).write_text(json.dumps(evidence,indent=2)+'\n')
 def verify(phase):
  value=json.loads(subprocess.check_output([str(inspector),'--verify-advanced-uv',str(manifest_path),'desktop',phase,str(project)],env=env,timeout=max(.001,240-(time.monotonic()-started))))
  (out/('native-'+phase+'.json')).write_text(json.dumps(value,indent=2)+'\n');check(value.get('verified') is True,'Independent native/finite-normal verifier authorizes public UV UV '+phase+' and rejects wrong method')
 def inspect():return json.loads(subprocess.check_output([str(inspector),'--inspect',str(project)],env=env))
 def named(title):
  for w in d.screen().root.query_tree().children:
   try:
    if visible(w) and title in str(w.get_wm_name() or ''):return w
   except error.BadWindow:pass
 def key(name,down=None):
  code=d.keysym_to_keycode(XK.string_to_keysym(name))
  for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,code)
  d.sync()
 async def focus(window):
  window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();await wait(lambda:focus_id()==window.id)
 def mouse(w,x,y,down=None):
  g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y))
  if down is not None:xtest.fake_input(d,X.ButtonPress if down else X.ButtonRelease,1)
  d.sync()
 async def launch(number):
  global app
  log=(out/('app-'+str(number)+'.log')).open('w');app=subprocess.Popen([str(app_path),str(project)],env=env,cwd=task_root,stdout=log,stderr=log);processes.append(app);w=await wait(lambda:named('Dome Sculpt'));await focus(w)
  await wait(lambda:project.name in str(w.get_wm_name() or ''));acknowledge('document-title-visible',window=window_info(w))
  def prop(name):
   p=w.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));menu=bus.get_proxy_object(service,path,await bus.introspect(service,path)).get_interface('com.canonical.dbusmenu')
  async def layout():
   r=await menu.call_get_layout(0,-1,[]);values={}
   def walk(n):
    ident,props,children=n;props={k:v.value for k,v in props.items()};values[props.get('label','').replace('_','')]=(ident,props)
    for c in children:walk(c.value)
   walk(r[1]);return values
  async def command(label,pause=.15):
   global last_action
   last_action='universal menu '+label
   for _ in range(1000):
    nodes=await layout()
    if label in nodes and nodes[label][1].get('enabled',True):break
    await asyncio.sleep(.05)
   else:raise AssertionError(('Actual universal menu command disabled',label))
   with (out/'commands.jsonl').open('a') as f:f.write(json.dumps({'label':label,'time':time.monotonic()})+'\n')
   await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
   acknowledge('public-menu-command',label=label,id=nodes[label][0],properties=nodes[label][1]);
   if pause:await asyncio.sleep(pause)
  (out/('runtime-maps-'+str(number)+'.txt')).write_text(Path('/proc/'+str(app.pid)+'/maps').read_text());return w,layout,command
 async def save(command,immediate=False):
  stamp=project.stat().st_mtime_ns;await command('Save',pause=0 if immediate else .15);await wait(lambda:project.stat().st_mtime_ns!=stamp);return inspect()
 def chord(*names):
  for name in names:key(name,True)
  for name in reversed(names):key(name,False)
 def picture(window,name):image(window).save(out/name)
 async def menu_state(layout,label,enabled):
  last={}
  async def read():
   nonlocal last
   nodes=await layout();last=nodes
   return label in nodes and bool(nodes[label][1].get('enabled',True))==enabled
  deadline=time.monotonic()+50
  while time.monotonic()<deadline:
   if await read():acknowledge('public-menu-state',label=label,enabled=enabled,properties=last[label][1]);return last
   await asyncio.sleep(.05)
  diagnostic_snapshot('menu-state-timeout');raise TimeoutError((label,enabled))
 original=json.loads(subprocess.check_output([str(inspector),'--inspect',str(fixtures['original'])],env=env));expected=json.loads(subprocess.check_output([str(inspector),'--inspect',str(fixtures['desktop-accepted'])],env=env))
 check(manifest.get('version')==1 and manifest['kind']=='advanced-normal-uv-authoring' and manifest['uv_method'] in ['Rasterize','RetainSource'] and expected!=original,'Independent native UV capsule retains genuine first Bake and two distinct public UV methods')
 window,layout,command=await launch(0);check(inspect()==original,'Actual ordinary application opens the genuine first-Bake source before any SourceUV edit')
 await menu_state(layout,'UV editor...',True);await menu_state(layout,'Undo',False);await menu_state(layout,'Redo',False)
 await command('UV editor...',pause=0);dialog=await wait(lambda:named('UV editor'));await focus(dialog);picture(dialog,'uv-before.png');acknowledge('public-UV-editor-open',window=window_info(dialog))
 row=32;g=dialog.get_geometry()
 if manifest['uv_method']=='RetainSource':
  mouse(dialog,g.width/2,row*2.5,True);mouse(dialog,g.width/2,row*2.5,False);await asyncio.sleep(.2)
  acknowledge('public-keep-normal-toggle',x=g.width/2,y=row*2.5);picture(dialog,'uv-keep-detail.png')
 else:acknowledge('public-default-raster-normal-choice',retention_toggle_not_pressed=True)
 # Actual toolbar Turn+ button (index3, row0), six 15-degree commands. The
 # native expected route invokes these exact float-valued public commands.
 for turn in range(6):
  mouse(dialog,g.width*3.5/6,row/2,True);mouse(dialog,g.width*3.5/6,row/2,False);await asyncio.sleep(.2)
  acknowledge('public-UV-turn-command',number=turn+1,x=g.width*3.5/6,y=row/2)
 picture(dialog,'uv-rotated.png');mouse(dialog,g.width/2+100,g.height-row/2,True);mouse(dialog,g.width/2+100,g.height-row/2,False)
 await wait(lambda:not visible(dialog));acknowledge('public-UV-Apply-closed')
 check(await save(command,immediate=True)==expected,'Actual UV editor Apply and immediate ordered Save match the exact native creation endpoint');verify('accepted')
 await focus(window);chord('Control_L','z');await menu_state(layout,'Redo',True)
 check(await save(command,immediate=True)==original,'Standard Ctrl+Z restores the complete genuine first-Bake Scene before UV creation');verify('undone')
 chord('Control_L','Shift_L','z');await menu_state(layout,'Undo',True)
 check(await save(command,immediate=True)==expected,'Standard Ctrl+Shift+Z restores the exact advanced source graph');verify('redone')
 await command('UV editor...',pause=0);dialog=await wait(lambda:named('UV editor'));await focus(dialog);g=dialog.get_geometry()
 mouse(dialog,g.width*3.5/6,row/2,True);mouse(dialog,g.width*3.5/6,row/2,False);await asyncio.sleep(.2);chord('Control_L','w');await wait(lambda:not visible(dialog))
 check(await save(command,immediate=True)==expected,'DomeD Ctrl+W cancels a further UV draft without source/paint/history mutation')
 await command('Close Window',pause=0);await wait(lambda:app.poll()is not None);check(app.returncode==0,'Actual UV authoring application closes after all workers drain');app=None
 window,layout,command=await launch(1);check(inspect()==expected,'Fresh ordinary application reopens the newly authored advanced source graph exactly');await save(command,immediate=True);verify('reopened')
 await menu_state(layout,'Undo',False);await menu_state(layout,'Redo',False);await command('Close Window',pause=0);await wait(lambda:app.poll()is not None);check(app.returncode==0,'Fresh UV authoring reopen drains and closes normally');app=None
 check(not[p for p,v in pins.items()if sha(p)!=v],'All UV-authoring application/inspector/capsule/font inputs remain unchanged')
 d.close();bus.disconnect();report(True)
try:asyncio.run(asyncio.wait_for(main(),240))
except BaseException as failure:
 diagnostic_snapshot('exception');report(problem=repr(failure));raise
finally:
 for process in reversed(processes):
  if process.poll()is None:
   process.terminate()
   try:process.wait(timeout=10)
   except subprocess.TimeoutExpired:process.kill();process.wait()
