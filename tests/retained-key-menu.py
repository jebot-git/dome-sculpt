#!/usr/bin/env python3
"""Real retained-domain key workflow; private FLTK/Xvfb/D-Bus, no HMD."""
import asyncio,copy,hashlib,json,os,shutil,signal,subprocess,sys,tempfile,time,traceback
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image,ImageChops
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_RETAINED_KEY_BUS')!='1':
 os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_RETAINED_KEY_BUS':'1'})
root=Path(__file__).resolve().parents[1]
build=Path(os.environ.get('DOME_SCULPT_TEST_BUILD',root/'artifacts/9050-binaries')).resolve()
fixture=Path(os.environ.get('DOME_RETAINED_KEY_FIXTURE',root.parent/'probe-retained-key-c79/artifacts/workflow-cancel-output/uv24.dsculpt')).resolve()
compact=os.environ.get('DOME_RETAINED_KEY_COMPACT')=='1'
out=root/'artifacts'/os.environ.get('DOME_RETAINED_KEY_OUT','retained-key-compact' if compact else 'retained-key-normal');out.mkdir(parents=True,exist_ok=True)
checks=[];processes=[];stage='initial';failure=None
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def report(complete=False):
 (out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['passed'] for c in checks),'complete':complete,'stage':stage,'error':failure},indent=2)+'\n')
def mark(value):
 global stage;stage=value;report();print('STAGE',stage,flush=True)
def check(ok,message):
 checks.append({'passed':bool(ok),'check':message});report();print(('PASS ' if ok else 'FAIL ')+message,flush=True);assert ok,message
async def wait(predicate,seconds=90,label='native condition'):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=predicate()
  if value:return value
  await asyncio.sleep(.04)
 raise TimeoutError(label+' timed out at '+stage)
def active(scene):return next(o for o in scene['objects'] if o['id']==scene['active'])
def payload(obj):return obj['painted']['payload'] if obj['painted'].get('version')==2 else obj['painted']
def targets(scene):return active(scene)['deformation']['morphs']['targets']
def rig(obj):
 result=copy.deepcopy(obj['deformation'])
 for key in ('morphs','morph_weights','expressions'):result.pop(key,None)
 return result
def first_shape(scene):
 pool=scene['polygon_sources'];index=active(scene)['shape_source'];seen=set()
 while index is not None:
  assert index not in seen,'Cyclic first shape capsule';seen.add(index);item=pool['origins'][index]
  if item['shape_source'] is None:
   result={k:v for k,v in item.items() if k not in ('volume','painted','polygon','shape_source','rig_source')}
   result['volume_payload']=pool['volumes'][item['volume']]
   result['paint_payload']=pool['paints'][item['painted']] if item['painted'] is not None else None
   if item['polygon'] is not None:
    authority=pool['authorities'][item['polygon']];result['topology']=authority['topology'];result['polygon_active']=authority['active']
   return result
  index=item['shape_source']
 raise AssertionError('Original first shape capsule lost')
def current_frames(scene):
 bindings=scene['source_materials'];entry=bindings['bindings'][active(scene)['material_bindings']];pool=bindings['normal_frames'];record=copy.deepcopy(pool['sets'][entry['frames']])
 for role in ('pbr','mtoon','canonical'):record[role]=pool['frames'][record[role]] if record[role] is not None else None
 return record
def preserve(before,after,label):
 a,b=active(before),active(after)
 check(b['painted']==a['painted'] and rig(b)==rig(a),label+' preserves complete retained paint, normal event, skin/rest and saved pose')
 check(after['source_models']==before['source_models'] and first_shape(after)==first_shape(before),label+' retains original raw source bytes and the original first shape capsule witness')
 check(b.get('rig_source')==a.get('rig_source'),label+' does not replace or invent a first rig source')
