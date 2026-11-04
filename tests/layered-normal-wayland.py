#!/usr/bin/env python3
"""Actual native Wayland conversion, ordered Save and compositor-delivered history keys; no physical-headset claim."""
import asyncio,json,math,os,platform,re,signal,socket,struct,subprocess,sys,tempfile,time,urllib.request
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display
from Xlib.ext import xtest
from PIL import Image,ImageChops
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_CANONICAL_WL_BUS')!='1':os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_CANONICAL_WL_BUS':'1'})
root=Path(__file__).resolve().parents[1];build=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',root/'build'));compositor=Path(os.environ['DOME_COMPOSITOR']);out=Path(os.environ['DOME_CANONICAL_WL_OUT']);out.mkdir(parents=True,exist_ok=True);checks=[];processes=[]
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
 with tempfile.TemporaryDirectory(prefix='dome-canonical-wl-',ignore_cleanup_errors=True) as home:
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
  helper=Path(os.environ['DOME_CANONICAL_HELPER']);project=Path(home,'canonical.dsculpt')
  helper_env={**env,'LD_LIBRARY_PATH':os.environ.get('DOME_CANONICAL_CLIENT_LD_LIBRARY_PATH',env.get('LD_LIBRARY_PATH',''))}
  subprocess.run([str(helper),'--ui-fixture',str(project)],env=helper_env,check=True)
  def inspect():return json.loads(subprocess.check_output([str(helper),'--inspect',str(project)],env=helper_env))
  original=inspect()['scene'];report=Path(home,'widgets.json');client=start([os.environ['DOME_CANONICAL_APP'],str(project)],'client',{**env,'WAYLAND_DISPLAY':wayland,'FLTK_BACKEND':'wayland','LD_LIBRARY_PATH':os.environ.get('DOME_CANONICAL_CLIENT_LD_LIBRARY_PATH',env.get('LD_LIBRARY_PATH','')),},cwd=home)
  def named(title):return next((w for w in scene()['windows'] if title in w['title']),None)
  window=await wait(lambda:named('canonical.dsculpt'));(out/'runtime-maps.txt').write_text(Path('/proc/'+str(client.pid)+'/maps').read_text());check(window['app_id']=='org.domed.Sculpt','Actual native Wayland conversion client publishes the Dome Sculpt identity')
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
     if 'Convert to portable normals' in str(result):menu=candidate;break
    except Exception:pass
   if menu is None:await asyncio.sleep(.1)
  check(menu is not None,'Actual native Wayland client exports the universal normal-conversion command')
  async def layout():
   result=await menu.call_get_layout(0,-1,[]);nodes={}
   def walk(node):
    ident,properties,children=node;nodes[properties.get('label',Variant('s','')).value.replace('_','')]=(ident,{k:v.value for k,v in properties.items()})
    for child in children:walk(child.value)
   walk(result[1]);return nodes
  async def command(label):
   for _ in range(1500):
    nodes=await layout()
    if label in nodes and nodes[label][1].get('enabled',True):break
    await asyncio.sleep(.04)
   else:raise TimeoutError(label+' remained unavailable')
   old=project.stat().st_mtime_ns;await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
   if label=='Save':await wait(lambda:project.stat().st_mtime_ns!=old)
   await asyncio.sleep(.15)
  nodes=await layout();check(nodes['Convert to portable normals'][1].get('enabled',True),'Eligible canonical mesh enables its actual Wayland universal-menu action')
  await command('Convert to portable normals');await command('Save');accepted=inspect();converted=accepted['scene']
  check(accepted['converted'] and converted['version']==33,'Actual Wayland callback and ordered Save publish a replayable Scene33 conversion')
  materials=converted['source_materials'];anchor=converted['baked_normal_fields']['origins'][0]
  check(materials['paints'][anchor['paint']]==original['objects'][0]['painted'],'Actual Wayland conversion retains the complete genuine original canonical paint')
  target=converted['objects'][0];old=original['objects'][0]
  check(target['painted']['layers']==old['painted']['layers'] and target['deformation']==old['deformation'] and target['bake_recipe']==old['bake_recipe'] and converted['objects'][1]==original['objects'][1],'Native Wayland conversion preserves sparse layers, skin/morph, recipe and hidden source')
  check(all(target['painted']['baked'][name]==old['painted']['baked'][name]for name in ['color','orm','emission','coverage']),'All other baked roles and coverage remain exact through the Wayland action')
  nodes=await layout();check(not nodes['Convert to portable normals'][1].get('enabled',True),'Converted native Wayland target disables duplicate conversion')
  action({'op':'focus','id':window['id']});await chord('Control_L','z');await asyncio.sleep(.3);await command('Save')
  check(inspect()['scene']==original,'Actual compositor Ctrl+Z restores the complete original native project')
  await chord('Control_L','Shift_L','z');await asyncio.sleep(.3);await command('Save')
  check(inspect()['scene']==converted,'Actual compositor Ctrl+Shift+Z restores the exact converted project')
  await command('Close Window');await wait(lambda:client.poll() is not None)
  check(client.returncode==0,'Native Wayland conversion client closes through normal worker/graphics/font teardown')
  (out/'final-scene.json').write_text(json.dumps(scene(),indent=2));(out/'report.json').write_text(json.dumps({'checks':checks,'complete':True,'physical_headset':False},indent=2)+'\n');d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:
   p.terminate()
   try:p.wait(timeout=8)
   except subprocess.TimeoutExpired:p.kill();p.wait()
