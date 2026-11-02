#!/usr/bin/env python3
"""Private native Wayland/DomeD + real Unix stream; no physical-device claim."""
import asyncio,json,os,platform,re,signal,socket,struct,subprocess,sys,tempfile,time,urllib.request
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display
from Xlib.ext import xtest
from PIL import Image,ImageChops
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_RIG_REST_WL_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_RIG_REST_WL_BUS':'1'})
root=Path(__file__).resolve().parents[1];build=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',root/'build-rig-rest-release')).resolve();compositor=Path(os.environ['DOME_COMPOSITOR']);out=Path(os.environ.get('DOME_RIG_REST_WL_OUT',root/'artifacts/wayland-rig-rest'));out.mkdir(parents=True,exist_ok=True);checks=[];processes=[]
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'complete':False},indent=2)+'\n');print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
async def wait(fn,seconds=60):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  try:
   value=fn()
   if value:return value
  except (OSError,ValueError,KeyError,StopIteration):pass
  await asyncio.sleep(.08)
 raise TimeoutError('Private DomeD projection condition timed out')
def start(args,name,env,**kw):
 log=(out/(name+'.log')).open('w');p=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT,**kw);processes.append(p);return p
async def main():
 with tempfile.TemporaryDirectory(prefix='dome-rig-fit-wl-') as home:
  rd,wr=os.pipe();start(['Xvfb','-displayfd',str(wr),'-screen','0','1600x1000x24','-nolisten','tcp','-noreset'],'xvfb',os.environ.copy(),pass_fds=(wr,));os.close(wr)
  with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
  with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
  icd=next(p for p in (Path('/usr/share/vulkan/icd.d/lvp_icd.json'),Path('/usr/share/vulkan/icd.d/lvp_icd.'+platform.machine()+'.json')) if p.is_file())
  config=Path(home,'config/dome');config.mkdir(parents=True);(config/'keyboard.json').write_text(json.dumps({'ime':False,'layout':'us'}))
  env={**os.environ,'HOME':home,'DISPLAY':screen,'XDG_CONFIG_HOME':home+'/config','XDG_DATA_HOME':home+'/data','XDG_CACHE_HOME':home+'/cache','XDG_STATE_HOME':home+'/state','XDG_RUNTIME_DIR':home,'XDG_CURRENT_DESKTOP':'Dome','XDG_SESSION_TYPE':'wayland','VK_DRIVER_FILES':str(icd),'LIBGL_ALWAYS_SOFTWARE':'1','LIBGL_KOPPER_DISABLE':'true','LP_NUM_THREADS':'2','DOME_RENDER_CAPTURE':str(out/'render.ppm'),'DOME_LOGIN_SESSION':'0','DOME_PREVIEW_PORT':str(port),'DOME_RENDERER':'vulkan','DOME_START_XR':'0','RUST_LOG':'warn','SPNAV_SOCKET':home+'/spnav.sock'}
  for k in ('WAYLAND_DISPLAY','WAYLAND_SOCKET','FLTK_BACKEND','QT_QPA_PLATFORM'):env.pop(k,None)
  host_process=start([str(compositor),'--backend','nested'],'compositor',env);base=f'http://127.0.0.1:{port}'
  def scene():return json.load(urllib.request.urlopen(base+'/api/state',timeout=2))['scene']
  def action(data):urllib.request.urlopen(urllib.request.Request(base+'/api/action',data=json.dumps(data).encode(),headers={'Content-Type':'application/json'}),timeout=2).close()
  await wait(lambda:scene()['source']=='wayland');wayland=re.search(r'Dome Wayland socket: (\S+)',(out/'compositor.log').read_text())[1]
  helper=build/'dome-sculpt-source-rest-ui-fixture';project=Path(home,'humanoid.dsculpt');subprocess.run([str(helper),'--ui-fixture',str(project)],env=env,check=True)
  def inspect():return json.loads(subprocess.check_output([str(helper),'--inspect',str(project)],env=env))
  original=inspect();client=start([os.environ.get('DOME_SPACENAV_APP',str(build/'dome-sculpt')),str(project)],'client',{**env,'WAYLAND_DISPLAY':wayland,'FLTK_BACKEND':'wayland'},cwd=home)
  def named(title):return next((w for w in scene()['windows'] if title in w['title']),None)
  window=await wait(lambda:named('humanoid.dsculpt'));check(window['app_id']=='org.domed.Sculpt','Native Wayland humanoid fitting client publishes the Dome Sculpt identity')
  d=display.Display(screen);host=await wait(lambda:next((w for w in d.screen().root.query_tree().children if 'Dome' in str(w.get_wm_name() or '')),None));host.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();action({'op':'focus','id':window['id']});await asyncio.sleep(.5)
  async def key(name,down=None):
   code=d.keysym_to_keycode(XK.string_to_keysym(name))
   for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,code);d.sync();await asyncio.sleep(.05)
  async def chord(*names):
   for n in names:await key(n,True)
   for n in reversed(names):await key(n,False)
  bus=await MessageBus().connect();db=bus.get_proxy_object('org.freedesktop.DBus','/org/freedesktop/DBus',await bus.introspect('org.freedesktop.DBus','/org/freedesktop/DBus')).get_interface('org.freedesktop.DBus');menu=None
  end=time.monotonic()+60
  while menu is None and time.monotonic()<end:
   for service in await db.call_list_names():
    if not service.startswith(':'):continue
    try:
     path='/org/de_xr/FLTK/Menu1';candidate=bus.get_proxy_object(service,path,await asyncio.wait_for(bus.introspect(service,path),1)).get_interface('com.canonical.dbusmenu');result=await candidate.call_get_layout(0,-1,[])
     if 'Humanoid rig fitting...' in str(result):menu=candidate;break
    except Exception:pass
   if menu is None:await asyncio.sleep(.15)
  assert menu is not None,'No native menu endpoint'
  async def layout():
   nodes={};result=await menu.call_get_layout(0,-1,[])
   def walk(n):
    ident,p,children=n;p={k:v.value for k,v in p.items()};nodes[p.get('label','').replace('_','')]=(ident,p)
    for c in children:walk(c.value)
   walk(result[1]);return nodes
  async def exported(label):
   nodes=await layout();await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0);await asyncio.sleep(.2)
  async def native(heading,label):
   nodes=await layout();top=await menu.call_get_layout(0,1,[]);headings=[c.value[1].get('label',Variant('s','')).value.replace('_','') for c in top[1][2]];index=2+headings.index(heading);count=len(top[1][2])+3
   await chord('Super_L','F10')
   for _ in range((count-index)%count):await key('Left')
   subtree=await menu.call_get_layout(nodes[heading][0],1,[]);rows=[c.value[0] for c in subtree[1][2] if c.value[1].get('type',Variant('s','')).value!='separator' and c.value[1].get('enabled',Variant('b',True)).value]
   await key('Home')
   for _ in range(rows.index(nodes[label][0])):await key('Down')
   await key('Return');await asyncio.sleep(.2)
  def focus():
   action({'op':'focus','id':window['id']});rect=named('humanoid.dsculpt')['desktop'];g=host.get_geometry();xtest.fake_input(d,X.MotionNotify,x=int(g.x+rect['x']+rect['w']/2),y=int(g.y+rect['y']+rect['h']/2));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
  def pixels():
   rect=named('humanoid.dsculpt')['desktop'];return Image.open(out/'render.ppm').transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB').crop((int(rect['x'])+8,int(rect['y'])+8,int(rect['x']+rect['w'])-8,int(rect['y']+rect['h'])-40))
  def difference(a,b):
   delta=ImageChops.difference(a,b);return sum(any(c>8 for c in p) for p in (delta.get_flattened_data() if hasattr(delta,'get_flattened_data') else delta.getdata()))
  def tap(dialog,x,y):
   current=scene();hg=host.get_geometry()
   def placed(ident):
    target=next(w for w in current['windows'] if w['id']==ident);popup=next((p for p in current['popups'] if p['id']==ident),None)
    if popup is None:return target['desktop']
    parent=placed(popup['parent']);anchor=popup['anchor'];return {'x':max(0,min(parent['x']+anchor['x'],hg.width-anchor['w'])),'y':max(36,min(parent['y']+anchor['y'],hg.height-60-anchor['h'])),'w':anchor['w'],'h':anchor['h']}
   rect=placed(dialog['id']);xtest.fake_input(d,X.MotionNotify,x=int(hg.x+rect['x']+x(rect)),y=int(hg.y+rect['y']+y(rect)));d.sync();xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
  async def type_text(value):
   for c in value:
    code=d.keysym_to_keycode(ord(c));shift=d.keycode_to_keysym(code,0)!=ord(c)
    if shift:await key('Shift_L',True)
    xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code);d.sync()
    if shift:await key('Shift_L',False)
  await exported('Humanoid rig fitting...');dialog=await wait(lambda:named('Fit existing rig'))
  check(dialog['app_id']==window['app_id'] and scene()['focused']==dialog['id'],'Rig landmark controls are native FLTK Wayland and retain focused document identity')
  await key('Escape');await wait(lambda:not named('Fit existing rig'));check(inspect()==original,'Native Wayland Escape cancels landmark editing without modifying Scene')
  await exported('Humanoid rig fitting...');dialog=await wait(lambda:named('Fit existing rig'));action({'op':'focus','id':dialog['id']});await asyncio.sleep(.2)
  tap(dialog,lambda r:r['w']*.75,lambda r:162);await chord('Control_L','a');await type_text('.025');await key('Tab');await asyncio.sleep(.2)
  tap(dialog,lambda r:r['w']/2+80,lambda r:r['h']-46);await wait(lambda:not named('Fit existing rig'))
  check((await layout())['Save'][1].get('enabled',True),'Accepted native Wayland fitting leaves ordered Save available')
  stamp=project.stat().st_mtime_ns;await exported('Save');await wait(lambda:project.stat().st_mtime_ns!=stamp);after=inspect()
  check(after['rig_source'] and after['scene']['version']==25,'Actual native Wayland Apply fits existing rig through the worker before following Save')
  old_objects={o['id']:o for o in original['scene']['objects']};new_objects={o['id']:o for o in after['scene']['objects']};source_ids=[ident for ident,o in new_objects.items() if o.get('rig_source') is not None]
  assert len(source_ids)==22,'Missing real original skin/rigid sources'
  for ident in new_objects:
   a=new_objects[ident].copy();b=old_objects[ident].copy()
   if ident in source_ids:
    for field in ('deformation','rig_source','shape_source'):a.pop(field,None);b.pop(field,None)
   for field in set(a)-set(b):
    if a[field] is None:a.pop(field)
   assert a==b,'Source or unrelated mesh/paint/material/placement changed: '+str(ident)
  check(True,'Native Wayland rest fitting preserves exact mesh paint material and placement on all22 real source parts and unrelated objects')
  await exported('Undo');stamp=project.stat().st_mtime_ns;await exported('Save');await wait(lambda:project.stat().st_mtime_ns!=stamp);check(inspect()==original,'DomeD universal Undo restores exact prior complete rig authoring state')
  await exported('Redo');stamp=project.stat().st_mtime_ns;await exported('Save');await wait(lambda:project.stat().st_mtime_ns!=stamp);check(inspect()==after,'Native Wayland Redo and ordered Save/reopen retain fitted rig and source data')
  await exported('Close Window');await wait(lambda:client.poll() is not None);check(client.returncode==0,'Native Wayland rig fitting closes through normal App GL worker and font teardown')
  (out/'report.json').write_text(json.dumps({'checks':checks,'complete':True,'physical_device':False,'backend':'native FLTK Wayland + private canonical d271 DomeD nested compositor'},indent=2)+'\n');d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
