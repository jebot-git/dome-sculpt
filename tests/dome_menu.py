#!/usr/bin/env python3
"""Native FLTK/Wayland commands through a private running DomeD universal menu.
Harness setup follows DomeD scripts/test-global-menu.py; no other GUI toolkit.
"""
import asyncio, json, os, platform, re, signal, socket, subprocess, sys, tempfile, time, urllib.request
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X, XK, display
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK, [])
if os.environ.get("DOME_SCULPT_DOME_BUS") != "1":
    os.execvpe("dbus-run-session", ["dbus-run-session", "--", sys.executable, *sys.argv], {**os.environ,"DOME_SCULPT_DOME_BUS":"1"})
root=Path(os.environ.get("DOME_SCULPT_TEST_ROOT",Path(__file__).resolve().parents[1]))
compositor=Path(os.environ.get("DOME_COMPOSITOR",root.parent/"dome-compositor/target/release/dome-compositor"))
out=root/"artifacts/dome-menu";out.mkdir(parents=True,exist_ok=True)
icd=Path(os.environ["DOME_TEST_VULKAN_ICD"]) if "DOME_TEST_VULKAN_ICD" in os.environ else next((p for p in [Path("/usr/share/vulkan/icd.d/lvp_icd.json"),Path("/usr/share/vulkan/icd.d/lvp_icd."+platform.machine()+".json")] if p.is_file()),None)
if icd is None or not icd.is_file():raise RuntimeError("Set DOME_TEST_VULKAN_ICD to the installed software Vulkan ICD manifest")

processes=[];checks=[]
def check(ok,label):
    checks.append({"passed":bool(ok),"check":label});print(("PASS " if ok else "FAIL ")+label,flush=True)
    (out/"report.json").write_text(json.dumps({"checks":checks,"failed":sum(not c["passed"] for c in checks)},indent=2)+"\n")
    assert ok,label
def start(args,name,env,**kw):
    log=(out/(name+".log")).open("w")
    p=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True,**kw);processes.append(p);return p
async def wait(fn,seconds=90):
    until=time.monotonic()+seconds
    while time.monotonic()<until:
        try:
            result=fn()
            if result:return result
        except (OSError,ValueError,KeyError,StopIteration):pass
        await asyncio.sleep(.1)
    raise AssertionError("Private DomeD state timed out; inspect artifacts/dome-menu")
