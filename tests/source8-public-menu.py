#!/usr/bin/env python3
"""Actual public universal-menu FLOAT Bake, ordered Save and exact Undo/Redo.
Existing GUI81 and blur GUI10 drivers remain unchanged. No private widget
probe/callback is used. Requires a fresh owning current-ABI actual application.
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
if len(sys.argv)!=5:raise SystemExit('usage: source8-public-menu.py APP INSPECTOR FIXTURE_MANIFEST OUTPUT')
app_path,inspector,manifest_path,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
manifest=json.loads(manifest_path.read_text());fixtures={k:(manifest_path.parent/v).resolve() for k,v in manifest['files'].items()};fixture=fixtures['original']
processes=[];checks=[];app=None
diagnostic_snapshot=lambda why:None
public_acks=[];last_action='startup'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins={str(p):sha(p) for p in [app_path,inspector,manifest_path,Path(__file__).resolve(),*fixtures.values()]}
def report(complete=False,problem=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':problem,'checks':checks,'count':len(checks),'inputs':pins,'drift':[p for p,v in pins.items() if sha(p)!=v],'public_acknowledgments':public_acks,'last_action':last_action,'scope':'Actual native FLTK universal Mesh/Bake dialog/worker/ordered Save/standard UndoRedo/fresh reopen; public X11/DBus acknowledgments and exact native files, no product observer or physical headset claim'},indent=2)+'\n')
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
 env={**os.environ,'DISPLAY':screen,'XDG_CURRENT_DESKTOP':'Dome','FLTK_BACKEND':'x11','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none','MESA_SHADER_CACHE_DISABLE':'true'}
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
 check(manifest.get('version')==2 and manifest['kind']=='source8-uv1-float-public' and manifest['resolution']==64 and manifest['original_texcoord']==1 and manifest['current_texcoord']==0 and manifest['scene_version']==42 and manifest['paint_version']==8 and manifest['hidden_locked_source'] and manifest['hidden_target_tiles'] and manifest['hidden_sibling_physical_tiles'] and manifest['independent_native_oracle'] and manifest['independent_texel_oracle'] and expected!=original,'Actual independent producer supplies genuine UV1/Source8/FLOAT and complete hidden/normal/native endpoints')
 window,layout,command=await launch(0);check(inspect()==original,'Actual ordinary app opens the complete original native fixture')
 nodes=await menu_state(layout,'Bake surface...',True);check('Mesh'in nodes,'Actual DomeD universal Mesh menu exports the enabled Bake surface command')
 await menu_state(layout,'Undo',False);await menu_state(layout,'Redo',False);picture(window,'before.png')
 await command('Bake surface...',pause=0)
 dialog=await wait(lambda:named('Bake surface'));await focus(dialog);picture(dialog,'bake-options.png');acknowledge('public-bake-dialog-appeared',window=window_info(dialog))
 # Source-derived public buttons from bake-editor.cpp. The explicitly recorded
 # session font16 gives row=max(36,16+16)=36. No widget probe supplies authority.
 row=36;g=dialog.get_geometry();mouse(dialog,g.width*.75-6,g.height-row*1.5+8,True);mouse(dialog,g.width*.75-6,g.height-row*1.5+8,False)
 await wait(lambda:str(dialog.get_wm_name() or '')=='Bake surface - complete')
 picture(dialog,'bake-complete.png');acknowledge('public-bake-complete-title',window=window_info(dialog))
 mouse(dialog,g.width*.25+6,g.height-row*1.5+8,True);mouse(dialog,g.width*.25+6,g.height-row*1.5+8,False)
 await wait(lambda:not visible(dialog));acknowledge('public-bake-dialog-disappeared',window_id=dialog.id)
 accepted=await save(command,immediate=True)
 check(accepted==expected,'Actual public dialog Bake and ordered Save publish the exact independently checked native FLOAT endpoint')
 await menu_state(layout,'Bake surface...',True);await menu_state(layout,'Undo',True);await menu_state(layout,'Redo',False);picture(window,'accepted.png')
 await focus(window);chord('Control_L','z');await menu_state(layout,'Redo',True)
 check(await save(command,immediate=True)==original,'Standard Ctrl+Z and ordered native Save restore every original field/source/layer byte in one step')
 await menu_state(layout,'Undo',False);picture(window,'undone.png')
 chord('Control_L','Shift_L','z');await menu_state(layout,'Undo',True);await menu_state(layout,'Redo',False)
 check(await save(command,immediate=True)==expected,'Standard Ctrl+Shift+Z restores the exact full accepted native endpoint in one step')
 picture(window,'redone.png');await command('Close Window',pause=0);await wait(lambda:app.poll()is not None);check(app.returncode==0,'Actual document closes after workers, graphics and ordered file work drain');app=None
 window,layout,command=await launch(1);check(inspect()==expected,'Fresh ordinary application reopens the accepted editable native payload exactly');picture(window,'reopened.png')
 await menu_state(layout,'Undo',False);await menu_state(layout,'Redo',False)
 await command('Close Window',pause=0);await wait(lambda:app.poll()is not None);check(app.returncode==0,'Fresh application closes normally without creating spurious history');app=None
 check(not[p for p,v in pins.items()if sha(p)!=v],'All application/inspector/fixture/expected/script inputs remain unchanged')
 # The original ten public Bake checks above are preserved. Additional source
 # views use real fresh apps and ordinary-material counterfactual controls.
 async def source_view(file,name,accept=lambda shown:True):
  nonlocal project
  project=task_root/(name+'.dsculpt');project.write_bytes(file.read_bytes())
  wanted=json.loads(subprocess.check_output([str(inspector),'--inspect',str(file)],env=env))
  w,layout,command=await launch(name)
  await command('Authored material')
  # The public X window hierarchy exposes the actual GL drawable geometry.
  # No FLTK pointer, product callback, private marker or widget observer.
  def drawable():
   candidates=[];todo=list(w.query_tree().children)
   while todo:
    child=todo.pop()
    try:
     todo+=list(child.query_tree().children);g=child.get_geometry()
     if visible(child)and g.width>200 and g.height>200:candidates.append(child)
    except error.BadWindow:pass
   return max(candidates,key=lambda v:v.get_geometry().width*v.get_geometry().height)if candidates else None
  canvas=await wait(drawable);previous=None;stable=0
  def settled():
   nonlocal previous,stable
   current=image(canvas);data=current.tobytes();stable=stable+1 if data==previous else 0;previous=data
   return current if stable>=3 and accept(current)else None
  shown=await wait(settled);shown.save(out/(name+'.png'))
  check(inspect()==wanted,name+': exact native source/control input remains unchanged during actual display')
  await command('Close Window',pause=0);await wait(lambda:app.poll()is not None)
  check(app.returncode==0,name+': actual view app closes normally')
  return shown
 view_files={k:(manifest_path.parent/v).resolve()for k,v in manifest['view_files'].items()}
 for file in view_files.values():pins[str(file)]=sha(file)
 empty_view=await source_view(view_files['empty'],'source8-hidden-counterfactual')
 descriptors={};controls={};wrong_controls={};normal_effects=[]
 for probe in range(4):
  control=task_root/('independent-control-'+str(probe)+'.dsculpt')
  descriptor=json.loads(subprocess.check_output([str(inspector),'--visible-control',str(manifest_path),'desktop',str(empty_view.width),str(empty_view.height),'0',str(probe),str(control)],env=env,timeout=50))
  pins[str(control)]=sha(control);descriptors[probe]=descriptor;controls[probe]=control
  wrong=task_root/('without-baked-normal-'+str(probe)+'.dsculpt')
  wrong_descriptor=json.loads(subprocess.check_output([str(inspector),'--visible-control',str(manifest_path),'desktop',str(empty_view.width),str(empty_view.height),'0',str(probe),str(wrong),'--without-baked-detail'],env=env,timeout=50))
  check(wrong_descriptor['control_kind']=='without-baked-detail','Independent wrong-normal control is explicitly distinguished from accepted source data')
  pins[str(wrong)]=sha(wrong);wrong_controls[probe]=wrong
 def probe_rgb(shown,descriptor):
  x,y=descriptor['pixel_bottom_to_top'];return shown.getpixel((x,shown.height-1-y))
 def source_geometry(shown):
  return shown.size==empty_view.size and all(max(abs(a-b)for a,b in zip(probe_rgb(shown,descriptors[probe]),probe_rgb(empty_view,descriptors[probe])))>=12 for probe in range(4))
 actual_view=await source_view(view_files['original'],'source8-visible',source_geometry)
 check(actual_view.size==empty_view.size,'Source8 actual and hidden counterfactual use identical real drawable geometry')
 for probe in range(4):
  descriptor=descriptors[probe]
  def independent_match(shown):
   if shown.size!=actual_view.size:return False
   got=probe_rgb(actual_view,descriptor);wanted=probe_rgb(shown,descriptor);hidden=probe_rgb(empty_view,descriptor)
   return max(abs(a-b)for a,b in zip(got,wanted))<=3 and max(abs(a-b)for a,b in zip(wanted,hidden))>=12
  reference=await source_view(controls[probe],'source8-control-'+str(probe),independent_match)
  check(reference.size==actual_view.size,'Independent source control'+str(probe)+' preserves exact real drawable projection')
  actual_rgb=probe_rgb(actual_view,descriptor);expected_rgb=probe_rgb(reference,descriptor);hidden_rgb=probe_rgb(empty_view,descriptor)
  def wrong_geometry(shown):
   return shown.size==empty_view.size and max(abs(a-b)for a,b in zip(probe_rgb(shown,descriptor),hidden_rgb))>=12
  wrong=await source_view(wrong_controls[probe],'source8-without-baked-detail-'+str(probe),wrong_geometry)
  wrong_rgb=probe_rgb(wrong,descriptor)
  normal_effects.append({'probe':probe,'independent_effect':max(abs(a-b)for a,b in zip(expected_rgb,wrong_rgb)),'actual_effect':max(abs(a-b)for a,b in zip(actual_rgb,wrong_rgb))})
  check(not all(v==255 for v in expected_rgb)and not all(v==255 for v in wrong_rgb),'Source8 normal probe'+str(probe)+' ordinary controls do not pass through clipped-white equality')
  check(descriptor['maximum_rgb_difference']==3 and max(abs(a-b)for a,b in zip(actual_rgb,expected_rgb))<=3,'Visible Source8 probe'+str(probe)+' matches independent original/current normal/FLOAT ordinary material control within original renderer bound3')
  check(descriptor['minimum_geometry_counterfactual_difference']==12 and max(abs(a-b)for a,b in zip(expected_rgb,hidden_rgb))>=12,'Visible Source8 probe'+str(probe)+' is actual geometry rather than shared empty/background pixels')
  (out/('source8-probe-'+str(probe)+'.json')).write_text(json.dumps({**descriptor,'actual_RGB':actual_rgb,'independent_RGB':expected_rgb,'hidden_RGB':hidden_rgb,'without_baked_detail_RGB':wrong_rgb},indent=2)+'\n')
 check(manifest['visible_minimum_case_normal_effect']==7 and any(v['independent_effect']>=7 and v['actual_effect']>3 for v in normal_effects),'Actual visible Source8 has disjoint independent baked-normal acceptance intervals and wrong-normal rejection across declared four probes')
 (out/'source8-normal-sensitivity.json').write_text(json.dumps(normal_effects,indent=2)+'\n')
 d.close();bus.disconnect();report(True)
try:asyncio.run(asyncio.wait_for(main(),900))
except BaseException as failure:
 diagnostic_snapshot('exception');report(problem=repr(failure));raise
finally:
 for process in reversed(processes):
  if process.poll()is None:
   process.terminate()
   try:process.wait(timeout=10)
   except subprocess.TimeoutExpired:process.kill();process.wait()
