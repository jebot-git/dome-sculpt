#!/usr/bin/env python3
"""Actual HDR import/linear controls/physical pointer paint/editable export workflow.
Uses a private Xvfb, D-Bus, HOME and software GL; no physical XR session.
"""
import asyncio,json,os,signal,struct,subprocess,sys,tempfile,time
from pathlib import Path
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X,XK,display,error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK,[])
if os.environ.get('DOME_SOURCE_AUTHORING_BUS')!='1':
    os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'DOME_SOURCE_AUTHORING_BUS':'1'})
root=Path.cwd()
build=root/os.environ.get('DOME_SOURCE_AUTHORING_BUILD','build')
inspector=root/os.environ.get('DOME_SOURCE_AUTHORING_INSPECT',str(build/'dome-sculpt-native-authoring-inspect'))
fixture=root/os.environ.get('DOME_SOURCE_AUTHORING_FIXTURE',str(build/'source-paint-authoring/recovered0.dsculpt'))
out=root/os.environ.get('DOME_SOURCE_AUTHORING_OUT','artifacts/source-paint-authoring-menu');out.mkdir(parents=True,exist_ok=True)
compact=os.environ.get('DOME_SOURCE_AUTHORING_COMPACT')=='1'
checks=[];processes=[]
def check(ok,label):
    checks.append({'passed':bool(ok),'check':label});(out/'report.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['passed'] for x in checks)},indent=2)+'\n');print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
async def wait(fn,seconds=45):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        value=fn()
        if value:return value
        await asyncio.sleep(.04)
    raise TimeoutError('Native painted-export workflow did not reach expected state')
async def main():
    bus=await MessageBus().connect();await bus.request_name('com.canonical.AppMenu.Registrar')
    with tempfile.TemporaryDirectory(prefix='dome-painted-menu-') as home:
        rd,wr=os.pipe();log=(out/'xvfb.log').open('w');processes.append(subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','640x360x24' if compact else '1280x1000x24','-nolisten','tcp'],pass_fds=(wr,),stdout=log,stderr=log));os.close(wr)
        with os.fdopen(rd) as stream:screen=':'+stream.readline().strip()
        env={**os.environ,'HOME':home,'DISPLAY':screen,'FLTK_BACKEND':'x11','XDG_CURRENT_DESKTOP':'Dome','LIBGL_ALWAYS_SOFTWARE':'1','LP_NUM_THREADS':'2','XMODIFIERS':'@im=none'}
        for key in ['WAYLAND_DISPLAY','WAYLAND_SOCKET']:env.pop(key,None)
        for key,suffix in [('XDG_STATE_HOME','state'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_RUNTIME_DIR','runtime')]:env[key]=home+'/'+suffix;Path(env[key]).mkdir(mode=0o700)
        if compact:
            config=Path(env['XDG_CONFIG_HOME'])/'dome/appearance.json';config.parent.mkdir(parents=True,exist_ok=True);config.write_text(json.dumps({'font':'Unifont / Sans-Serif','font_size':16,'font_scale':2}))
        project=Path(home)/'original.dsculpt';project.write_bytes(fixture.read_bytes());original=project.read_bytes();package=Path(home)/'editable.glb';saved=Path(home)/'recovered.dsculpt'
        def inspect(path):return json.loads(subprocess.check_output([str(inspector),str(path)],env=env,text=True))
        baseline=inspect(project);log=(out/'app.log').open('w');app=subprocess.Popen([str(build/'dome-sculpt'),str(project)],cwd=home,env=env,stdout=log,stderr=log);processes.append(app);d=display.Display(screen)
        def named(title):
            for child in d.screen().root.query_tree().children:
                try:
                    if title in str(child.get_wm_name() or ''):return child
                except error.BadWindow:pass
        window=await wait(lambda:named('Dome Sculpt'));window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        def prop(name):
            value=window.get_full_property(d.intern_atom(name),X.AnyPropertyType);return bytes(value.value).decode() if value else None
        service=await wait(lambda:prop('_KDE_NET_WM_APPMENU_SERVICE_NAME'));path=await wait(lambda:prop('_KDE_NET_WM_APPMENU_OBJECT_PATH'));proxy=bus.get_proxy_object(service,path,await bus.introspect(service,path));menu=proxy.get_interface('com.canonical.dbusmenu')
        async def nodes():
            result=await menu.call_get_layout(0,-1,[]);values={}
            def walk(node):
                ident,properties,children=node;properties={k:v.value for k,v in properties.items()};values[properties.get('label','').replace('_','')]=(ident,properties)
                for child in children:walk(child.value)
            walk(result[1]);return values
        async def command(label):
            values=await nodes();await menu.call_event(values[label][0],'clicked',Variant('i',0),0);await asyncio.sleep(.2)
        async def ready():
            for _ in range(1000):
                if (await nodes())['Import mesh...'][1].get('enabled',True):return
                await asyncio.sleep(.04)
            raise TimeoutError('Native asset worker did not finish')
        def key(symbol,down=None):
            for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,d.keysym_to_keycode(XK.string_to_keysym(symbol)))
            d.sync()
        # Serve the real X11 clipboard throughout D-Bus menu calls: toolkit
        # clipboard probing may be synchronous. XSync alone does not acknowledge
        # FLTK input processing; copy the actual text back before submitting.
        async def choose(chooser,target,navigate=False,numeric=False,accept=True,before=None):
            from Xlib import Xatom
            from Xlib.protocol import event
            clipboard=d.intern_atom('CLIPBOARD');utf8=d.intern_atom('UTF8_STRING');targets=d.intern_atom('TARGETS')
            owner=d.screen().root.create_window(0,0,1,1,0,X.CopyFromParent);offered=str(target);copied=[]
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
                for _ in range(200):
                    state=await nodes()
                    if state['Select All'][1].get('enabled',False):break
                    await asyncio.sleep(.025)
                else:raise AssertionError('Actual text focus must enable native Select All before copyback')
                await command('Select All')
                for _ in range(200):
                    state=await nodes()
                    if state['Copy'][1].get('enabled',False):break
                    await asyncio.sleep(.025)
                else:raise AssertionError('Actual selected text must enable native Copy before copyback')
                await command('Copy')
                try:await wait(clipboard_is_application,10)
                except TimeoutError:
                    current=d.get_selection_owner(clipboard);(out/'clipboard-owner-failure.json').write_text(json.dumps({'offered':offered,'sender':owner.id,'actual_owner':current.id if current else None,'expected_application_base':application_base,'menu':await nodes()},indent=2)+'\n')
                    raise AssertionError('Native filename Copy did not transfer clipboard ownership to the actual application client')
                copied.clear();reader=d.screen().root.create_window(0,0,1,1,0,X.CopyFromParent);reader.convert_selection(clipboard,utf8,d.intern_atom('DOME_INPUT_ACK'),X.CurrentTime);d.flush();await wait(lambda:copied,10);reader.destroy();return copied[0]
            async def paste_ack(value,counted=False):
                nonlocal offered
                offered=str(value);owner.set_selection_owner(clipboard,X.CurrentTime);d.sync();chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
                # Only an active, visible Fl_Input_ in this actual modal enables
                # Select All. Its default filename focus avoids guessed pixels.
                for _ in range(200):
                    state=await nodes()
                    if state['Select All'][1].get('enabled',False) and state['Paste'][1].get('enabled',False):break
                    await asyncio.sleep(.05)
                else:raise AssertionError('Native filename focus readiness was not published')
                await command('Select All');await command('Paste');await asyncio.sleep(.2)
                assert chooser.get_attributes().map_state==X.IsViewable,'Universal Paste must not accept a filename before actual Return'
                value_back=await copyback();assert (abs(float(value_back)-float(offered))<1e-8 if numeric else value_back==offered),'Native actual input copy-back differs from offered value'
                with (out/'clipboard-ack.jsonl').open('a') as proof:proof.write(json.dumps({'destination':offered,'actual':value_back,'sender':owner.id,'owner':d.get_selection_owner(clipboard).id,'application_client_base':application_base})+'\n')
                if counted:check((abs(float(value_back)-float(target))<1e-8 if numeric else value_back==str(target)),'Native numeric field acknowledges '+str(target) if numeric else 'Native filename input acknowledges the complete '+target.name+' destination')
            if navigate:
                directory=target.parent/'chooser navigation 日本語';directory.mkdir();await paste_ack(directory)
                key('Return');await asyncio.sleep(.2);assert chooser.get_attributes().map_state==X.IsViewable,'Directory Return navigates without accepting a file'
                directory_back=await copyback();assert Path(directory_back)==directory,'Actual Return must update the native directory field'
                (out/'directory-return.json').write_text(json.dumps({'requested':str(directory),'actual_filename':directory_back,'chooser_stays_open':True},indent=2)+'\n')
            if before is not None:
                value_back=await copyback();print('NUMERIC_FOCUS_DIAGNOSTIC',repr(value_back),'expected',before,flush=True)
                gg=chooser.get_geometry();rr=chooser.get_image(0,0,gg.width,gg.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(gg.width,gg.height),rr.data,'raw','BGRX').save(out/('focus-before-'+str(target)+'.png'))
                check(abs(float(value_back)-before)<1e-6,"Actual focused numeric field begins at expected native value")
            await paste_ack(target,True)
            g=chooser.get_geometry();screen_geometry=d.screen();assert g.x>=0 and g.y>=0 and g.x+g.width<=screen_geometry.width_in_pixels and g.y+g.height<=screen_geometry.height_in_pixels,'Native chooser must fit the actual viewport'
            raw=chooser.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/('numeric-'+str(target)+'.png' if numeric else 'chooser-'+target.name+'.png'))
            state=await nodes();assert state['Select All'][1].get('enabled',False),'Acknowledged native filename lost focus'
            chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();
            if accept:key('Return');await asyncio.sleep(.2)
            server.cancel();await asyncio.gather(server,return_exceptions=True);owner.destroy();d.sync()
        def tap(window,x,y):
            g=window.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
        await ready();(out/'runtime-maps.txt').write_text(Path('/proc/'+str(app.pid)+'/maps').read_text())
        if os.environ.get('DOME_EMISSION_IMPORT','1')=='1':
            # Actual import UI is independently exercised; hide the previous source
            # through its real menu so it cannot occlude the newly active object.
            await command('Visible');await command('Save');await wait(lambda:project.read_bytes()!=original)
            baseline=inspect(project);original=project.read_bytes();prior_count=len(baseline['objects'])
            raw_source=fixture.parent/'raw0.glb';raw_before=raw_source.read_bytes()
            for cancel_import in [True,False]:
                await command('Import mesh...');chooser=await wait(lambda:named('Import model'));await choose(chooser,raw_source)
                options=await wait(lambda:named('Import options'));g=options.get_geometry();row=36 if not compact else 48
                raw=options.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/('import-cancel.png' if cancel_import else 'import-options.png'))
                if cancel_import:
                    options.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return');await wait(lambda:named('Import options') is None)
                    check(project.read_bytes()==original and raw_source.read_bytes()==raw_before,'Actual default-Cancel HDR import options preserve project and source bytes')
                    continue
                # Exact authored control rows: five earlier fields precede resolution.
                tap(options,g.width*.65,row*7+16+row*.4);
                for _ in range(3):key('Up')
                key('Return');await asyncio.sleep(.15)
                tap(options,30,row*8+16+row*.5);await asyncio.sleep(.1)
                tap(options,g.width*.65,g.height-row*1.5+8);await wait(lambda:named('Import options') is None);await ready()
                await command('Save');await wait(lambda:len(inspect(project)['objects'])==prior_count+1)
                (out/'actual-imported.dsculpt').write_bytes(project.read_bytes());(out/'actual-imported.json').write_text(json.dumps(inspect(project),indent=2))
                baseline=inspect(project);original=project.read_bytes();actual=next(x for x in baseline['objects'] if x['id']==baseline['active'])
                check(actual['painted']['version']==4 and actual['painted']['payload']['resolution']==64,'Actual import options publish genuine Source4 with the selected64-pixel editable atlas')
                raw_model=baseline['source_models'][actual['appearance']['asset']]['original_glb'];raw_model=bytes(raw_model['bytes'] if isinstance(raw_model,dict) else raw_model)
                check(raw_model==raw_before and raw_source.read_bytes()==raw_before and actual['material']['emissive']==[8.0,0.01600000075995922,4.0],'Actual HDR Import preserves raw factor/strength bytes and publishes their genuine native radiance product')
        active=next(x for x in baseline['objects'] if x['id']==baseline['active'])
        await command('Object and material...');dialog=await wait(lambda:named('Object and material'))
        g=dialog.get_geometry();row=g.height/24 if not compact else 44
        tap(dialog,g.width*.65,row*1.5);await asyncio.sleep(.1)
        for _ in range(17):key('Tab');await asyncio.sleep(.015)
        await choose(dialog,'24',numeric=True,accept=False,before=float(active['material']['emissive'][0]))
        tap(dialog,g.width*.65,g.height-row);await wait(lambda:named('Object and material') is None)
        await command('Save');await wait(lambda:project.read_bytes()!=original);edited=inspect(project)
        actual=next(x for x in edited['objects'] if x['id']==edited['active'])
        check(actual['material']['emissive'][0]==24,'Actual desktop material control accepts and saves linear radiance above one')
        check(edited['source_materials']==baseline['source_materials'],'Desktop radiance edit preserves complete source bindings and original references')
        await command('Undo');await command('Save');await wait(lambda:inspect(project)==baseline)
        check(project.read_bytes()==original,'Desktop radiance Undo and native Save return the exact original project bytes')
        await command('Brush settings...');dialog=await wait(lambda:named('Paint brush'));g=dialog.get_geometry();row=g.height/20 if not compact else 44
        # Real FLTK choices: select Emission, then navigate to the units choice.
        tap(dialog,g.width*.65,row*1.5);key('Home')
        for _ in range(3):key('Down')
        key('Return');await asyncio.sleep(.15)
        if compact:
            # Keyboard focus traverses the real scrolling controls; no offscreen
            # coordinates are treated as text input in the compact layout.
            tap(dialog,g.width*.65,row*2.5);dialog.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();await asyncio.sleep(.1)
            for _ in range(6):key('Tab');await asyncio.sleep(.08)
            key('space');key('Down');key('Return');await asyncio.sleep(.15)
            raw=dialog.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/'compact-units-focused.png')
            for index,(value,old) in enumerate([('12',.8),('0.002',.1),('3',.02)]):
                key('Tab');await asyncio.sleep(.08);await choose(dialog,value,numeric=True,accept=False,before=old)
        else:
            tap(dialog,g.width*.65,row*8.5);key('Home');key('Down');key('Return');await asyncio.sleep(.15)
            for index,(value,old) in enumerate([('12',.8),('0.002',.1),('3',.02)]):
                tap(dialog,g.width*.65,row*(9.5+index));await asyncio.sleep(.05);await choose(dialog,value,numeric=True,accept=False,before=old)
        tap(dialog,g.width*.65,g.height-row);await wait(lambda:named('Paint brush') is None)
        check(project.read_bytes()==original,'Actual physical brush settings alone do not rewrite native source content')
        await command('Frame all');await ready();await asyncio.sleep(.3)
        # Pick an actually visible chromatic interior pixel on the rendered HDR
        # source, rather than accepting a stroke that misses the object.
        g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);picture=Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX');picture.save(out/'before-native-physical-stroke.png')
        candidates=[]
        for y in range(int(g.height*.25),int(g.height*.75)):
            for x in range(int(g.width*.3),int(g.width*.7)):
                rgb=picture.getpixel((x,y))
                if max(rgb)>150 and max(rgb)-min(rgb)>40 and all(max(picture.getpixel((x+dx,y+dy)))>140 for dx,dy in [(2,0),(-2,0),(0,2),(0,-2)]):candidates.append(((x-g.width/2)**2+(y-g.height/2)**2,x,y))
        assert candidates,'Native source must actually be visible before physical painting'
        _,x,y=min(candidates);window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Shift_L',True);xtest.fake_input(d,X.MotionNotify,x=g.x+x,y=g.y+y);xtest.fake_input(d,X.ButtonPress,1);d.sync();await asyncio.sleep(.15)
        for step in [2,4,6]:xtest.fake_input(d,X.MotionNotify,x=g.x+x+step,y=g.y+y);d.sync();await asyncio.sleep(.04)
        xtest.fake_input(d,X.ButtonRelease,1);key('Shift_L',False);d.sync()
        for _ in range(500):
            if (await nodes())['Undo'][1].get('enabled',False):break
            await asyncio.sleep(.02)
        else:raise AssertionError('Actual native physical pointer stroke must publish Undo')
        await command('Save');await wait(lambda:project.read_bytes()!=original);painted=inspect(project)
        physical=[]
        def planes(node):
            if isinstance(node,dict):
                for tile in node.get('physical_emission_tiles',[]):
                    data=tile['data'];blob=bytes(data['bytes'] if isinstance(data,dict) else data)
                    for offset in range(0,len(blob),28):
                        values=struct.unpack_from('<7f',blob,offset)
                        if values[6]>.001:physical.append(values[3:6])
                for value in node.values():planes(value)
            elif isinstance(node,list):
                for value in node:planes(value)
        planes(painted)
        check(physical and max(v[0] for v in physical)>11.9 and .0019<max(v[1] for v in physical)<.0021,'Actual desktop units/RGB controls create unclamped physical float paint including small linear values')
        check(painted['source_materials']==baseline['source_materials'],'Actual physical pointer painting keeps original source references and factor/strength exact')
        (out/'before-native-paint.dsculpt').write_bytes(original);baseline=painted;original=project.read_bytes();(out/'actual-pointer-paint.dsculpt').write_bytes(original)
        values=await nodes();label='Export editable project (.glb)...'
        check(values[label][1].get('enabled',True),'Editable project export is an actual enabled DomeD universal-menu command')
        await command(label);options=await wait(lambda:named('Editable GLB export'))
        check(options.get_wm_transient_for().id==window.id and options.get_wm_class()[1]==window.get_wm_class()[1],'Portable policy uses a native FLTK modal with shared document identity')
        values=await nodes();check(not values[label][1].get('enabled',True),'Actual policy modal disables duplicate document export commands')
        options.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key('Return');await wait(lambda:named('Editable GLB export') is None)
        check(not package.exists() and project.read_bytes()==original,'Return on default Cancel submits no export and preserves the project')
        await command(label);await wait(lambda:named('Editable GLB export'));await command('Close Window');await wait(lambda:named('Editable GLB export') is None)
        check(project.read_bytes()==original,'Universal Close cancels the policy modal without a model edit')
        await command(label);options=await wait(lambda:named('Editable GLB export'));g=options.get_geometry();raw=options.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff);Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/'options.png')
        row=48 if compact else 36;tap(options,g.width*.65,g.height-row/2-12)
        await wait(lambda:named('Editable GLB export') is None);chooser=await wait(lambda:named('Export editable project'))
        check(chooser.get_wm_transient_for().id==window.id,'Confirmed portable policy opens the actual owned GLB file chooser')
        await choose(chooser,package,True);await wait(lambda:named('Export editable project') is None)
        try:await wait(lambda:package.exists(),90)
        except TimeoutError:
            g=window.get_geometry();raw=window.get_image(0,0,g.width,g.height,X.ZPixmap,0xffffffff)
            Image.frombytes('RGB',(g.width,g.height),raw.data,'raw','BGRX').save(out/'export-timeout.png')
            (out/'export-timeout.json').write_text(json.dumps({'window_title':str(window.get_wm_name()),'files':[str(p.relative_to(home)) for p in Path(home).rglob('*') if p.is_file()],'menu':await nodes()},indent=2)+'\n')
            raise
        emitted=package.read_bytes();n=struct.unpack_from('<I',emitted,12)[0];metadata=json.loads(emitted[20:20+n]);native=metadata['extras']['domeSculptAuthoring']
        check(native['scope']=='wholeScene' and native['fallback']['lossless'] is False,'Actual GUI output declares complete native project scope and an approximate portable view')
        binary=emitted[28+n:];sizes=[]
        for image in metadata.get('images',[]):
            view=metadata['bufferViews'][image['bufferView']];offset=view.get('byteOffset',0);blob=binary[offset:offset+view['byteLength']]
            if blob.startswith(b'\x89PNG\r\n\x1a\n'):sizes.append(struct.unpack_from('>II',blob,16))
        check((512,512) in sizes,'Default native policy really emits the selected512-pixel portable paint images')
        check(inspect(package)==baseline and project.read_bytes()==original,'Actual GUI file worker preserves all editable source, paint, metadata and the original project file')
        await command('Open...');chooser=await wait(lambda:named('Open project or editable package'));await choose(chooser,package)
        await wait(lambda:named('Open project or editable package') is None);await wait(lambda:'Untitled' in str(window.get_wm_name()) and '*' in str(window.get_wm_name()));await ready()
        check(package.read_bytes()==emitted,'Actual Open restores the exported native project as unsaved and preserves the interchange source')
        await command('Save');chooser=await wait(lambda:named('Save sculpt project'));await choose(chooser,saved);await wait(lambda:saved.exists());await wait(lambda:'recovered.dsculpt' in str(window.get_wm_name()) and '*' not in str(window.get_wm_name()))
        check(inspect(saved)==baseline and package.read_bytes()==emitted,'Actual subsequent Save writes the original editable project independently of the portable GLB')
        await command('Close Window');await wait(lambda:app.poll() is not None)
        check(app.returncode==0,'Native export/recovery/Save workflow exits cleanly without sanitizer suppression')
        (out/'editable.glb').write_bytes(emitted);(out/'recovered.dsculpt').write_bytes(saved.read_bytes());d.close()
    bus.disconnect()
try:asyncio.run(main())
finally:
    for process in reversed(processes):
        if process.poll() is None:
            process.terminate()
            try:process.wait(timeout=15)
            except subprocess.TimeoutExpired:process.kill();process.wait()
