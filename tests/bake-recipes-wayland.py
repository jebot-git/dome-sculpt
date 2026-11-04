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
if os.environ.get('DOME_PROJECTION_WL_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_PROJECTION_WL_BUS':'1'})
root=Path(__file__).resolve().parents[1];build=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',root/'build'));compositor=Path(os.environ['DOME_COMPOSITOR']);out=Path(os.environ['DOME_CAGE_WL_OUT']);out.mkdir(parents=True,exist_ok=True);checks=[];processes=[]
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
 with tempfile.TemporaryDirectory(prefix='dome-cage-wl-',ignore_cleanup_errors=True) as home:
  rd,wr=os.pipe();start(['Xvfb','-displayfd',str(wr),'-screen','0','1600x1000x24','-nolisten','tcp','-noreset'],'xvfb',os.environ.copy(),pass_fds=(wr,));os.close(wr)
  with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
  with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
  icd=next(p for p in (Path('/usr/share/vulkan/icd.d/lvp_icd.json'),Path('/usr/share/vulkan/icd.d/lvp_icd.'+platform.machine()+'.json')) if p.is_file())
  config=Path(home,'config/dome');config.mkdir(parents=True);(config/'keyboard.json').write_text(json.dumps({'ime':False,'layout':'us'}))
  env={**os.environ,'HOME':home,'DISPLAY':screen,'XDG_CONFIG_HOME':home+'/config','XDG_DATA_HOME':home+'/data','XDG_CACHE_HOME':home+'/cache','XDG_RUNTIME_DIR':home,'XDG_CURRENT_DESKTOP':'Dome','XDG_SESSION_TYPE':'wayland','VK_DRIVER_FILES':str(icd),'LIBGL_ALWAYS_SOFTWARE':'1','GIO_USE_VFS':'local','LIBGL_KOPPER_DISABLE':'true','LP_NUM_THREADS':'2','DOME_RENDER_CAPTURE':str(out/'render.ppm'),'DOME_LOGIN_SESSION':'0','DOME_PREVIEW_PORT':str(port),'DOME_RENDERER':'vulkan','DOME_START_XR':'0','DOME_START_TRAY':'0','DOME_START_IME':'0','DOME_SESSION_RESTORE':'0','DBUS_SYSTEM_BUS_ADDRESS':'unix:path=/nonexistent/viewport-test-bus','XR_RUNTIME_JSON':'/nonexistent/viewport-no-headset.json','RUST_LOG':'warn','SPNAV_SOCKET':home+'/spnav.sock'}
  for k in ('WAYLAND_DISPLAY','WAYLAND_SOCKET','FLTK_BACKEND','QT_QPA_PLATFORM'):env.pop(k,None)
  host_process=start([str(compositor),'--backend','nested'],'compositor',env);base=f'http://127.0.0.1:{port}'
  def scene():return json.load(urllib.request.urlopen(base+'/api/state',timeout=2))['scene']
  def action(data):urllib.request.urlopen(urllib.request.Request(base+'/api/action',data=json.dumps(data).encode(),headers={'Content-Type':'application/json'}),timeout=2).close()
  await wait(lambda:scene()['source']=='wayland');wayland=re.search(r'Dome Wayland socket: (\S+)',(out/'compositor.log').read_text())[1]
  helper=Path(os.environ['DOME_CAGE_HELPER']);project=Path(home,'cage.dsculpt');subprocess.run([str(helper),'--ui-fixture',str(project)],env=env,check=True)
  def inspect():return json.loads(subprocess.check_output([str(helper),'--inspect',str(project)],env=env))
  original=inspect()['scene'];report=Path(home,'widgets.json');client=start([os.environ['DOME_CAGE_APP'],str(project)],'client',{**env,'WAYLAND_DISPLAY':wayland,'FLTK_BACKEND':'wayland','LD_PRELOAD':os.environ['DOME_CAGE_PROBE'],'LD_LIBRARY_PATH':os.environ.get('DOME_CAGE_CLIENT_LD_LIBRARY_PATH',env.get('LD_LIBRARY_PATH','')),'DOME_CAGE_WIDGET_REPORT':str(report)},cwd=home)
  def named(title):return next((w for w in scene()['windows'] if title in w['title']),None)
  window=await wait(lambda:named('cage.dsculpt'));(out/'runtime-maps.txt').write_text(Path('/proc/'+str(client.pid)+'/maps').read_text());check(window['app_id']=='org.domed.Sculpt','Native Wayland cage client publishes the Dome Sculpt identity')
  d=display.Display(screen);host=await wait(lambda:next((w for w in d.screen().root.query_tree().children if 'Dome' in str(w.get_wm_name() or '')),None));host.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();action({'op':'focus','id':window['id']});await asyncio.sleep(.4)
  async def key(name,down=None):
   code=d.keysym_to_keycode(XK.string_to_keysym(name))
   for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,code);d.sync();await asyncio.sleep(.06)
  async def chord(*names):
   for n in names:await key(n,True)
   for n in reversed(names):await key(n,False)
  bus=await MessageBus().connect();db=bus.get_proxy_object('org.freedesktop.DBus','/org/freedesktop/DBus',await bus.introspect('org.freedesktop.DBus','/org/freedesktop/DBus')).get_interface('org.freedesktop.DBus');menu=None
  end=time.monotonic()+60
  while menu is None and time.monotonic()<end:
   for service in await db.call_list_names():
    if not service.startswith(':'):continue
    try:
     endpoint='/org/de_xr/FLTK/Menu1';candidate=bus.get_proxy_object(service,endpoint,await asyncio.wait_for(bus.introspect(service,endpoint),1)).get_interface('com.canonical.dbusmenu');result=await candidate.call_get_layout(0,-1,[])
     if 'Bake surface...' in str(result):menu=candidate;break
    except Exception:pass
   if menu is None:await asyncio.sleep(.1)
  check(menu is not None,'Actual native Wayland client exports the universal Bake command')
  async def command(label):
   result=await menu.call_get_layout(0,-1,[]);nodes={}
   def walk(node):
    ident,properties,children=node;nodes[properties.get('label',Variant('s','')).value.replace('_','')]=ident
    for child in children:walk(child.value)
   walk(result[1]);old=project.stat().st_mtime_ns;await menu.call_event(nodes[label],'clicked',Variant('i',0),0)
   if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=old)
   await asyncio.sleep(.15)
  def widget(label):
   try:windows=json.loads(report.read_text())
   except (OSError,ValueError):return None
   def walk(item):
    if item['label']==label:return item
    for c in item.get('children',[]):
     found=walk(c)
     if found:return found
   for w in windows:
    if w['label']=='Bake surface':
     found=walk(w)
     if found:return found
  await command('Bake surface...');dialog=await wait(lambda:named('Bake surface'));action({'op':'focus','id':dialog['id']});choice=await wait(lambda:widget('Cage mesh'));check(choice['choice']==2,'Native Wayland project restores the saved custom-cage recipe')
  for _ in range(20):
   if widget('Cage mesh') and widget('Cage mesh')['focused']:break
   await key('Tab');await asyncio.sleep(.08)
  check(widget('Cage mesh')['focused'],'Actual compositor keyboard input focuses the native cage chooser')
  await key('space');await key('Up');await key('Up');await key('Return');choice=await wait(lambda:widget('Cage mesh') if widget('Cage mesh') and widget('Cage mesh')['choice']==0 else None)
  check('automatic' in choice['selected'],'Actual compositor keyboard changes the custom cage to automatic in the local draft')
  await key('Tab');await wait(lambda:widget('Use last bake')['focused']);await key('space')
  await wait(lambda:widget('Cage mesh')['choice']==2)
  check('envelope' in widget('Cage mesh')['selected'] and abs(widget('Front (mm)')['value']-2/3)<1e-6,'Actual native Wayland Recall restores the custom Unicode cage and saved projection range')
  await chord('Control_L','w');await wait(lambda:named('Bake surface') is None);await command('Save');check(inspect()['scene']==original,'Native Ctrl+W cancels staged cage choice and ordered Save preserves the complete scene')
  await command('Bake surface...');dialog=await wait(lambda:named('Bake surface'));action({'op':'focus','id':dialog['id']});choice=await wait(lambda:widget('Cage mesh'));check(choice['choice']==2,'Reopened Wayland options retain saved recipe after cancelling the recalled draft')
  await chord('Control_L','w');await wait(lambda:named('Bake surface') is None);await command('Close Window');await wait(lambda:client.poll() is not None);check(client.returncode==0,'Native Wayland client closes through normal worker and graphics/font release')
  (out/'final-scene.json').write_text(json.dumps(scene(),indent=2));d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:
   p.terminate()
   try:p.wait(timeout=8)
   except subprocess.TimeoutExpired:p.kill();p.wait()
