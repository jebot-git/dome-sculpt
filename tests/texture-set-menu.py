#!/usr/bin/env python3
"""Actual FLTK manager51 plus painted-field resize workflows; native file inspection only.
Run against a fresh coherent app/inspector and the actual fixture producer manifest.
DOME_FIELD_MENU_ROW defaults28 (canonical default16px bitmap metrics); use the measured
row when qualifying another toolkit/font profile, including compact windows.
"""
import asyncio,hashlib,json,os,signal,subprocess,sys,time,struct,math
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
manifest_path=Path(os.environ["DOME_FIELD_RESOLUTION_MANIFEST"]).resolve();manifest=json.loads(manifest_path.read_text());fixtures={k:(manifest_path.parent/v).resolve() for k,v in manifest["files"].items()}
processes=[];checks=[];app=None;legacy_count=0
diagnostic_snapshot=lambda why:None
public_acks=[];last_action='startup'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins={str(p):sha(p) for p in [build/'dome-sculpt',inspector,fixture,manifest_path,Path(__file__).resolve(),*fixtures.values()]}
def report(complete=False,problem=None):
 (out/'report.json').write_text(json.dumps({'complete':complete,'error':problem,'checks':checks,'count':len(checks),'inputs':pins,'drift':[p for p,v in pins.items() if sha(p)!=v],'legacy_manager_checks':legacy_count,'public_acknowledgments':public_acks,'last_action':last_action,'scope':'Actual FLTK manager51 plus independent active-field resizing, modal/focus/Cancel/UndoRedo/orderedSave/freshreopen; public X11 window/pixel acknowledgments, no product observer or physical headset claim'},indent=2)+'\n')
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
 global app,legacy_count,diagnostic_snapshot,last_action
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 home=out/'home';home.mkdir();rd,wr=os.pipe();xlog=(out/'xvfb.log').open('w');xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0',os.environ.get('DOME_FIELD_MENU_SCREEN','1100x800x24'),'-nolisten','tcp'],pass_fds=(wr,),stdout=xlog,stderr=xlog);processes.append(xvfb);os.close(wr)
 with os.fdopen(rd) as p:screen=':'+p.readline().strip()
 env={**os.environ,'HOME':str(home),'DISPLAY':screen,'XDG_CURRENT_DESKTOP':'Dome','FLTK_BACKEND':'x11','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none','MESA_SHADER_CACHE_DISABLE':'true'}
 for name in ['WAYLAND_DISPLAY','WAYLAND_SOCKET','__EGL_VENDOR_LIBRARY_FILENAMES','LIBGL_DRIVERS_PATH','MESA_LOADER_DRIVER_OVERRIDE']:env.pop(name,None)
 for name,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[name]=str(home/suffix);Path(env[name]).mkdir(mode=0o700)
 project=home/'actual.dsculpt';project.write_bytes(fixture.read_bytes());d=display.Display(screen)
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
 def inspect():return json.loads(subprocess.check_output([str(inspector),str(project)],env=env))
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
 def canvas_window(window):
  # FLTK's GL subwindow is public X11 geometry, not a private editor observer.
  g=window.get_geometry();candidates=[]
  for child in window.query_tree().children:
   try:
    z=child.get_geometry()
    if visible(child) and z.width==g.width and z.height>=g.height*.75:candidates.append((z.width*z.height,child))
   except error.BadWindow:pass
  return max(candidates,key=lambda v:v[0])[1] if candidates else None
 async def framed_view(window):
  # Independent source-file geometry and documented 45-degree framing policy.
  # All fixtures here have one identity-placed mesh. Do not accept a learned
  # old screenshot, an arbitrary stable image or the overzoomed initial view.
  scene=inspect();objects=scene['objects'];meshes=scene['texture_set_geometry']['meshes']
  assert len(objects)==len(meshes)==1 and objects[0]['rotation']==[0,0,0,1] and objects[0]['scale']==1 and objects[0]['translation']==[0,0,0]
  positions=list(struct.iter_unpack('<3f',bytes(meshes[0]['positions']['bytes'])))
  low=[min(v[i] for v in positions) for i in range(3)];high=[max(v[i] for v in positions) for i in range(3)];extent=[(b-a)/2 for a,b in zip(low,high)]
  last={}
  def presented():
   nonlocal last
   canvas=canvas_window(window)
   if canvas is None:return False
   im=image(canvas);width,height=im.size;tangent=math.tan(math.pi/8)
   distance=max(extent[0]/(tangent*width/height),extent[1]/tangent)*1.12+extent[2]+max(.0001,math.sqrt(sum(v*v for v in extent))*.01)
   half_y=height*.5*extent[1]/(tangent*(distance-extent[2]));half_x=height*.5*extent[0]/(tangent*(distance-extent[2]))
   left=width/2-half_x;top=height/2-half_y;assert left>2 and top>2
   # Source renderer clears to displayed gray48 (dark) or208 (light). Probe
   # all four independently derived background margins and a central interior.
   boxes=[(2,2,max(3,int(left/2)),height-2),(width-max(3,int(left/2)),2,width-2,height-2),(2,2,width-2,max(3,int(top/2))),(2,height-max(3,int(top/2)),width-2,height-2)]
   def background(rgb):return any(max(abs(c-value) for c in rgb)<=2 for value in (48,208))
   margins=[]
   for box in boxes:
    values=list(im.crop(box).getdata());margins.append(sum(background(v) for v in values)/len(values))
   center=(int(width/2-half_x*.2),int(height/2-half_y*.2),int(width/2+half_x*.2),int(height/2+half_y*.2));values=list(im.crop(center).getdata());inside=sum(not background(v) for v in values)/len(values)
   last={'canvas':window_info(canvas),'fixture_bounds':[low,high],'projected_half_extent':[half_x,half_y],'margin_boxes':boxes,'background_fractions':margins,'center_box':center,'center_nonbackground_fraction':inside}
   return min(margins)>=.99 and inside>=.95
  await wait(presented);acknowledge('framed-view-presented',**last)
 def mouse(w,x,y,down=None):
  g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y))
  if down is not None:xtest.fake_input(d,X.ButtonPress if down else X.ButtonRelease,1)
  d.sync()
 async def launch(number):
  global app
  log=(out/('app-'+str(number)+'.log')).open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],env=env,cwd=home,stdout=log,stderr=log);processes.append(app);w=await wait(lambda:named('Dome Sculpt'));await focus(w)
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
   if label=='Frame all':await framed_view(w)
   elif label!='Texture sets...' and pause:await asyncio.sleep(pause)
  (out/('runtime-maps-'+str(number)+'.txt')).write_text(Path('/proc/'+str(app.pid)+'/maps').read_text());return w,layout,command
 async def save(command,immediate=False):
  stamp=project.stat().st_mtime_ns;await command('Save',pause=0 if immediate else .15);await wait(lambda:project.stat().st_mtime_ns!=stamp);return inspect()
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
 def payload(p):
  p=p['payload']
  while 'resolution' not in p:p=p['payload']
  return p
 def only_extra(before,after):
  expected=json.loads(json.dumps(before));collection(expected)['additional'][0]=json.loads(json.dumps(extra(after)));return expected==after
 def text(value):
  key('Control_L',True);key('a');key('Control_L',False);key('BackSpace')
  for c in value:key('space' if c==' ' else 'period' if c=='.' else c)
 combo_state={}
 async def manager(command):
  await command('Texture sets...');dialog=await wait(lambda:named('Material texture sets'));await focus(dialog);g=dialog.get_geometry();row=int(os.environ.get('DOME_FIELD_MENU_ROW','28'));assert 28<=row<=96;assert g.height<=row*20+2
  await wait(lambda:len(image(dialog).getcolors(65536) or [])>=3);acknowledge('manager-presented',window=window_info(dialog),row=row)
  check(dialog.get_wm_transient_for().id==w.id,'Texture manager is an actual document-owned FLTK modal')
  current=inspect();combo_state[(dialog.id,1)]=collection(current)['active'];combo_state[(dialog.id,2)]=0;active=primary(current) if collection(current)['active']==0 else next(v['painted'] for v in collection(current)['additional'] if v['id']==collection(current)['active']);combo_state[(dialog.id,3)]=int(math.log2(payload(active)['resolution']))-6
  return dialog,row
 async def choice(dialog,row,index,value):
  global last_action
  last_action='choice row '+str(index)+' '+str(combo_state[(dialog.id,index)])+' -> '+str(value)
  await focus(dialog);g=dialog.get_geometry();old=combo_state[(dialog.id,index)]
  before={z.id for z in d.screen().root.query_tree().children if visible(z)};px=g.x+int(g.width*.7);py=g.y+int(row*(index+.4))
  text_box=(int(g.width*2/5)+10,row*index+5,g.width-65,row*(index+1)-9);old_label=image(dialog).crop(text_box).tobytes()
  mouse(dialog,g.width*.7,row*(index+.4),True);mouse(dialog,g.width*.7,row*(index+.4),False)
  def popup():
   candidates=[]
   for z in d.screen().root.query_tree().children:
    try:
     a=z.get_attributes();q=z.get_geometry()
     if z.id not in before and a.map_state==X.IsViewable and a.override_redirect and q.height>=row and q.x<=px<=q.x+q.width and q.y<=py<=q.y+q.height:candidates.append((q.width*q.height,z))
    except error.BadWindow:pass
   return min(candidates,key=lambda v:v[0])[1] if candidates else None
  menu_window=await wait(popup)
  await wait(lambda:len(image(menu_window).getcolors(65536) or [])>=3)
  acknowledge('choice-popup-presented',row=index,old=old,target=value,window=window_info(menu_window))
  for step in range(abs(value-old)):
   prior=image(menu_window).tobytes();key('Down' if value>old else 'Up')
   await wait(lambda:visible(menu_window) and image(menu_window).tobytes()!=prior)
   acknowledge('choice-highlight-presented',row=index,step=step+1,window=menu_window.id)
  key('Return');await wait(lambda:not visible(menu_window));await wait(lambda:visible(dialog));await focus(dialog)
  if value!=old:await wait(lambda:image(dialog).crop(text_box).tobytes()!=old_label)
  acknowledge('choice-popup-closed',row=index,target=value,window=menu_window.id,label_redrawn=value!=old,label_box=text_box,label_sha256=hashlib.sha256(image(dialog).crop(text_box).tobytes()).hexdigest());combo_state[(dialog.id,index)]=value
 async def entry(dialog,row,index,value):
  index+=1 if index>=4 else 0 # Real new resolution row precedes old name/material fields.
  g=dialog.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(g.width*2/5)+16,y=g.y+int(row*(index+.4)));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync();text(value);key('Tab');await asyncio.sleep(.06)
 async def finish(dialog,row,accept=True,shortcut=None):
  global last_action
  last_action='manager '+(shortcut or ('Apply' if accept else 'Cancel'))
  if shortcut:
   if shortcut=='Ctrl+W':key('Control_L',True);key('w');key('Control_L',False)
   else:key(shortcut)
  else:
   g=dialog.get_geometry();mouse(dialog,g.width/2+(80 if accept else -80),g.height-row,True);mouse(dialog,g.width/2+(80 if accept else -80),g.height-row,False)
  await wait(lambda:named('Material texture sets') is None)
  acknowledge('manager-closed',requested_button_accept=accept if shortcut is None else None,shortcut=shortcut,window=dialog.id)
 async def select_set(command,set_id):
  dialog,row=await manager(command);await choice(dialog,row,1,set_id);await finish(dialog,row);return await save(command)
 async def history(command,before,after,label):
  await command('Undo');check(await save(command)==before,label+' has exact one-step Undo');await command('Redo');check(await save(command)==after,label+' has exact one-step Redo')
 def picture(label):
  im=image(w);im.save(out/(label+'.png'));return im
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
 legacy_count=len(checks);check(legacy_count==51,'All51 previous real manager assertions remain present and passing before added resize cases')
 def decoded_blob(value):
  if isinstance(value,dict) and 'bytes' in value:return bytes(value['bytes'])
  if isinstance(value,list):return bytes(value)
  raise AssertionError('Native inspector lost the actual binary plane representation')
 def field(scene,set_id):return primary(scene) if set_id==0 else next(v['painted'] for v in collection(scene)['additional'] if v['id']==set_id)
 def authored(scene,set_id):
  grid=payload(field(scene,set_id));components=[False]*14;masked=False;hidden=False;physical=False;legacy=False;tiny=False;physical_max=0
  for layer in grid['layers']:
   tiles=layer['tiles'];hidden|=bool(tiles) and (not layer['visible'] or layer['opacity']==0)
   for tile in tiles:
    data=decoded_blob(tile['data']);assert len(data)==32*32*14
    for at in range(0,len(data),14):
     for c in range(14):components[c]|=data[at+c]!=0
     masked|=data[at+13]!=255
   for tile in layer.get('physical_emission_tiles',[]):
    data=decoded_blob(tile['data']);assert len(data)==32*32*28
    for values in struct.iter_unpack('<7f',data):
     assert all(math.isfinite(v) for v in values) and 0<=values[6]<=1 and all(0<=v<=10000 for v in values[3:6])
     physical|=max(values[3:6])>0;legacy|=max(values[:3])>0;tiny|=0<values[6]<1/255;physical_max=max(physical_max,*values[3:6])
  return {'all14':all(components),'masked':masked,'hidden':hidden,'physical':physical,'legacy':legacy,'tiny':tiny,'physical_max':physical_max,'layers':len(grid['layers']),'resolution':grid['resolution']}
 async def load_fixture(key,number):
  global app
  if app is not None:
   await command('Close Window');check(app.wait(timeout=15)==0,'Independent resize case closes its prior real app cleanly');app=None
  project.write_bytes(fixtures[key].read_bytes());return await launch(number)
 async def resize(command,target,accept=True,shortcut=None,lose_focus=False):
  dialog,row=await manager(command);await choice(dialog,row,2,5);await choice(dialog,row,3,int(math.log2(target))-6)
  if lose_focus:
   dummy=d.screen().root.create_window(4,4,100,50,0,d.screen().root_depth,X.InputOutput,X.CopyFromParent);dummy.set_wm_name('External focus control');dummy.map();dummy.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();await asyncio.sleep(.2);dialog.set_input_focus(X.RevertToParent,X.CurrentTime);dummy.destroy();d.sync()
  await finish(dialog,row,accept=accept,shortcut=shortcut)
 w,layout,command=await load_fixture('rich-extra128',2);rich=inspect();before=authored(rich,1);check(before['all14'] and before['masked'] and before['hidden'] and before['physical'] and before['legacy'] and before['tiny'] and before['physical_max']>9000,'Rich real native input contains all14 channels, independent mask, hidden data and mixed/tiny FLOAT radiance')
 await resize(command,64,shortcut='Escape');check(await save(command)==rich,'Escape cancels a staged actual painted-field resize exactly')
 await resize(command,64,accept=False,lose_focus=True);check(await save(command)==rich,'Actual FLTK focus loss cannot implicitly resize; explicit Cancel retains every original plane')
 await resize(command,128);check(await save(command,True)==rich,'Same-size real manager Apply is an exact native no-op')
 await resize(command,64);small=await save(command,True);expected=json.loads(subprocess.check_output([str(inspector),str(fixtures['extra64'])],env=env));check(small==expected and only_extra(rich,small),'Actual manager Apply plus immediately ordered Save reaches the exact measured extra64 endpoint and untouched primary')
 after=authored(small,1);check(all(after[k] for k in ['all14','masked','hidden','physical','legacy','tiny']) and after['physical_max']>9000 and after['layers']==before['layers'],'Saved resize retains all independent authored planes, units, masks and hidden layers')
 check(small['texture_set_geometry']==rich['texture_set_geometry'] and collection(small)['slots']==collection(rich)['slots'],'Resize preserves complete native shared geometry and logical face membership')
 await history(command,rich,small,'Manager painted resolution resize');picture('resized-extra64')
 await command('Close Window');check(app.wait(timeout=15)==0,'Resized real FLTK app drains and closes normally');app=None
 w,layout,command=await launch(3);check(await save(command)==small,'A genuinely fresh app reopens the exact resized editable FLOAT/mask/layer state')
 w,layout,command=await load_fixture('rich-primary64',4);primary_before=inspect();await resize(command,128);primary_after=await save(command,True);expected=json.loads(subprocess.check_output([str(inspector),str(fixtures['primary128'])],env=env));check(primary_after==expected and extra(primary_after)==extra(primary_before),'Actual primary64-to128 resize keeps the complete extra128 field byte-exact')
 p=authored(primary_after,0);check(p['all14'] and p['masked'] and p['hidden'] and p['physical'] and p['legacy'],'Primary field magnification retains its own full authoring planes');await history(command,primary_before,primary_after,'Primary field magnification')
 w,layout,command=await load_fixture('quality-source',5);quality_before=inspect();await command('Frame all');baseline_status=picture('quality-before');await resize(command,64);await asyncio.sleep(.3)
 # Wait for the real worker's menus to re-enable. Error evidence is the actual
 # changed rendered status plus exact saved data and absent Undo; no fake observer.
 for _ in range(1600):
  nodes=await layout()
  if nodes.get('Texture sets...',(0,{}))[1].get('enabled',False):break
  await asyncio.sleep(.05)
 else:raise TimeoutError('Quality refusal worker did not terminate')
 await asyncio.sleep(.2);quality_status=picture('quality-refusal');g=w.get_geometry();box=(0,max(0,g.height-40),g.width,g.height)
 check(baseline_status.crop(box).tobytes()!=quality_status.crop(box).tobytes(),'Real failed resize displays a changed native error/status line before Save clears it')
 nodes=await layout();check(not nodes.get('Undo',(0,{}))[1].get('enabled',True),'Refused real resize creates no accepted global Undo record')
 check(manifest['quality']['measured_maximum']>.30 or manifest['quality']['measured_rms']>.08,'Independent native control measured reconstruction beyond the actual default policy')
 check(await save(command)==quality_before,'Real Apply of measured quality-refusal input preserves every original field and source byte')
 await command('Close Window');check(app.wait(timeout=15)==0,'Quality-refusal app closes cleanly after exact native Save');app=None
 check(not [p for p,v in pins.items() if sha(p)!=v],'All actual application, inspector, native fixtures and script inputs remain unchanged')
 report(True);d.close();bus.disconnect()
try:asyncio.run(main())
except BaseException as e:diagnostic_snapshot('exception');report(problem=repr(e));raise
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