async def main():
    assert compositor.is_file(),"Set DOME_COMPOSITOR to a standalone DomeD executable"
    with tempfile.TemporaryDirectory(prefix="dome-sculpt-host-") as tmp:
        home=Path(tmp);rd,wr=os.pipe()
        start(["Xvfb","-displayfd",str(wr),"-screen","0","1600x1000x24","-nolisten","tcp","-noreset"],"xvfb",os.environ.copy(),pass_fds=(wr,));os.close(wr)
        with os.fdopen(rd) as pipe:screen=":"+pipe.readline().strip()
        with socket.socket() as sock:sock.bind(("127.0.0.1",0));port=sock.getsockname()[1]
        config=home/"config/dome";config.mkdir(parents=True)
        (config/"keyboard.json").write_text(json.dumps({"ime":False,"layout":"us"}))
        apps=home/"data/applications";apps.mkdir(parents=True)
        (apps/"org.domed.Sculpt.desktop").write_text((root/"packaging/org.domed.Sculpt.desktop").read_text())
        env={**os.environ,"HOME":tmp,"DISPLAY":screen,"XDG_CONFIG_HOME":str(home/"config"),"XDG_DATA_HOME":str(home/"data"),"XDG_CACHE_HOME":str(home/"cache"),"XDG_RUNTIME_DIR":tmp,"XDG_CURRENT_DESKTOP":"Dome","XDG_SESSION_TYPE":"wayland","VK_DRIVER_FILES":str(icd),"LIBGL_ALWAYS_SOFTWARE":"1","LIBGL_KOPPER_DISABLE":"true","DOME_RENDER_CAPTURE":str(out/"render.ppm"),"DOME_LOGIN_SESSION":"0","DOME_PREVIEW_PORT":str(port),"DOME_RENDERER":"vulkan","DOME_START_XR":"0","RUST_LOG":"warn"}
        for key in ["WAYLAND_DISPLAY","WAYLAND_SOCKET","FLTK_BACKEND","QT_QPA_PLATFORM","LD_LIBRARY_PATH"]:env.pop(key,None)
        host_process=start([str(compositor),"--backend","nested"],"compositor",env)
        base=f"http://127.0.0.1:{port}"
        def scene():return json.load(urllib.request.urlopen(base+"/api/state",timeout=2))["scene"]
        def action(data):urllib.request.urlopen(urllib.request.Request(base+"/api/action",data=json.dumps(data).encode(),headers={"Content-Type":"application/json"}),timeout=2).close()
        await wait(lambda:scene()["source"]=="wayland")
        log=(out/"compositor.log").read_text();wayland=re.search(r"Dome Wayland socket: (\S+)",log)[1]
        client=start([str(root/"build/dome-sculpt")],"client",{**env,"WAYLAND_DISPLAY":wayland,"FLTK_BACKEND":"wayland"},cwd=tmp)
        def named(title):return next((w for w in scene()["windows"] if title in w["title"]),None)
        window=await wait(lambda:named("Dome Sculpt"))
        check(window["app_id"]=="org.domed.Sculpt","Native Wayland sculpt window publishes the installed Dome Sculpt application identity")
        d=display.Display(screen)
        host=await wait(lambda:next((w for w in d.screen().root.query_tree().children if "Dome" in str(w.get_wm_name() or "")),None))
        host.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();action({"op":"focus","id":window["id"]});await asyncio.sleep(.8)
        async def key(name,down=None):
            code=d.keysym_to_keycode(XK.string_to_keysym(name));assert code,name
            for kind in ([X.KeyPress,X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):xtest.fake_input(d,kind,code);d.sync();await asyncio.sleep(.06)
        async def chord(*names):
            for n in names:await key(n,True)
            for n in reversed(names):await key(n,False)
        bus=await MessageBus().connect()
        db=bus.get_proxy_object("org.freedesktop.DBus","/org/freedesktop/DBus",await bus.introspect("org.freedesktop.DBus","/org/freedesktop/DBus")).get_interface("org.freedesktop.DBus")
        menu=None
        deadline=time.monotonic()+60
        while menu is None and time.monotonic()<deadline:
            for service in await db.call_list_names():
                if not service.startswith(":"):continue
                try:
                    path="/org/de_xr/FLTK/Menu1"
                    proxy=bus.get_proxy_object(service,path,await asyncio.wait_for(bus.introspect(service,path),1))
                    candidate=proxy.get_interface("com.canonical.dbusmenu")
                    await candidate.call_get_layout(0,-1,[]);menu=candidate;break
                except Exception:pass
            if menu is None:await asyncio.sleep(.2)
        assert menu is not None,"No native menu endpoint on the private bus"
        async def layout():
            result=await menu.call_get_layout(0,-1,[]);nodes={}
            def walk(node):
                ident,props,children=node;p={k:v.value for k,v in props.items()};nodes[p.get("label","").replace("_","")]=(ident,p)
                for c in children:walk(c.value)
            walk(result[1]);return nodes
        async def exported(label):
            nodes=await layout();await menu.call_event(nodes[label][0],"clicked",Variant("i",0),0);await asyncio.sleep(.5)
        async def header(index):
            await chord("Super_L","F10")
            # Right descends into a selected submenu (e.g. File/Recovery).
            # Left at the root always changes headers, including behind modals.
            top=await menu.call_get_layout(0,1,[])
            count=len(top[1][2])+3 # System, application and shell Window.
            for _ in range((count-index)%count):await key("Left")
            await asyncio.sleep(.2)
        async def first(index,down=0):
            await header(index);await key("Home")
            for _ in range(down):await key("Down")
            await key("Return");await asyncio.sleep(.5)
        async def native_command(heading,index,label):
            nodes=await layout()
            subtree=await menu.call_get_layout(nodes[heading][0],1,[])
            rows=[]
            for child in subtree[1][2]:
                ident,props,_=child.value
                values={k:v.value for k,v in props.items()}
                if values.get("type")!="separator" and values.get("enabled",True):rows.append(ident)
            await first(index,rows.index(nodes[label][0]))
        await native_command("Scene",6,"Duplicate object")
        nodes=await layout()
        check(nodes["Delete object"][1].get("enabled") is True,"DomeD Super+F10 and header/row navigation activate the actual exported Duplicate command")
        await native_command("Scene",6,"Object and material...")
        dialog=await wait(lambda:named("Object and material"))
        check(dialog["app_id"]==window["app_id"] and scene()["focused"]==dialog["id"],"DomeD focuses the native parameter dialog as part of the same application")
        (out/"dialog.json").write_text(json.dumps(scene(),indent=2))
        hg=host.get_geometry();rect=dialog["desktop"]
        xtest.fake_input(d,X.MotionNotify,x=int(hg.x+rect["x"]+rect["w"]*2/3),y=int(hg.y+rect["y"]+40));d.sync()
        xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync();await asyncio.sleep(.3)
        await exported("Select All");await exported("Copy")
        nodes=await layout()
        check(nodes["Cut"][1].get("enabled") is True and named("Object and material") is not None,"The compositor keeps the focused native field and its text selection while opening universal menus")
        await first(3) # Edit / first enabled item is Cut when selected text has no undo.
        nodes=await layout()
        check(nodes["Select All"][1].get("enabled") is False and named("Object and material") is not None,"A focused Wayland dialog inherits its parent's universal menu and routes Edit Cut to its field")
        await first(3) # Now Undo is enabled before Cut/Copy.
        nodes=await layout()
        check(nodes["Select All"][1].get("enabled") is True,"Universal Edit Undo restores dialog text through the real compositor")
        await native_command("File",2,"Close Window")
        await wait(lambda:named("Object and material") is None)
        check(client.poll() is None,"The compositor's universal File Close cancels the modal dialog and retains the sculpt document")
        await header(9) # Shell-owned Window follows the application's seven exported menus.
        await key("Home");await key("Return");await asyncio.sleep(.4)
        current=next(w for w in scene()["windows"] if w["id"]==window["id"])
        check(not current["xr_visible"],"The universal Window menu retains DomeD's Send Window to Desktop action")
        try:
            im=Image.open(out/"render.ppm").transpose(Image.Transpose.FLIP_TOP_BOTTOM);im.save(out/"universal-menu.png")
        except OSError:pass
        (out/"scene.json").write_text(json.dumps(scene(),indent=2))
        bus.disconnect();d.close()
try:asyncio.run(main())
finally:
    for p in reversed(processes):
        if p.poll() is None:
            try:os.killpg(p.pid,signal.SIGTERM)
            except ProcessLookupError:pass
    for p in reversed(processes):
        try:p.wait(timeout=5)
        except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
