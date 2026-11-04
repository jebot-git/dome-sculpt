#!/usr/bin/env python3
"""Actual FLTK export chooser/Cancel/retry, with a test-only durable-write gate."""
import asyncio,json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_EXPORT_MENU_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_EXPORT_MENU_BUS':'1'})
root=Path(__file__).resolve().parents[1];build=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',root/'build')).resolve()
out=Path(os.environ.get('DOME_EXPORT_MENU_OUT',root/'artifacts/menu-export-cancellation'));out.mkdir(parents=True,exist_ok=True)
compact=os.environ.get('DOME_EXPORT_MENU_COMPACT')=='1'
checks=[];processes=[];failure_diagnostic=None
def report(complete=False):
 (out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['passed'] for x in checks),'complete':complete},indent=2)+'\n')
def check(ok,label):
 checks.append({'passed':bool(ok),'check':label});report();print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
async def wait(predicate,seconds=45):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  result=predicate()
  if result:return result
  await asyncio.sleep(.04)
 if failure_diagnostic:await failure_diagnostic()
 raise TimeoutError('Native export did not reach expected state')
async def main():
 global failure_diagnostic
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-export-menu-') as home:
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','640x360x24' if compact else '1280x900x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log));os.close(wr)
  with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for name in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(name,None)
  for name,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[name]=home+'/'+suffix;Path(env[name]).mkdir(mode=0o700)
  if compact:
   config=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';config.parent.mkdir(parents=True,exist_ok=True);config.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  helper=str(build/'dome-sculpt-export-cancellation-test');project=Path(home,'native.dsculpt');target=Path(home,'Sculpt.glb');gate=Path(home,'gate');gate.mkdir()
  subprocess.run([helper,'--ui-fixture',str(project)],check=True,env=env)
  def inspect():return json.loads(subprocess.check_output([helper,'--inspect',str(project)],env=env))
  original=inspect();env.update(LD_PRELOAD=os.environ.get('DOME_TEST_PRELOAD_PREFIX','')+str(build/'libdome-sculpt-export-cancel-gate.so'),DOME_TEST_EXPORT_GATE_DIR=str(gate),DOME_TEST_EXPORT_OUTPUT=str(target))
  if not os.environ.get('DOME_TEST_PRELOAD_PREFIX') and 'libasan.so' in subprocess.check_output(['ldd',str(build/'dome-sculpt')],text=True):
   asan=next(Path(line.split('=>',1)[1].split()[0]).resolve() for line in subprocess.check_output(['ldd',str(build/'dome-sculpt')],text=True).splitlines() if line.lstrip().startswith('libasan.so') and '=>' in line);assert asan.read_bytes()[:4]==b'\x7fELF';env['LD_PRELOAD']=str(asan)+':'+str(build/'libdome-sculpt-export-cancel-gate.so')
  log=(out/'app.log').open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],env=env,cwd=home,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if title in str(w.get_wm_name() or ''):return w
    except error.BadWindow:pass
  window=await wait(lambda:named('Dome Sculpt'));(out/'runtime-maps.txt').write_text(Path('/proc/'+str(app.pid)+'/maps').read_text());window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
  def prop(name):
   p=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
  service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'))
  menu=bus.get_proxy_object(service,path,await bus.introspect(service,path)).get_interface('com.canonical.dbusmenu')
  async def layout():
   result=await menu.call_get_layout(0,-1,[]);nodes={}
   def walk(node):
    ident,p,children=node;p={k:v.value for k,v in p.items()};nodes[p.get('label','').replace('_','')]=(ident,p)
    for child in children:walk(child.value)
   walk(result[1]);return nodes
  async def ready(label,enabled):
   end=time.monotonic()+45
   while time.monotonic()<end:
    nodes=await layout()
    if bool(nodes[label][1].get('enabled',True))==enabled:return nodes
    await asyncio.sleep(.04)
   raise TimeoutError('Native menu capability did not settle: '+label)
  async def command(label):
   nodes=await layout();assert nodes[label][1].get('enabled',True),label+' disabled'
   await menu.call_event(nodes[label][0],'clicked',Variant('i',0),0)
  def key(name):
   code=d.keysym_to_keycode(XK.string_to_keysym(name));xtest.fake_input(d,X.KeyPress,code);xtest.fake_input(d,X.KeyRelease,code);d.sync()
  def capture(name):
   g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/name)
  async def diagnose():
   info={'app_returncode':app.poll(),'target_exists':target.exists(),'gate_files':[p.name for p in gate.iterdir()],'home_files':[p.name for p in Path(home).iterdir()],'windows':[]}
   for w in d.screen().root.query_tree().children:
    try:
     g=w.get_geometry();info['windows'].append({'title':str(w.get_wm_name()),'class':str(w.get_wm_class()),'geometry':[g.x,g.y,g.width,g.height],'mapped':w.get_attributes().map_state})
     if 'Export glTF scene' in str(w.get_wm_name()):
      raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/'failure-chooser.png')
    except Exception as e:info['windows'].append({'error':str(e)})
   try:info['menu_enabled']={k:bool(v[1].get('enabled',True)) for k,v in (await asyncio.wait_for(layout(),2)).items()}
   except Exception as e:info['menu_error']=str(e)
   (out/'failure-state.json').write_text(json.dumps(info,indent=2)+'\n')
  async def filename(chooser):
   target_path=target
   from Xlib import Xatom
   from Xlib.protocol import event
   clipboard=d.intern_atom('CLIPBOARD');utf8=d.intern_atom('UTF8_STRING');targets=d.intern_atom('TARGETS')
   owner=d.screen().root.create_window(0,0,1,1,0,X.CopyFromParent);offered=str(target_path);copied=[]
   async def serve():
       while True:
           while d.pending_events():
               request=d.next_event()
               if request.type==X.MappingNotify:d.refresh_keyboard_mapping(request)
               if request.type==X.SelectionNotify and request.property!=X.NONE:
                   received=request.requestor.get_full_property(request.property,utf8)
                   if received:copied.append(bytes(received.value).decode())
               if request.type==X.SelectionRequest:
                   prop=request.property or request.target
                   if request.target==targets:request.requestor.change_property(prop,Xatom.ATOM,32,[utf8,Xatom.STRING,targets])
                   elif request.target in [utf8,Xatom.STRING]:request.requestor.change_property(prop,request.target,8,offered.encode())
                   else:prop=X.NONE
                   request.requestor.send_event(event.SelectionNotify(time=request.time,requestor=request.requestor,selection=request.selection,target=request.target,property=prop),propagate=False);d.flush()
           await asyncio.sleep(.01)
   server=asyncio.create_task(serve());mask=d.display.info.resource_id_mask;application_base=window.id&~mask
   def clipboard_is_application():
       current=d.get_selection_owner(clipboard)
       return current and current.id!=owner.id and current.id&~mask==application_base
   async def copyback():
       await command('Select All');await command('Copy')
       try:await wait(clipboard_is_application,10)
       except TimeoutError:
           current=d.get_selection_owner(clipboard);(out/'clipboard-owner-failure.json').write_text(json.dumps({'offered':offered,'sender':owner.id,'actual_owner':current.id if current else None,'expected_application_base':application_base,'menu':await layout()},indent=2)+'\n')
           raise AssertionError('Native filename Copy did not transfer clipboard ownership to the actual application client')
       copied.clear();reader=d.screen().root.create_window(0,0,1,1,0,X.CopyFromParent);reader.convert_selection(clipboard,utf8,d.intern_atom('DOME_INPUT_ACK'),X.CurrentTime);d.flush();await wait(lambda:copied,10);reader.destroy();return copied[0]
   async def paste_ack(value,counted=False):
       nonlocal offered
       offered=str(value);owner.set_selection_owner(clipboard,X.CurrentTime);d.sync();chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
       # Only an active, visible Fl_Input_ in this actual modal enables
       # Select All. Its default filename focus avoids guessed pixels.
       for _ in range(200):
           state=await layout()
           if state['Select All'][1].get('enabled',False) and state['Paste'][1].get('enabled',False):break
           await asyncio.sleep(.05)
       else:raise AssertionError('Native filename focus readiness was not published')
       await command('Select All');await command('Paste');await asyncio.sleep(.2)
       assert chooser.get_attributes().map_state==X.IsViewable,'Universal Paste must not accept a filename before actual Return'
       value_back=await copyback();assert value_back==offered,'Native filename copy-back differs from the offered destination'
       with (out/'clipboard-ack.jsonl').open('a') as proof:proof.write(json.dumps({'destination':offered,'actual':value_back,'sender':owner.id,'owner':d.get_selection_owner(clipboard).id,'application_client_base':application_base})+'\n')
       assert value_back==str(target),'Native filename ACK must equal actual export destination'
   await paste_ack(target_path,True)
   g=chooser.get_geometry();screen_geometry=d.screen();assert g.x>=0 and g.y>=0 and g.x+g.width<=screen_geometry.width_in_pixels and g.y+g.height<=screen_geometry.height_in_pixels,'Native chooser outside viewport'
   chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return');await asyncio.sleep(.2);server.cancel();await asyncio.gather(server,return_exceptions=True);owner.destroy();d.sync()
  failure_diagnostic=diagnose
  await ready('Cancel background work',False)
  check(True,'Native universal menu initially disables Cancel background work without an export')
  await command('Export glTF (.glb)...');chooser=await wait(lambda:named('Export glTF scene'))
  check(chooser.get_wm_transient_for().id==window.id and chooser.get_wm_class()[1]==window.get_wm_class()[1],'Actual export chooser retains document ownership and DomeD application identity')
  await command('Close Window');await wait(lambda:named('Export glTF scene') is None)
  check(not target.exists() and inspect()==original,'Universal Close cancels the native export chooser before submitting any output')
  await command('Export glTF (.glb)...');chooser=await wait(lambda:named('Export glTF scene'));await filename(chooser)
  await wait(lambda:(gate/'entered').exists());nodes=await ready('Cancel background work',True);capture('export-running.png')
  check(not target.exists() and nodes['Duplicate object'][1].get('enabled') and nodes['Save'][1].get('enabled'),'A real export pauses after temporary fsync while native authoring and ordered Save remain available')
  await command('Duplicate object');await ready('Undo',True);await command('Undo');await ready('Redo',True)
  stamp=project.stat().st_mtime_ns;await command('Save');await command('Cancel background work');await ready('Cancel background work',False)
  (gate/'release').touch();await wait(lambda:project.stat().st_mtime_ns!=stamp)
  check(not target.exists() and not list(Path(home).glob('Sculpt.glb.tmp-*')),'Actual universal Cancel stops replacement and removes the incomplete export temporary')
  check(inspect()==original and (await layout())['Redo'][1].get('enabled') and app.poll() is None,'The following Save completes with exact original scene and preserved authoring Redo')
  await command('Export glTF (.glb)...');chooser=await wait(lambda:named('Export glTF scene'));await filename(chooser)
  await wait(lambda:target.exists());await ready('Cancel background work',False)
  subprocess.run([helper,'--validate-glb',str(target)],check=True,env={k:v for k,v in env.items() if k!='LD_PRELOAD'})
  check(inspect()==original and app.poll() is None,'Retry through the actual native chooser produces an independently parsed GLB and preserves the project')
  capture('export-complete.png');await command('Close Window');await wait(lambda:app.poll() is not None);check(app.returncode==0,'Actual native Close drains and exits cleanly with no live export');report(True);d.close();bus.disconnect()
try:asyncio.run(main())
finally:
 for p in reversed(processes):
  if p.poll() is None:p.terminate()
 for p in reversed(processes):
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
