#!/usr/bin/env python3
"""Actual FLTK material texture-set manager workflow on a private X11 display."""
import asyncio,hashlib,json,os,signal,subprocess,sys,time
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_FIELD_MENU_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_FIELD_MENU_BUS':'1'})
root=Path(__file__).resolve().parents[1]
build=Path(os.environ['DOME_FIELD_MENU_BUILD']).resolve();inspector=Path(os.environ['DOME_FIELD_MENU_INSPECTOR']).resolve();fixture=Path(os.environ['DOME_FIELD_MENU_FIXTURE']).resolve();out=Path(os.environ['DOME_FIELD_MENU_OUT']).resolve();out.mkdir(parents=True,exist_ok=True)
processes=[];checks=[];app=None
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def report(complete=False,problem=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':problem,'checks':checks,'count':len(checks),'inputs':{str(p):sha(p) for p in [build/'dome-sculpt',inspector,fixture]},'scope':'Actual FLTK texture-set manager, active field controls and pointer painting on private X11; no resolution resize control or physical headset claim'},indent=2)+'\n')
def check(value,label):
 checks.append({'passed':bool(value),'label':label});report();print(('PASS ' if value else 'FAIL ')+label,flush=True);assert value,label
async def wait(fn,seconds=50):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=fn()
  if value:return value
  if app is not None and app.poll() is not None:raise RuntimeError(('ordinary app exited',app.returncode))
  await asyncio.sleep(.05)
 raise TimeoutError('Actual FLTK workflow did not reach its expected state')
async def main():
 global app
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 home=out/'home';home.mkdir();rd,wr=os.pipe();xlog=(out/'xvfb.log').open('w');xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1100x800x24','-nolisten','tcp'],pass_fds=(wr,),stdout=xlog,stderr=xlog);processes.append(xvfb);os.close(wr)
 with os.fdopen(rd) as p:screen=':'+p.readline().strip()
 env={**os.environ,'HOME':str(home),'DISPLAY':screen,'XDG_CURRENT_DESKTOP':'Dome','FLTK_BACKEND':'x11','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
 for name in ['WAYLAND_DISPLAY','WAYLAND_SOCKET','__EGL_VENDOR_LIBRARY_FILENAMES','LIBGL_DRIVERS_PATH','MESA_LOADER_DRIVER_OVERRIDE']:env.pop(name,None)
 for name,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[name]=str(home/suffix);Path(env[name]).mkdir(mode=0o700)
 project=home/'actual.dsculpt';project.write_bytes(fixture.read_bytes());d=display.Display(screen)
 def inspect():return json.loads(subprocess.check_output([str(inspector),str(project)],env=env))
 def named(title):
  for w in d.screen().root.query_tree().children:
   try:
    if title in str(w.get_wm_name() or ''):return w
   except error.BadWindow:pass
 def key(name,down=None):
  code=d.keysym_to_keycode(XK.string_to_keysym(name))
  for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,code)
  d.sync()
 def mouse(w,x,y,down=None):
  g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y))
  if down is not None:xtest.fake_input(d,X.ButtonPress if down else X.ButtonRelease,1)
  d.sync()
 async def launch(number):
  global app
  log=(out/('app-'+str(number)+'.log')).open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],env=env,cwd=home,stdout=log,stderr=log);processes.append(app);w=await wait(lambda:named('Dome Sculpt'));w.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
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
   for _ in range(1000):
    nodes=await layout()
    if label in nodes and nodes[label][1].get('enabled',True):break
    await asyncio.sleep(.05)
   else:raise AssertionError(('Actual universal menu command disabled',label))
   with (out/'commands.jsonl').open('a') as f:f.write(json.dumps({'label':label,'time':time.monotonic()})+'\n')
   await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0);await asyncio.sleep(pause)
  (out/('runtime-maps-'+str(number)+'.txt')).write_text(Path('/proc/'+str(app.pid)+'/maps').read_text());return w,layout,command
 async def save(command):
  stamp=project.stat().st_mtime_ns;await command('Save');await wait(lambda:project.stat().st_mtime_ns!=stamp);return inspect()
 async def begin(w,command,cancel=False):
  await command('Start polygon editing');await asyncio.sleep(.3);dialog=named('Convert to editable polygons')
  if dialog:
   check(dialog.get_wm_transient_for().id==w.id,'Actual conversion confirmation belongs to its native document')
   dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
   if cancel:key('Return')
   else:
    g=dialog.get_geometry();row=36;button=min(g.width//2-24,max(140,16*9));mouse(dialog,g.width/2+10+button/2,g.height-1.5*row+8,True);mouse(dialog,g.width/2+10+button/2,g.height-1.5*row+8,False)
   await wait(lambda:named('Convert to editable polygons') is None)
  elif cancel:await command('Cancel polygon edit')
 def actor(scene):return next(o for o in scene['objects'] if o['id']==scene['active'])
 def collection(scene):return actor(scene)['texture_sets']
 def extra(scene):return collection(scene)['additional'][0]
 def primary(scene):return collection(scene)['primary']
 def payload(p):return p['payload']
 def only_extra(before,after):
  expected=json.loads(json.dumps(before));collection(expected)['additional'][0]=json.loads(json.dumps(extra(after)));return expected==after
 def text(value):
  key('Control_L',True);key('a');key('Control_L',False);key('BackSpace')
  for c in value:key('space' if c==' ' else 'period' if c=='.' else c)
 combo_state={}
 async def manager(command):
  await command('Texture sets...');dialog=await wait(lambda:named('Material texture sets'));dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();g=dialog.get_geometry();row=g.height/19
  check(dialog.get_wm_transient_for().id==w.id,'Texture manager is an actual document-owned FLTK modal')
  combo_state[(dialog.id,1)]=collection(inspect())['active'];combo_state[(dialog.id,2)]=0
  return dialog,row
 async def choice(dialog,row,index,value):
  g=dialog.get_geometry();old=combo_state[(dialog.id,index)];mouse(dialog,g.width*.7,row*(index+.4),True);mouse(dialog,g.width*.7,row*(index+.4),False);await asyncio.sleep(.08)
  for _ in range(abs(value-old)):key('Down' if value>old else 'Up')
  key('Return');await asyncio.sleep(.08);combo_state[(dialog.id,index)]=value
 async def entry(dialog,row,index,value):
  g=dialog.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(g.width*2/5)+16,y=g.y+int(row*(index+.4)));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync();text(value);key('Tab');await asyncio.sleep(.06)
 async def finish(dialog,row,accept=True,shortcut=None):
  if shortcut:
   if shortcut=='Ctrl+W':key('Control_L',True);key('w');key('Control_L',False)
   else:key(shortcut)
  else:
   g=dialog.get_geometry();mouse(dialog,g.width/2+(80 if accept else -80),g.height-row,True);mouse(dialog,g.width/2+(80 if accept else -80),g.height-row,False)
  await wait(lambda:named('Material texture sets') is None)
 async def select_set(command,set_id):
  dialog,row=await manager(command);await choice(dialog,row,1,set_id);await finish(dialog,row);return await save(command)
 async def history(command,before,after,label):
  await command('Undo');check(await save(command)==before,label+' has exact one-step Undo');await command('Redo');check(await save(command)==after,label+' has exact one-step Redo')
 def picture(label):
  g=w.get_geometry();raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);im=Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX');im.save(out/(label+'.png'));return im
 async def stroke(x,y,cancel=False):
  w.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Shift_L',True);mouse(w,x,y,True);await asyncio.sleep(.15)
  for dx in [2,4,6]:mouse(w,x+dx,y);await asyncio.sleep(.05)
  if cancel:key('Escape');await asyncio.sleep(.08)
  mouse(w,x+6,y,False);key('Shift_L',False);await asyncio.sleep(.5)
 w,layout,command=await launch(0);initial=inspect();check(initial['version']==40 and collection(initial)['active']==1 and payload(primary(initial))['resolution']==64 and payload(extra(initial)['painted'])['resolution']==128,'Actual native fixture has independent primary64/additional128 fields and active extra1')
 await command('Frame all');dialog,row=await manager(command);g=dialog.get_geometry();raw=dialog.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/'manager-initial.png');await choice(dialog,row,1,0);await choice(dialog,row,2,4);await entry(dialog,row,5,'0.23');await finish(dialog,row,shortcut='Escape');check(await save(command)==initial,'Escape cancels staged field/material controls without changing any source or field')
 dialog,row=await manager(command);await entry(dialog,row,4,'discard this name');await finish(dialog,row,shortcut='Ctrl+W');check(await save(command)==initial and named('Dome Sculpt') is not None,'Ctrl+W from manager text input closes only the modal and discards staged name')
 selected=await select_set(command,0);expected=json.loads(json.dumps(initial));collection(expected)['active']=0;check(selected==expected,'Actual manager selects primary0 without modifying either paint grid/material/membership');await history(command,initial,selected,'Field selection')
 dialog,row=await manager(command);await choice(dialog,row,1,1);await choice(dialog,row,2,3);await entry(dialog,row,4,'gui front');await finish(dialog,row);renamed=await save(command);expected=json.loads(json.dumps(selected));extra(expected)['name']='gui front';check(renamed==expected,'Manager rename changes only its requested additional field, retaining active primary0');await history(command,selected,renamed,'Independent field rename')
 front=await select_set(command,1);expected=json.loads(json.dumps(renamed));collection(expected)['active']=1;check(front==expected,'Manager reselects the actual128px field without replacing primary64 pixels')
 await command('Add layer');layered=await save(command);check(only_extra(front,layered) and len(payload(extra(layered)['painted'])['layers'])==len(payload(extra(front)['painted'])['layers'])+1,'Actual Add layer changes only selected additional128 field');await history(command,front,layered,'Additional layer creation')
 await command('Base color');await command('Blue paint');await command('Frame all');await asyncio.sleep(.2);im=picture('before-front-paint');g=w.get_geometry();points=[]
 for y in range(int(g.height*.3),int(g.height*.7)):
  for x in range(int(g.width*.35),int(g.width*.65)):
   rgb=im.getpixel((x,y))
   if max(rgb)>90 and max(rgb)-min(rgb)>25 and all(max(im.getpixel((x+dx,y+dy)))>90 for dx,dy in [(3,0),(-3,0),(0,3),(0,-3)]):points.append(((x-g.width/2)**2+(y-g.height/2)**2,x,y))
 check(bool(points),'The actual foreground set is visible at the selected pointer paint location');_,x,y=min(points);await stroke(x,y,True);check(await save(command)==layered,'Escape cancels a real selected-field pointer stroke with exact paint/membership rollback')
 await stroke(x,y);painted=await save(command);check(only_extra(layered,painted) and extra(painted)['painted']!=extra(layered)['painted'] and extra(painted)['material']==extra(layered)['material'],'Real pointer stroke changes only selected Front pixels, preserving sibling64 paint and all materials');check(painted['texture_set_geometry']==layered['texture_set_geometry'] and collection(painted)['slots']==collection(layered)['slots'],'Pointer paint preserves actual shared mesh, topology and per-face membership');await history(command,layered,painted,'Selected-field pointer paint');picture('front-painted')
 await command('Layer visible');hidden=await save(command);check(only_extra(painted,hidden) and payload(extra(hidden)['painted'])['layers'][-1]['visible'] is False,'Layer visibility changes only the active additional field');await command('Cycle layer opacity');faded=await save(command);check(only_extra(hidden,faded) and abs(payload(extra(faded)['painted'])['layers'][-1]['opacity']-.75)<1e-6,'Layer opacity targets the selected additional layer independently');await history(command,hidden,faded,'Selected layer opacity')
 await command('Next layer');next_layer=await save(command);check(only_extra(faded,next_layer) and payload(extra(next_layer)['painted'])['active']!=payload(extra(faded)['painted'])['active'],'Next layer navigates only the active material field');await command('Delete layer');deleted=await save(command);check(only_extra(next_layer,deleted) and len(payload(extra(deleted)['painted'])['layers'])==len(payload(extra(next_layer)['painted'])['layers'])-1,'Delete layer preserves the sibling material field and shared geometry');await history(command,next_layer,deleted,'Selected layer deletion')
 dialog,row=await manager(command);await choice(dialog,row,2,4);await entry(dialog,row,5,'0.17');await entry(dialog,row,6,'0.63');await entry(dialog,row,7,'0.28');await entry(dialog,row,10,'0.27');await entry(dialog,row,11,'2.5');await finish(dialog,row,accept=False);check(await save(command)==deleted,'Cancel discards actual color/roughness/HDR material text edits without changing any field')
 dialog,row=await manager(command);await choice(dialog,row,2,4);await entry(dialog,row,5,'0.17');await entry(dialog,row,6,'0.63');await entry(dialog,row,7,'0.28');await entry(dialog,row,10,'0.27');await entry(dialog,row,11,'2.5');await finish(dialog,row);material=await save(command);m=extra(material)['material'];check(only_extra(deleted,material) and extra(material)['painted']==extra(deleted)['painted'] and all(abs(m['color'][i]-v)<1e-5 for i,v in enumerate([.17,.63,.28,1])) and abs(m['roughness']-.27)<1e-5 and abs(m['emissive'][0]-2.5)<1e-5,'Manager Apply publishes only selected Front color/roughness/HDR material and retains both paint grids');await history(command,deleted,material,'Selected material editing')
 back=await select_set(command,0);dialog,row=await manager(command);await choice(dialog,row,2,4);await entry(dialog,row,5,'0.55');await entry(dialog,row,6,'0.12');await entry(dialog,row,7,'0.2');await finish(dialog,row);back_material=await save(command);expected=json.loads(json.dumps(back));actor(expected)['material']=actor(back_material)['material'];check(expected==back_material and all(abs(actor(back_material)['material']['color'][i]-v)<1e-5 for i,v in enumerate([.55,.12,.2,1])),'Primary material editing leaves the separate Front material and every paint layer unchanged')
 await command('Add layer');back_layer=await save(command);expected=json.loads(json.dumps(back_material));collection(expected)['primary']=primary(back_layer);check(expected==back_layer and len(payload(primary(back_layer))['layers'])==len(payload(primary(back_material))['layers'])+1,'Actual Add layer routes to primary64 while the untouched additional128 field remains intact');await command('Base color');await command('Red paint');await stroke(x,y);check(await save(command)==back_layer,'An unselected foreground material occludes primary painting instead of routing its face into the wrong field')
 check(payload(primary(back_layer))['resolution']==64 and payload(extra(back_layer)['painted'])['resolution']==128,'Both independent64/128 resolutions survive manager, material, layer and pointer workflows')
 final=await select_set(command,1);check(payload(primary(final))['resolution']==64 and payload(extra(final)['painted'])['resolution']==128,'Final active128 selection preserves primary64 resolution and source pixels');await command('Close Window');await asyncio.sleep(.2);check(app.wait(timeout=15)==0,'Actual texture-authoring application closes cleanly after Save');app=None
 w,layout,command=await launch(1);check(await save(command)==final,'Fresh ordinary application reopens the exact independent fields/materials/layers/resolutions');dialog,row=await manager(command);await finish(dialog,row,shortcut='Ctrl+W');check(await save(command)==final,'Reopened manager cancellation preserves complete editable set state');await command('Close Window');await asyncio.sleep(.2);check(app.wait(timeout=15)==0,'Fresh texture-authoring application closes cleanly');app=None
 report(True);d.close();bus.disconnect()
try:asyncio.run(main())
except BaseException as e:report(problem=repr(e));raise
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