async def main():
 inputs={str(fixture):sha(fixture),str(build/'dome-sculpt'):sha(build/'dome-sculpt'),str(build/'dome-sculpt-prepared-normal-shapes-test'):sha(build/'dome-sculpt-prepared-normal-shapes-test'),str(Path(__file__).resolve()):sha(__file__)}
 (out/'before.json').write_text(json.dumps(inputs,indent=2)+'\n')
 check(inputs[str(fixture)]=='d6deb7b47165a960f92e644b68d6221e0bb3724634c267280c05021c2beb9b2a','UI input is the exact actual terminal broad-proof retained Scene24 fixture')
 bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
 with tempfile.TemporaryDirectory(prefix='dome-retained-key-ui-') as home:
  mark('private-session')
  rd,wr=os.pipe();log=(out/'xvfb.log').open('w');xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','640x360x24' if compact else '1280x900x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log);processes.append(xvfb);os.close(wr)
  with os.fdopen(rd) as pipe:screen=':'+pipe.readline().strip()
  env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
  for k in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(k,None)
  for k,suffix in [('XDG_CONFIG_HOME','config'),('XDG_STATE_HOME','state'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime')]:env[k]=home+'/'+suffix;Path(env[k]).mkdir(mode=0o700)
  if compact:
   config=Path(home,'config/dome/appearance.json');config.parent.mkdir(parents=True);config.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
  project=Path(home,'retained-keys.dsculpt');shutil.copyfile(fixture,project);helper=build/'dome-sculpt-prepared-normal-shapes-test'
  def inspect():
   data=json.loads(subprocess.check_output([str(helper),'--inspect',str(project)],env=env));(out/'latest-inspect.json').write_text(json.dumps(data));return data
  original=inspect();(out/'original.json').write_text(json.dumps(original));check(original['version']==24 and original['source_materials']['version']==9 and 'paint_sampling' in original and payload(active(original))['normal_projection']['version']==2,'Fixture owns actual paint24/material9 and independently retained normal21 domains')
  d=display.Display(screen);window=None;menu=None;app=None
  def named(title):
   for w in d.screen().root.query_tree().children:
    try:
     if w.get_attributes().map_state==X.IsViewable and title in str(w.get_wm_name() or ''):return w
    except error.BadWindow:pass
  async def launch(label):
   nonlocal window,menu,app
   log=(out/('app-'+label+'.log')).open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],env=env,cwd=home,stdout=log,stderr=log);processes.append(app);window=await wait(lambda:named('Dome Sculpt'),120,'main FLTK window');window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
   def prop(name):
    p=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(p.value).decode() if p else None
   service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));menu=bus.get_proxy_object(service,path,await bus.introspect(service,path)).get_interface('com.canonical.dbusmenu')
  async def nodes():
   found={};result=await menu.call_get_layout(0,-1,[])
   def walk(n):
    ident,p,children=n;p={k:v.value for k,v in p.items()};found[p.get('label','').replace('_','')]=(ident,p)
    for child in children:walk(child.value)
   walk(result[1]);return found
  async def ready(label,enabled=True):
   end=time.monotonic()+120
   while time.monotonic()<end:
    n=await nodes()
    if label in n and n[label][1].get('enabled',True)==enabled:return n
    await asyncio.sleep(.04)
   raise TimeoutError('Menu readiness '+label+' at '+stage)
  async def command(label):
   n=await ready(label);await menu.call_event(n[label][0],'clicked',Variant('i',0),0)
  async def save(expected_count=None):
   stamp=project.stat().st_mtime_ns;await command('Save');await wait(lambda:project.stat().st_mtime_ns!=stamp,120,'atomic native Save');result=inspect()
   if expected_count is not None:check(len(targets(result))==expected_count,'Saved native target count matches the accepted menu/gesture operation')
   return result
  def key(name,down=None):
   code=d.keysym_to_keycode(XK.string_to_keysym(name))
   for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,code)
   d.sync()
  def tap(w,x,y):
   g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
  def pixels(w):
   g=w.get_geometry();raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);return Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX')
  await launch('initial');await ready('New shapekey...')
  mark('default-modal-cancel');await command('New shapekey...');dialog=await wait(lambda:named('New shapekey'));check(dialog.get_wm_transient_for().id==window.id,'Retained-key controls are a real document-owned FLTK modal using universal-menu identity');dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return');await wait(lambda:named('New shapekey') is None);check(await save()==original,'Default Return cancels retained-domain key creation with exact complete source state')
  mark('native-create');await command('New shapekey...');dialog=await wait(lambda:named('New shapekey'));await asyncio.sleep(.2);g=dialog.get_geometry();initial_dialog=pixels(dialog);initial_dialog.save(out/'new-key.png')
  if compact:
   check(g.width<=600 and g.height<=320,'Canonical large-bitmap-font key controls fit the compact native desktop');xtest.fake_input(d,X.MotionNotify,x=g.x+g.width//2,y=g.y+g.height//2)
   for _ in range(9):xtest.fake_input(d,X.ButtonPress,5);xtest.fake_input(d,X.ButtonRelease,5)
   d.sync();await wait(lambda:pixels(dialog).tobytes()!=initial_dialog.tobytes());scrolled=pixels(dialog);scrolled.save(out/'new-key-scrolled.png');check(scrolled.crop((0,g.height-85,g.width,g.height)).tobytes()==initial_dialog.crop((0,g.height-85,g.width,g.height)).tobytes(),'Compact content scrolls while genuine Cancel/Apply controls remain fixed')
  tap(dialog,g.width*.75,g.height-(46 if compact else 32)*2+24);await wait(lambda:named('New shapekey') is None);created=await save(len(targets(original))+1);await ready('Duplicate shapekey...');preserve(original,created,'Native Create');created_frames=current_frames(created);(out/'created.json').write_text(json.dumps(created))
  await command('Undo');check(await save()==original,'Universal Undo restores the exact pre-Create retained source and domain pools');await command('Redo');check(await save()==created,'Universal Redo restores the exact created target, frames and ownership graph')
  # Undo clamps the ephemeral selection when it removes the new slot; Redo
  # restores data, not selection. Select the existing slot through the real
  # editor rather than assuming the last-created target remained selected.
  async def select_existing(label):
   mark('native-existing-target-selection-'+label);await command('Rename shapekey...');dialog=await wait(lambda:named('Rename shapekey'));g=dialog.get_geometry();row=46 if compact else 32
   visible={w.id for w in d.screen().root.query_tree().children if w.get_attributes().map_state==X.IsViewable}
   pixels(dialog).save(out/('target-choice-before-'+label+'.png'));tap(dialog,g.width*.5,row*4 if compact else row*3+row*.35)
   def popup_window():
    for w in d.screen().root.query_tree().children:
     try:
      a=w.get_attributes()
      if w.id not in visible and a.map_state==X.IsViewable and a.override_redirect:return w
     except error.BadWindow:pass
   popup=await wait(popup_window,10,'native Source target popup');pg=popup.get_geometry();pixels(popup).save(out/('target-popup-'+label+'.png'));tap(popup,pg.width*.5,pg.height*.75)
   def popup_closed():
    try:return popup.get_attributes().map_state!=X.IsViewable
    except error.BadWindow:return True
   await wait(popup_closed)
   check(named('Rename shapekey') is not None,'Existing target selection clicks the real Source target popup without accepting or dismissing the modal');pixels(dialog).save(out/('selected-existing-target-'+label+'.png'));(out/'selected-existing-target.json').write_text(json.dumps({'index':len(targets(created))-1,'name':targets(created)[-1]['name']}));tap(dialog,g.width*.75,g.height-row*2+24);await wait(lambda:named('Rename shapekey') is None);check(await save()==created,'Selecting the existing new key with its unchanged name preserves the exact complete native Scene')
  await select_existing('history');await command('Undo');check(await save()==original,'Unchanged-name selection adds no Undo entry: one Undo still removes the real Create');await command('Redo');check(await save()==created,'Redo after the selection-only history check restores the exact real Create');await select_existing('for-stroke')
  mark('native-selected-key-replace');await command('Sculpt selected shapekey');check(await save()==created,'Entering actual selected-key sculpt mode changes no saved weights, paint or source')
  for _ in range(8):await command('Larger brush')
  await asyncio.sleep(.2);g=window.get_geometry();before=pixels(window);before.save(out/'before-stroke.png');view=before.crop((int(g.width*.1),35,int(g.width*.9),g.height-55));mask=Image.new('L',view.size);mask.putdata([255 if max(p)-min(p)>18 and max(p)>70 else 0 for p in view.getdata()]);bounds=mask.getbbox();check(bounds is not None,'Actual retained-domain keyed mesh has a visible colored native paint surface');x0,y0,x1,y1=bounds;x0+=int(g.width*.1);x1+=int(g.width*.1);y0+=35;y1+=35
  points=[((x0+x1)//2,(y0+y1)//2),(x1-4,y0+4),(x1-4,y1-4),(x0+4,y1-4),(x0+4,y0+4),((x0+x1)//2,(y0+y1)//2)]
  async def stroke(cancel=False):
   window.set_input_focus(X.RevertToParent,X.CurrentTime);key('Shift_L',True);xtest.fake_input(d,X.MotionNotify,x=g.x+points[0][0],y=g.y+points[0][1]);xtest.fake_input(d,X.ButtonPress,1);d.sync();await ready('New shapekey...',False)
   for x,y in points[1:]:xtest.fake_input(d,X.MotionNotify,x=g.x+x,y=g.y+y);d.sync();await asyncio.sleep(.06)
   if cancel:key('Escape')
   xtest.fake_input(d,X.ButtonRelease,1);key('Shift_L',False);d.sync();await ready('New shapekey...')
  await stroke();await asyncio.sleep(.5);pixels(window).save(out/'stroke-result-before-save.png');sculpted=await save(len(targets(created)));selected=len(targets(created))-1;old,new=active(created),active(sculpted);check(targets(sculpted)[selected]!=targets(created)[selected] and targets(sculpted)[:selected]==targets(created)[:selected],'Actual native Shift/left gesture replaces the selected key without changing prior targets');preserve(created,sculpted,'Native Replace');check(new['deformation']['morph_weights']==old['deformation']['morph_weights'] and current_frames(sculpted)!=created_frames,'Native Replace updates its real role-frame attributes while keeping saved mixed weights');await asyncio.sleep(.2);after=pixels(window);after.save(out/'after-stroke.png');check(sum(max(abs(a[i]-b[i]) for i in range(3))>12 for a,b in zip(before.getdata(),after.getdata()))>50,'Actual native canvas displays changed selected-key geometry/shading')
  await command('Undo');check(await save()==created,'One universal Undo restores complete pre-stroke retained paint/keys/source');await command('Redo');check(await save()==sculpted,'One universal Redo restores exact selected-key geometry and normal-frame endpoint')
  mark('native-gesture-cancel');history_flags=await nodes();await stroke(True);check(await save()==sculpted,'Actual Escape during the next held key stroke restores exact native Scene and all ownership pools');flags=await nodes();check(flags['Undo'][1].get('enabled',True)==history_flags['Undo'][1].get('enabled',True) and flags['Redo'][1].get('enabled',True)==history_flags['Redo'][1].get('enabled',True),'Cancelled native key gesture does not create or consume Undo/Redo history');await command('Sculpt selected shapekey');check(await save()==sculpted,'Finishing key preview preserves accepted source, paint and original saved mix')
  mark('graceful-close-reopen');await command('Close Window');await wait(lambda:app.poll() is not None,30,'graceful native Close');check(app.returncode==0,'Universal Close performs normal frontend/worker teardown');await wait(lambda:named('Dome Sculpt') is None);await launch('reopened');await ready('New shapekey...');check(inspect()==sculpted,'A new real native frontend reopens exact retained key/normal/paint/source data')
  await command('Rename shapekey...');dialog=await wait(lambda:named('Rename shapekey'));dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Control_L',True);key('w');key('Control_L',False);await wait(lambda:named('Rename shapekey') is None);check(await save()==sculpted and app.poll() is None,'DomeD Ctrl+W cancels owned modal controls after reopen without closing or mutating the document')
  await command('Close Window');await wait(lambda:app.poll() is not None,30,'final graceful Close');check(app.returncode==0,'Final universal-menu Close drains the accepted native application cleanly');d.close();bus.disconnect()
 after={p:sha(p) for p in inputs};(out/'after.json').write_text(json.dumps(after,indent=2)+'\n');check(after==inputs,'Exact retained fixture, verified application/inspector and harness bytes remain unchanged');mark('complete');report(True)
try:asyncio.run(main())
except Exception as exc:
 failure=str(exc);report();(out/'exception.txt').write_text(traceback.format_exc());raise
finally:
 for process in reversed(processes):
  if process.poll() is None:process.terminate()
 for process in reversed(processes):
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.kill();process.wait()
