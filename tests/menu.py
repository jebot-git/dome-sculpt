#!/usr/bin/env python3
"""Exercise real FLTK DBusMenu export with a private host/bus/display."""

import asyncio, json, os, signal, subprocess, sys, tempfile, time
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X, XK, display, error
from Xlib.ext import xtest
from PIL import Image

signal.pthread_sigmask(signal.SIG_SETMASK, [])
if os.environ.get("DOME_SCULPT_MENU_BUS") != "1":
    os.execvpe(
        "dbus-run-session",
        ["dbus-run-session", "--", sys.executable, *sys.argv],
        {**os.environ, "DOME_SCULPT_MENU_BUS": "1"},
    )
root = Path(__file__).resolve().parents[1]
bake_compact=os.environ.get("DOME_MENU_BAKE_COMPACT")=="1"
rig_compact = os.environ.get("DOME_MENU_RIG_COMPACT") == "1"
retopo_compact = os.environ.get("DOME_MENU_RETOPO_COMPACT") == "1"
retopo_flow = os.environ.get("DOME_MENU_RETOPO") == "1" or retopo_compact
compact = os.environ.get("DOME_MENU_VOXEL_COMPACT") == "1" or bake_compact or rig_compact or retopo_compact
rig_flow = os.environ.get("DOME_MENU_RIG") == "1" or rig_compact
export_flow = os.environ.get("DOME_MENU_EXPORT") == "1"
bake_cancel = os.environ.get("DOME_MENU_BAKE_CANCEL") == "1"
bake_flow = os.environ.get("DOME_MENU_BAKE") == "1" or bake_cancel or bake_compact
if sum((rig_flow, export_flow, bake_flow, retopo_flow)) > 1:
    raise ValueError("Select one menu fixture flow: rig, export or bake")
out = root / ("artifacts/menu-rig-compact" if rig_compact else "artifacts/menu-export" if export_flow else "artifacts/menu-bake-compact" if bake_compact else "artifacts/menu-bake-cancel" if bake_cancel else "artifacts/menu-bake" if bake_flow else "artifacts/menu-rig" if rig_flow else "artifacts/menu-voxel-compact" if compact else "artifacts/menu")
if retopo_flow:
    out = root / ("artifacts/menu-retopo-compact" if retopo_compact else "artifacts/menu-retopo")
out.mkdir(parents=True, exist_ok=True)
checks = []
processes = []


def check(ok, label):
    checks.append({"passed": bool(ok), "check": label})
    print(("PASS " if ok else "FAIL ") + label, flush=True)
    (out / "report.json").write_text(
        json.dumps(
            {"checks": checks, "failed": sum(not c["passed"] for c in checks)}, indent=2
        )
        + "\n"
    )
    assert ok, label


async def wait(fn, timeout=30):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        value = fn()
        if value:
            return value
        await asyncio.sleep(0.1)
    raise AssertionError("UI state timed out")


async def main():
    bus = await MessageBus().connect()
    await bus.request_name("com.canonical.AppMenu.Registrar")
    with tempfile.TemporaryDirectory(prefix="dome-sculpt-menu-") as home:
        rd, wr = os.pipe()
        log = (out / "xvfb.log").open("w")
        xvfb = subprocess.Popen(
            [
                "Xvfb",
                "-displayfd",
                str(wr),
                "-screen",
                "0",
                "640x360x24" if compact else "1280x900x24",
                "-nolisten",
                "tcp",
            ],
            pass_fds=(wr,),
            stdout=log,
            stderr=log,
        )
        processes.append(xvfb)
        os.close(wr)
        with os.fdopen(rd) as f:
            screen = ":" + f.readline().strip()
        env = {
            **os.environ,
            "HOME": home,
            "XDG_STATE_HOME": home + "/state",
            "XDG_CONFIG_HOME": home + "/config",
            "XDG_CACHE_HOME": home + "/cache",
            "XDG_DATA_HOME": home + "/data",
            "XDG_RUNTIME_DIR": home,
            "DISPLAY": screen,
            "XDG_CURRENT_DESKTOP": "Dome",
            "FLTK_BACKEND": "x11",
            "LIBGL_ALWAYS_SOFTWARE": "1",
            "XMODIFIERS": "@im=none",
        }
        for key in ("WAYLAND_DISPLAY", "WAYLAND_SOCKET"):
            env.pop(key, None)
        if compact:
            config = Path(home, "config/dome/appearance.json")
            config.parent.mkdir(parents=True)
            config.write_text(json.dumps({"font": "Unifont / Sans-Serif",
                                         "font_size": 16, "font_scale": 2}))
        log = (out / "app.log").open("w")
        arguments = [str(root / "build/dome-sculpt")]
        fixture = Path(home, "rig.dsculpt")
        if retopo_flow:
            fixture = Path(home, "retopo.dsculpt")
            subprocess.run([str(root / "build/dome-sculpt-retopo-authoring-test"), "--ui-fixture", str(fixture)], check=True)
            arguments.append(str(fixture))
        elif rig_flow or export_flow:
            fixture_tool = "dome-sculpt-document-export-test" if export_flow else "dome-sculpt-rig-edit-test"
            subprocess.run([str(root / "build" / fixture_tool), "--ui-fixture", str(fixture)], check=True)
            arguments.append(str(fixture))
        elif bake_flow:
            fixture=Path(home,"bake.dsculpt")
            subprocess.run([str(root / "build/dome-sculpt-bake-controls-test"),"--ui-large-fixture" if bake_cancel else "--ui-fixture",str(fixture)],check=True)
            arguments.append(str(fixture))
        app = subprocess.Popen(
            arguments, env=env, cwd=home, stdout=log, stderr=log
        )
        processes.append(app)
        d = display.Display(screen)

        def named(text):
            for w in d.screen().root.query_tree().children:
                try:
                    if text in str(w.get_wm_name() or ""):
                        return w
                except error.BadWindow:
                    # A modal may be destroyed between QueryTree and GetProperty.
                    continue
            return None

        window = await wait(lambda: named("Dome Sculpt"),90)
        window.set_input_focus(X.RevertToParent, X.CurrentTime)
        d.sync()

        def prop(name):
            p = window.get_full_property(d.intern_atom(name), X.AnyPropertyType)
            return bytes(p.value).decode() if p else None

        service = await wait(lambda: prop("_KDE_NET_WM_APPMENU_SERVICE_NAME"))
        path = await wait(lambda: prop("_KDE_NET_WM_APPMENU_OBJECT_PATH"))
        proxy = bus.get_proxy_object(service, path, await bus.introspect(service, path))
        menu = proxy.get_interface("com.canonical.dbusmenu")

        async def layout():
            result = await menu.call_get_layout(0, -1, [])
            nodes = {}

            def walk(node):
                ident, properties, children = node
                properties = {k: v.value for k, v in properties.items()}
                label = properties.get("label", "").replace("_", "")
                nodes[label] = (ident, properties)
                for child in children:
                    walk(child.value)

            walk(result[1])
            return nodes, result[1]

        async def click(label):
            nodes, _ = await layout()
            saved = fixture.stat().st_mtime_ns if label == "Save" and (rig_flow or bake_flow or export_flow or retopo_flow) else None
            await menu.call_event(nodes[label][0], "clicked", Variant("i", 0), 0)
            if saved is not None:
                await wait(lambda: fixture.stat().st_mtime_ns != saved)
            await asyncio.sleep(0.4)

        def key(name, down=None):
            for kind in (
                [X.KeyPress, X.KeyRelease]
                if down is None
                else [X.KeyPress if down else X.KeyRelease]
            ):
                xtest.fake_input(
                    d, kind, d.keysym_to_keycode(XK.string_to_keysym(name))
                )
                d.sync()

        await asyncio.sleep(0.6)
        if retopo_flow:
            def info():
                return json.loads(subprocess.check_output([str(root / "build/dome-sculpt"), "--project-info", str(fixture)], env=env))
            baseline = info()
            nodes, _ = await layout()
            check(nodes["Start patch"][1].get("enabled") is True and nodes["Apply patch"][1].get("enabled") is False,
                  "Native universal menu exposes source-aware retopology actions")
            await click("Start patch")
            async def ready(label):
                for _ in range(300):
                    nodes, _ = await layout()
                    if nodes[label][1].get("enabled") is True:
                        return nodes
                    await asyncio.sleep(.1)
                raise AssertionError("Retopology menu did not enable " + label)
            await ready("Place points")
            nodes, _ = await layout()
            check(nodes["Place points"][1].get("toggle-state") == 1 and nodes["Move points"][1].get("toggle-state") == 0
                  and nodes["Save"][1].get("enabled") is False and nodes["Voxel remesh..."][1].get("enabled") is False,
                  "Retopology radio state and source-changing command isolation reach DomeD")
            await click("Projection distance...")
            w = await wait(lambda: named("Retopology projection distance"))
            check(w.get_wm_transient_for().id == window.id,
                  "Projection distance uses the native document-owned modal")
            if retopo_compact:
                g=w.get_geometry()
                raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF)
                Image.frombytes("RGB",(g.width,g.height),raw.data,"raw","BGRX").save(out/"projection-large-bdf.png")
                check(g.width<=600 and g.height<=320,"Projection slider and fixed actions fit 640x360 at 2x Unifont BDF")
            await click("Close Window")
            await wait(lambda: named("Retopology projection distance") is None)
            nodes,_=await layout()
            check(nodes["Cancel patch"][1].get("enabled") is True and info()["scene_hash"]==baseline["scene_hash"],
                  "Universal Close cancels projection settings while preserving the patch workspace and source")
            if retopo_compact:
                await click("Cancel patch")
                check(info()["scene_hash"]==baseline["scene_hash"] and app.poll() is None,
                      "Compact cancellation preserves the complete native scene")
                d.close();bus.disconnect();return
            window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
            g=window.get_geometry()
            def pixels():
                raw=window.get_image(0,0,g.width,g.height-60,X.ZPixmap,0xFFFFFFFF)
                return Image.frombytes("RGB",(g.width,g.height-60),raw.data,"raw","BGRX")
            def press(x,y):
                xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));d.sync()
                key("Shift_L",True);xtest.fake_input(d,X.ButtonPress,1);d.sync()
            def release():
                xtest.fake_input(d,X.ButtonRelease,1);d.sync();key("Shift_L",False)
            before=pixels()
            press(g.width*.5,g.height*.5)
            await wait(lambda:pixels().tobytes()!=before.tobytes())
            pixels().save(out/"held-point.png")
            check(info()["scene_hash"]==baseline["scene_hash"],"Held native placement shows real point feedback without editing the scene")
            key("Escape");release()
            await wait(lambda:pixels().tobytes()==before.tobytes())
            check(True,"Escape restores the exact native viewport before the held point")
            for dx,dy in ((-70,70),(70,70),(70,-70),(-70,-70)):
                press(g.width*.5+dx,g.height*.5+dy);release();await asyncio.sleep(.25)
            await ready("Make face");pixels().save(out/"ordered-corners.png")
            await click("Make face");await ready("Apply patch")
            check(info()["scene_hash"]==baseline["scene_hash"],"Ordered face remains an unconfirmed draft until Apply")
            await click("Move points")
            nodes,_=await layout()
            check(nodes["Move points"][1].get("toggle-state")==1 and nodes["Place points"][1].get("toggle-state")==0,
                  "Actual retopology tool switching updates exported radio feedback")
            await click("Apply patch");await ready("Save");await click("Save")
            accepted=info()
            (out / "accepted-scene.json").write_text(json.dumps({"baseline": baseline, "accepted": accepted}, indent=2))
            check(len(accepted["objects"])==2 and accepted["retopology_vertices"]==4 and accepted["retopology_faces"]==1
                  and accepted["objects"][0]["payload_hash"]==baseline["objects"][0]["payload_hash"]
                  and accepted["objects"][0]["visible"] is False,
                  "Native Apply and Save persist an editable quad and atlas mesh while retaining the exact hidden source")
            await click("Undo");await click("Save")
            check(info()["scene_hash"]==baseline["scene_hash"],"Universal Undo restores the complete scene in one transaction")
            await click("Redo");await click("Save")
            check(info()["scene_hash"]==accepted["scene_hash"],"Universal Redo restores the complete editable retopology scene")
            await click("Start patch");await ready("Place points");await click("Cancel patch");await click("Save")
            check(info()["scene_hash"]==accepted["scene_hash"] and app.poll() is None,
                  "Reopen and cancellation retain saved topology and leave the native application responsive")
            d.close();bus.disconnect();return
        if export_flow:
            import hashlib, struct
            original_hash = hashlib.sha256(fixture.read_bytes()).digest()
            nodes, _ = await layout()
            check(nodes["Export selected avatar (.vrm)..."][1].get("enabled") is True,
                  "Source avatar exports an enabled native universal-menu command")
            await click("Export selected avatar (.vrm)...")
            w = await wait(lambda: named("Export selected avatar"))
            check(w.get_wm_transient_for() is not None and w.get_wm_class()[1] == window.get_wm_class()[1],
                  "Avatar export chooser inherits the document's native application identity")
            await click("Close Window")
            await wait(lambda: named("Export selected avatar") is None)
            check(not Path(home, "Avatar.vrm").exists() and hashlib.sha256(fixture.read_bytes()).digest() == original_hash,
                  "Universal Close cancels avatar export without output or project mutation")
            await click("Export selected avatar (.vrm)...")
            w = await wait(lambda: named("Export selected avatar"))
            w.set_input_focus(X.RevertToParent, X.CurrentTime); d.sync(); key("Return")
            await wait(lambda: named("Export selected avatar") is None)
            target = Path(home, "Avatar.vrm")
            await wait(lambda: target.is_file())
            payload = target.read_bytes()
            magic, version, length = struct.unpack_from("<III", payload)
            size, kind = struct.unpack_from("<II", payload, 12)
            model = json.loads(payload[20:20+size])
            check(magic == 0x46546c67 and version == 2 and length == len(payload) and kind == 0x4e4f534a and
                  len(model["meshes"]) == 19 and len(model["skins"]) == 1 and "VRM" in model["extensions"],
                  "Native file chooser submits a real grouped VRM container through the background worker")
            check(hashlib.sha256(fixture.read_bytes()).digest() == original_hash and app.poll() is None,
                  "Completed avatar export leaves the editable native project and desktop responsive")
            d.close(); bus.disconnect(); return
        if bake_flow:
            def info():
                return json.loads(subprocess.check_output([str(root / "build/dome-sculpt"),"--project-info",str(fixture)],env=env))
            def tap(w,x,y):
                g=w.get_geometry();xtest.fake_input(d,X.MotionNotify,x=g.x+int(x),y=g.y+int(y));xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
            baseline=info();nodes,_=await layout()
            check(nodes["Bake surface..."][1].get("enabled") is True and nodes["Clear baked surface"][1].get("enabled") is False,
                  "The universal menu enables valid baking and disables absent baked data")
            if bake_compact:
                await click("Bake surface...");w=await wait(lambda:named("Bake surface"));g=w.get_geometry()
                w.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();await asyncio.sleep(.2)
                def shot():
                    raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF)
                    return Image.frombytes("RGB",(g.width,g.height),raw.data,"raw","BGRX")
                before=shot();before.save(out / "bake-compact-before.png")
                check(g.width<=640 and g.height<=360,"A 2x BDF baking dialog fits the compact screen")
                xtest.fake_input(d,X.MotionNotify,x=g.x+g.width-8,y=g.y+80)
                for _ in range(5):xtest.fake_input(d,X.ButtonPress,5);xtest.fake_input(d,X.ButtonRelease,5)
                d.sync();await asyncio.sleep(.3);after=shot();after.save(out / "bake-compact-after.png")
                footer=max(72,48*2)
                check(before.crop((0,0,g.width,g.height-footer)).tobytes()!=after.crop((0,0,g.width,g.height-footer)).tobytes() and before.crop((0,g.height-footer,g.width,g.height)).tobytes()==after.crop((0,g.height-footer,g.width,g.height)).tobytes(),
                      "Large bitmap content scrolls while both baking action buttons remain fixed")
                key("Escape");await wait(lambda:named("Bake surface") is None)
                await click("Save");check(info()["scene_hash"]==baseline["scene_hash"],"Compact-screen cancellation preserves the complete source scene")
                d.close();bus.disconnect();return
            if not bake_cancel:
                for action in ("Return","Escape","Control-w","Universal Close"):
                    await click("Bake surface...");w=await wait(lambda:named("Bake surface"))
                    w.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
                    nodes,_=await layout()
                    check(w.get_wm_transient_for().id==window.id and nodes["Bake surface..."][1].get("enabled") is False,
                          "Baking options retain native modal ownership and isolate document commands: "+action)
                    if action=="Universal Close":await click("Close Window")
                    elif action=="Control-w":key("Control_L",True);key("w");key("Control_L",False)
                    else:key(action)
                    await wait(lambda:named("Bake surface") is None)
                    await click("Save")
                    check(info()["scene_hash"]==baseline["scene_hash"],"Baking options cancel without scene mutation: "+action)
            await click("Bake surface...");w=await wait(lambda:named("Bake surface"));g=w.get_geometry()
            w.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
            def pixels():
                raw=w.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF)
                return Image.frombytes("RGB",(g.width,g.height),raw.data,"raw","BGRX")
            if bake_cancel:
                # Last end of the AO-samples track is the exact integer maximum.
                tap(w,g.width-42,228)
            else:
                before=pixels();xtest.fake_input(d,X.MotionNotify,x=g.x+g.width-70,y=g.y+124)
                xtest.fake_input(d,X.ButtonPress,4);xtest.fake_input(d,X.ButtonRelease,4);d.sync();await asyncio.sleep(.15)
                check(pixels().crop((230,108,g.width-30,140)).tobytes()!=before.crop((230,108,g.width-30,140)).tobytes(),
                      "A wheel over the native logarithmic distance slider updates its visible value")
            pixels().save(out / "bake-options.png")
            tap(w,g.width*.75,g.height-46)
            if bake_cancel:
                def projecting():
                    title=str(w.get_wm_name() or "")
                    if "Baking " not in title:return False
                    try:return int(title.split("Baking ")[1].split("%")[0])>=10
                    except ValueError:return False
                await wait(projecting)
                check(True,"The native bake dialog reports live progress while projection/AO work is executing")
                key("Control_L",True);key("w");key("Control_L",False)
                await wait(lambda:named("Bake surface") is None)
            else:
                await wait(lambda:"complete" in str(w.get_wm_name() or ""))
                check(True,"Native Bake completes asynchronously and reports completion in its visible title")
                pixels().save(out / "bake-complete.png")
                await click("Close Window");await wait(lambda:named("Bake surface") is None)
            for _ in range(200):
                nodes,_=await layout()
                if nodes["Save"][1].get("enabled") is True:break
                await asyncio.sleep(.03)
            await click("Save");baked=info()
            if bake_cancel:
                check(baked["scene_hash"]==baseline["scene_hash"] and nodes["Undo"][1].get("enabled") is False,
                      "Ctrl+W cancels a running native bake without publishing partial maps or history")
                d.close();bus.disconnect();return
            check(baked["objects"][0]["baked"] and baked["paint_blue_texels"]>1000 and baked["objects"][1:]==baseline["objects"][1:],
                  "Native baking publishes visible source paint and retains both complete hidden source objects")
            await click("Undo");await click("Save")
            check(info()["scene_hash"]==baseline["scene_hash"],"Universal Undo restores the complete scene before native baking")
            await click("Redo");await click("Save")
            check(info()["scene_hash"]==baked["scene_hash"],"Universal Redo restores the exact baked surface")
            nodes,_=await layout();check(nodes["Clear baked surface"][1].get("enabled") is True,"The baked target exports an enabled explicit Clear command")
            await click("Clear baked surface");await asyncio.sleep(.2);await click("Save")
            check(info()["scene_hash"]==baseline["scene_hash"],"Native Clear retains editable paint layers and original source data")
            await click("Undo");await click("Save")
            check(info()["scene_hash"]==baked["scene_hash"],"Universal Undo restores the exact cleared baked surface")
            await click("Bake surface...");w=await wait(lambda:named("Bake surface"));g=w.get_geometry()
            tap(w,g.width/2+12,124);tap(w,g.width*.75,g.height-46)
            await wait(lambda:"failed" in str(w.get_wm_name() or ""))
            pixels().save(out / "bake-projection-error.png")
            check(True,"A too-short projection cage shows visible worker failure in the native dialog")
            await click("Close Window");await wait(lambda:named("Bake surface") is None);await click("Save")
            check(info()["scene_hash"]==baked["scene_hash"],"Failed native rebaking preserves the complete previous baked surface and sources")
            d.close();bus.disconnect();return

        if rig_flow:
            def info():
                return json.loads(subprocess.check_output([str(root / "build/dome-sculpt"), "--project-info", str(fixture)], env=env))
            def tap(w, x, y):
                g = w.get_geometry()
                xtest.fake_input(d, X.MotionNotify, x=g.x+int(x), y=g.y+int(y))
                xtest.fake_input(d, X.ButtonPress, 1); xtest.fake_input(d, X.ButtonRelease, 1); d.sync()
            def enter_rotation(w):
                w.set_input_focus(X.RevertToParent, X.CurrentTime); d.sync()
                g = w.get_geometry()
                tap(w, g.width/2+60, 112)
                key("Control_L", True); key("a"); key("Control_L", False)
                key("3"); key("0"); key("Tab")
            baseline = info()
            if rig_compact:
                await click("Skin weights..."); w = await wait(lambda: named("Skin weights"))
                w.set_input_focus(X.RevertToParent, X.CurrentTime); d.sync(); await asyncio.sleep(.2)
                g = w.get_geometry()
                def shot():
                    raw = w.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
                    return Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX")
                before = shot(); before.save(out / "weight-compact-before.png")
                check(g.width <= 640 and g.height <= 360, "A 2x BDF skin-weight dialog fits the compact screen")
                xtest.fake_input(d, X.MotionNotify, x=g.x+g.width-8, y=g.y+100)
                for _ in range(8): xtest.fake_input(d, X.ButtonPress, 5); xtest.fake_input(d, X.ButtonRelease, 5)
                d.sync(); await asyncio.sleep(.3); after = shot(); after.save(out / "weight-compact-after.png")
                check(before.crop((12,8,g.width-40,g.height-96)).tobytes() != after.crop((12,8,g.width-40,g.height-96)).tobytes(),
                      "The large-bitmap weight options scroll to their lower controls")
                check(before.crop((0,g.height-88,g.width,g.height)).tobytes() == after.crop((0,g.height-88,g.width,g.height)).tobytes(),
                      "Both native skin-weight action buttons remain fixed while options scroll")
                key("Escape"); await wait(lambda: named("Skin weights") is None)
                check(app.poll() is None and info()["scene_hash"] == baseline["scene_hash"],
                      "Compact weight-dialog cancellation preserves the native project and main window")
                d.close(); bus.disconnect(); return
            nodes, _ = await layout()
            check(nodes["Rig and shapekeys..."][1].get("enabled") is True,
                  "Rigged mesh exports enabled native rig authoring command")
            await click("Rig and shapekeys...")
            w = await wait(lambda: named("Rig and shapekeys"))
            nodes, _ = await layout()
            check(w.get_wm_transient_for().id == window.id and
                  nodes["Rig and shapekeys..."][1].get("enabled") is False,
                  "Rig editor inherits document identity and isolates scene commands")
            enter_rotation(w)
            g = w.get_geometry()
            raw = w.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
            Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX").save(out / "rig-options.png")
            await click("Close Window")
            await wait(lambda: named("Rig and shapekeys") is None)
            await click("Save")
            check(info()["scene_hash"] == baseline["scene_hash"],
                  "Universal Close cancels staged rig values without modifying native project")
            await click("Rig and shapekeys...")
            w = await wait(lambda: named("Rig and shapekeys"))
            enter_rotation(w)
            g = w.get_geometry(); tap(w, g.width/2+80, g.height-40)
            await wait(lambda: named("Rig and shapekeys") is None)
            for _ in range(200):
                nodes, _ = await layout()
                if nodes["Save"][1].get("enabled") is True: break
                await asyncio.sleep(.03)
            await click("Save")
            check(info()["scene_hash"] != baseline["scene_hash"],
                  "Native Apply commits the entered joint adjustment on the asset worker")
            await click("Undo"); await click("Save")
            check(info()["scene_hash"] == baseline["scene_hash"],
                  "Universal Undo restores the exact complete rigged scene")
            await click("Redo"); await click("Save")
            check(info()["scene_hash"] != baseline["scene_hash"],
                  "Universal Redo reapplies the complete grouped pose transaction")
            posed = info()["scene_hash"]
            await click("Rig and shapekeys...")
            w = await wait(lambda: named("Rig and shapekeys"))
            w.set_input_focus(X.RevertToParent, X.CurrentTime); d.sync()
            g = w.get_geometry(); tap(w, g.width/2+60, 400)
            key("Control_L", True); key("a"); key("Control_L", False)
            key("0"); key("period"); key("5"); key("Tab")
            tap(w, g.width/2+80, g.height-40)
            await wait(lambda: named("Rig and shapekeys") is None)
            for _ in range(200):
                nodes, _ = await layout()
                if nodes["Save"][1].get("enabled") is True: break
                await asyncio.sleep(.03)
            await click("Save")
            check(info()["scene_hash"] != posed,
                  "Editing native shapekey weight enables and commits that selected target")
            await click("Undo"); await click("Save")
            check(info()["scene_hash"] == posed,
                  "Shapekey Undo preserves the previously accepted grouped pose exactly")
            await click("Skin weights...")
            w = await wait(lambda: named("Skin weights"))
            nodes, _ = await layout()
            check(w.get_wm_transient_for().id == window.id and nodes["Skin weights..."][1].get("enabled") is False,
                  "Skin-weight controls share native identity and modal scene isolation")
            w.set_input_focus(X.RevertToParent, X.CurrentTime); d.sync()
            g = w.get_geometry()
            visible_before = {p.id for p in d.screen().root.query_tree().children
                              if p.get_attributes().map_state == X.IsViewable}
            xtest.fake_input(d, X.MotionNotify, x=g.x+int(g.width/2+70), y=g.y+80)
            xtest.fake_input(d, X.ButtonPress, 1); d.sync()
            def joint_popup():
                for p in d.screen().root.query_tree().children:
                    try:
                        geometry = p.get_geometry(); attributes = p.get_attributes()
                        if p.id not in visible_before and attributes.map_state == X.IsViewable and attributes.override_redirect and geometry.width > 100 and geometry.height < 200:
                            return p
                    except error.BadWindow:
                        pass
                return None
            popup = await wait(joint_popup); option = popup.get_geometry()
            xtest.fake_input(d, X.MotionNotify, x=option.x+int(option.width/2), y=option.y+int(option.height*.75))
            xtest.fake_input(d, X.ButtonRelease, 1); d.sync(); await asyncio.sleep(.2)
            raw = w.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
            Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX").save(out / "weight-options.png")
            tap(w, g.width/2+80, g.height-40)
            await wait(lambda: named("Skin weights") is None)
            await asyncio.sleep(.4); await click("Save")
            check(info()["scene_hash"] == posed, "Weight-view configuration changes no model data or history")
            for _ in range(14): await click("Larger brush")
            window.set_input_focus(X.RevertToParent, X.CurrentTime); d.sync()
            g = window.get_geometry()
            xtest.fake_input(d, X.MotionNotify, x=g.x+int(g.width*.60), y=g.y+int(g.height*.35)); d.sync()
            def weight_pixels():
                raw = window.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
                return Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX")
            canvas_box = (20, 20, g.width-20, g.height-50)
            before_preview = weight_pixels().crop(canvas_box).tobytes()
            key("Shift_L", True)
            xtest.fake_input(d, X.ButtonPress, 1); d.sync()
            await wait(lambda: weight_pixels().crop(canvas_box).tobytes() != before_preview)
            weight_pixels().save(out / "weight-live-preview.png")
            check(info()["scene_hash"] == posed,
                  "A held native pressure gesture displays live heatmap feedback without changing the saved scene")
            key("Escape")
            xtest.fake_input(d, X.ButtonRelease, 1); d.sync(); key("Shift_L", False)
            await wait(lambda: weight_pixels().crop(canvas_box).tobytes() == before_preview)
            check(info()["scene_hash"] == posed,
                  "Escape restores the exact native viewport after cancelling a live weight preview")
            key("Shift_L", True)
            xtest.fake_input(d, X.ButtonPress, 1); d.sync(); await asyncio.sleep(.15)
            xtest.fake_input(d, X.ButtonRelease, 1); d.sync(); key("Shift_L", False)
            for _ in range(200):
                nodes, _ = await layout()
                if nodes["Save"][1].get("enabled") is True: break
                await asyncio.sleep(.03)
            await asyncio.sleep(.2); await click("Save")
            raw = window.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
            Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX").save(out / "weight-gesture.png")
            check(info()["scene_hash"] != posed, "Actual Shift-drag in the native viewport paints persistent skin weights")
            await click("Undo"); await click("Save")
            check(info()["scene_hash"] == posed, "Universal Undo restores exactly one complete native weight gesture")
            d.close(); bus.disconnect(); return
        if compact:
            await click("Voxel remesh...")
            w = await wait(lambda: named("Voxel remesh"))
            w.set_input_focus(X.RevertToParent, X.CurrentTime); d.sync()
            await asyncio.sleep(.2)
            g = w.get_geometry()
            check(g.width <= 600 and g.height <= 320,
                  "A doubled BDF font keeps voxel options within a 640x360 work area")
            def shot():
                raw = w.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
                return Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX")
            before = shot(); before.save(out / "options-large-bdf.png")
            for _ in range(5):
                xtest.fake_input(d, X.MotionNotify, x=g.x+g.width-8, y=g.y+80)
                xtest.fake_input(d, X.ButtonPress, 5); xtest.fake_input(d, X.ButtonRelease, 5)
            d.sync(); await asyncio.sleep(.2)
            after = shot(); after.save(out / "options-scrolled.png")
            check(before.crop((0,0,g.width,g.height-46)).tobytes() !=
                  after.crop((0,0,g.width,g.height-46)).tobytes() and
                  before.crop((0,g.height-46,g.width,g.height)).tobytes() ==
                  after.crop((0,g.height-46,g.width,g.height)).tobytes(),
                  "Large-font voxel content scrolls while both action buttons stay fixed")
            key("Escape"); await wait(lambda: named("Voxel remesh") is None)
            nodes, _ = await layout()
            check(app.poll() is None and nodes["Undo"][1].get("enabled") is False,
                  "Compact-screen cancellation preserves the original sculpt")
            d.close(); bus.disconnect()
            return
        nodes, tree = await layout()
        (out / "layout.json").write_text(json.dumps(nodes, indent=2))
        check(
            all(
                h in nodes for h in ("File", "Edit", "View", "Sculpt", "Scene", "Paint")
            ),
            "DomeD receives the actual native sculpt and paint menu commands",
        )
        check(
            nodes["Close Window"][1].get("shortcut") == [["Control", "W"]]
            and "Quit" not in nodes,
            "File uses Ctrl+W Close Window and leaves application Quit to DomeD",
        )
        check(
            nodes["PBR"][1].get("toggle-type") == "radio"
            and nodes["PBR"][1].get("toggle-state") == 1,
            "The current shading mode has an exported radio marker",
        )
        check(
            nodes["Delete object"][1].get("enabled") is False
            and nodes["Undo"][1].get("enabled") is False,
            "Unavailable object and undo commands are exported as disabled",
        )
        check("Refresh recovery" in nodes and
              nodes["Recover project..."][1].get("enabled") is False,
              "Recovery commands use the universal menu and disable empty recovery")
        await click("Refresh recovery")
        await asyncio.sleep(0.3)
        nodes, tree = await layout()
        check(nodes["Refresh recovery"][1].get("enabled") is True,
              "Background recovery discovery returns without blocking the native menu")
        subprocess.run([str(root / "build/dome-sculpt-document-recovery-test"),
                        "--ui-fixtures", str(Path(home, "state/dome-sculpt/recovery"))], check=True)
        await click("Refresh recovery")
        for _ in range(100):
            nodes, tree = await layout()
            if nodes["Recover project..."][1].get("enabled") is True: break
            await asyncio.sleep(0.03)
        check(nodes["Recover project..."][1].get("enabled") is True,
              "Native recovery discovery exposes persisted projects")
        await click("Recover project...")
        recovery_editor = await wait(lambda: named("Recover project"))
        check(recovery_editor.get_wm_transient_for().id == window.id,
              "Recovery chooser uses the document's native modal ownership")
        await click("Close Window")
        await asyncio.sleep(0.2)
        nodes, tree = await layout()
        check(named("Recover project") is None and
              nodes["Undo"][1].get("enabled") is False,
              "Universal Close cancels recovery without changing the document")
        await click("Import mesh...")
        import_chooser = await wait(lambda: named("Import static mesh"))
        check(import_chooser.get_wm_transient_for() is not None and
              import_chooser.get_wm_class()[1] == window.get_wm_class()[1],
              "Mesh import uses the application's native file chooser")
        await click("Close Window")
        await asyncio.sleep(0.2)
        nodes, tree = await layout()
        check(named("Import static mesh") is None and nodes["Undo"][1].get("enabled") is False,
              "Universal Close cancels mesh import without changing scene history")
        await click("Create humanoid...")
        creator = await wait(lambda: named("Create humanoid"))
        check(creator.get_wm_transient_for().id == window.id,
              "Humanoid controls use the document's native modal ownership")
        nodes, tree = await layout()
        check(nodes["Create humanoid..."][1].get("enabled") is False,
              "Humanoid parameter editing isolates scene commands")
        await click("Close Window")
        await asyncio.sleep(0.2)
        nodes, tree = await layout()
        check(named("Create humanoid") is None and nodes["Undo"][1].get("enabled") is False,
              "Universal Close cancels humanoid creation without geometry or history changes")
        raw = window.get_image(2, 2, 1, 1, X.ZPixmap, 0xFFFFFFFF)
        pixel = Image.frombytes("RGB", (1, 1), raw.data, "raw", "BGRX").getpixel((0, 0))
        check(
            pixel == (48, 48, 48),
            "The local bar yields to the universal host and exposes the DomeD grey viewport",
        )
        check("Mesh" in nodes and nodes["Voxel remesh..."][1].get("enabled") is True,
              "Voxel remeshing exports an enabled command for a nonempty sculpt")

        def tap(w, x, y, button=1):
            g = w.get_geometry()
            xtest.fake_input(d, X.MotionNotify, x=g.x+int(x), y=g.y+int(y))
            d.sync()
            xtest.fake_input(d, X.ButtonPress, button)
            xtest.fake_input(d, X.ButtonRelease, button)
            d.sync()

        def pixels(w, box=None):
            g = w.get_geometry()
            raw = w.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
            image = Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX")
            return image.crop(box) if box else image

        async def voxel_dialog():
            await click("Voxel remesh...")
            w = await wait(lambda: named("Voxel remesh"))
            w.set_input_focus(X.RevertToParent, X.CurrentTime); d.sync()
            await asyncio.sleep(.2)
            return w

        for close in ("Return", "Escape", "Ctrl+W", "Universal Close"):
            w = await voxel_dialog()
            if close == "Return":
                nodes, _ = await layout()
                check(w.get_wm_transient_for().id == window.id and
                      w.get_wm_class()[1] == window.get_wm_class()[1] and
                      all(nodes[n][1].get("enabled") is False for n in
                          ("Voxel remesh...", "New", "Undo", "Brush settings...")),
                      "Voxel options inherit native application identity and isolate universal commands")
            if close == "Ctrl+W":
                key("Control_L", True); key("w"); key("Control_L", False)
            elif close == "Universal Close":
                await click("Close Window")
            else:
                key(close)
            await wait(lambda: named("Voxel remesh") is None)
            nodes, _ = await layout()
            check(app.poll() is None and nodes["Undo"][1].get("enabled") is False,
                  close + " cancels voxel options without changing scene history")

        # A real slider and scrollwheel change the millimetre readout. Confirm
        # only the subsequent explicit button press; merely editing is a draft.
        w = await voxel_dialog(); g = w.get_geometry(); row = g.height // 8
        box = (12, row*2, g.width-12, row*3)
        before = pixels(w, box)
        tap(w, g.width*.84, row*3.5)
        await asyncio.sleep(.2)
        changed = pixels(w, box)
        check(before.tobytes() != changed.tobytes(),
              "The native remesh slider updates its visible millimetre readout")
        tap(w, g.width*.84, row*3.5, 4)
        await asyncio.sleep(.2)
        check(changed.tobytes() != pixels(w, box).tobytes(),
              "A scrollwheel over the native slider adjusts size with visible feedback")
        # Minimum spacing exceeds the bounded grid budget for this sphere.
        tap(w, 30, row*3.5)
        await asyncio.sleep(.2)
        pixels(w).save(out / "voxel-options.png")
        gmain = window.get_geometry()
        status_box = (0, gmain.height-28, gmain.width, gmain.height)
        status_before = pixels(window, status_box)
        tap(w, g.width*.75, g.height-row*.5)
        await wait(lambda: named("Voxel remesh") is None)
        for _ in range(100):
            nodes, _ = await layout()
            if nodes["Voxel remesh..."][1].get("enabled") is True: break
            await asyncio.sleep(.05)
        check(nodes["Undo"][1].get("enabled") is False and
              status_before.tobytes() != pixels(window, status_box).tobytes(),
              "A rejected voxel grid displays feedback and leaves scene history unchanged")
        pixels(window).save(out / "voxel-budget-feedback.png")

        recovery_root = Path(home, "state/dome-sculpt/recovery")
        before_files = set(recovery_root.glob("*.dsculpt"))
        w = await voxel_dialog(); g = w.get_geometry(); row = g.height // 8
        # Reopen after the rejected minimum: click a practical coarser spacing.
        tap(w, g.width*.73, row*3.5)
        await asyncio.sleep(.2)
        tap(w, g.width*.75, g.height-row*.5)
        await wait(lambda: named("Voxel remesh") is None)
        files = await wait(lambda: set(recovery_root.glob("*.dsculpt"))-before_files)
        info = json.loads(subprocess.check_output(
            [str(root / "build/dome-sculpt"), "--project-info", str(next(iter(files)))],
            env=env, text=True))
        nodes, _ = await layout()
        check(info["object_count"] == 2 and not info["painted"] and
              info["objects"][0]["visible"] is False and info["samples"] > 0 and
              nodes["Undo"][1].get("enabled") is True,
              "Explicit native Remesh creates a recovered sculpt and retains a hidden source")
        await click("Undo")
        nodes, _ = await layout()
        check(nodes["Undo"][1].get("enabled") is False and
              nodes["Delete object"][1].get("enabled") is False and
              nodes["Redo"][1].get("enabled") is True,
              "Universal Undo restores the original single-object scene after voxel remeshing")
        check(nodes["Add sculpt layer"][1].get("enabled") is True and nodes["Delete sculpt layer"][1].get("enabled") is False,"Sculpt-layer commands expose context-aware native menu state")
        await click("Add sculpt layer")
        nodes,_=await layout()
        check(nodes["Delete sculpt layer"][1].get("enabled") is True and nodes["Next sculpt layer"][1].get("enabled") is True,"Universal sculpt-layer creation reaches the persistent document")
        await click("Sculpt layer properties...")
        layer_editor=await wait(lambda:named("Sculpt layer properties"))
        layer_editor.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        nodes,_=await layout()
        check(nodes["Add sculpt layer"][1].get("enabled") is False and layer_editor.get_wm_transient_for().id==window.id,"Sculpt layer properties obey native modal ownership and menu isolation")
        g=layer_editor.get_geometry()
        xtest.fake_input(d,X.MotionNotify,x=g.x+44,y=g.y+g.height//2);d.sync()
        xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
        xtest.fake_input(d,X.MotionNotify,x=g.x+g.width//2+75,y=g.y+int(g.height*5.5/7));d.sync()
        xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
        await asyncio.sleep(.4)
        nodes,_=await layout()
        check(named("Sculpt layer properties") is None and nodes["Sculpt layer visible"][1].get("toggle-state")==0,"Applying native layer properties changes the selected layer visibility")
        await click("Undo")
        nodes,_=await layout()
        check(nodes["Sculpt layer visible"][1].get("toggle-state")==1,"Universal Undo restores the complete sculpt-layer property transaction")
        await click("Sculpt layer properties...")
        await wait(lambda:named("Sculpt layer properties"))
        await click("Close Window")
        check(named("Sculpt layer properties") is None and app.poll() is None,"Universal Close cancels sculpt-layer properties without closing the document")
        await click("Undo")
        nodes,_=await layout()
        check(nodes["Delete sculpt layer"][1].get("enabled") is False,"Global Undo removes a newly created empty sculpt layer")
        await click("Toon preview")
        nodes, _ = await layout()
        check(
            nodes["Toon preview"][1].get("toggle-state") == 1
            and nodes["PBR"][1].get("toggle-state") == 0,
            "Universal-menu activation updates the live radio state",
        )
        await click("Duplicate object")
        nodes, _ = await layout()
        check(
            nodes["Delete object"][1].get("enabled") is True,
            "Object commands reach the editor and refresh their enabled state",
        )
        await click("Object and material...")
        editor = await wait(lambda: named("Object and material"))
        editor.set_input_focus(X.RevertToParent, X.CurrentTime)
        d.sync()
        await asyncio.sleep(0.2)
        check(editor.get_wm_transient_for() is not None and editor.get_wm_transient_for().id==window.id and editor.get_wm_class()[1]==window.get_wm_class()[1],"Parameter dialogs retain the owning application's identity and parent menu")
        nodes,_=await layout()
        check(all(nodes[n][1].get("enabled") is False for n in ["New","Reset trackball","PBR","Brush settings...","Red paint","Smaller brush"]),"Document, view and paint actions are disabled behind the modal parameter editor")
        g=editor.get_geometry()
        xtest.fake_input(d,X.MotionNotify,x=g.x+g.width*2//3,y=g.y+40);d.sync()
        xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
        await click("Select All")
        nodes,_=await layout()
        check(nodes["Cut"][1].get("enabled") is True and nodes["Copy"][1].get("enabled") is True,"Universal Edit actions follow the selected text in the focused native field")
        await click("Copy");await click("Cut")
        nodes,_=await layout()
        check(nodes["Select All"][1].get("enabled") is False and nodes["Undo"][1].get("enabled") is True,"Universal Cut edits the field and records text undo without changing scene history")
        await click("Paste")
        nodes,_=await layout()
        check(nodes["Select All"][1].get("enabled") is True,"Universal Paste restores the copied field contents")
        key("Control_L", True)
        key("w")
        key("Control_L", False)
        await asyncio.sleep(0.4)
        check(
            named("Object and material") is None and app.poll() is None,
            "Ctrl+W cancels the native parameter dialog without closing its document",
        )
        await click("Close Window")
        confirm = await wait(lambda: named("Unsaved changes"))
        confirm.set_input_focus(X.RevertToParent, X.CurrentTime)
        d.sync()
        await asyncio.sleep(0.2)
        key("Return")
        await asyncio.sleep(0.4)
        check(
            named("Unsaved changes") is None and app.poll() is None,
            "Discard confirmation defaults to Cancel when Enter is pressed",
        )
        await click("Close Window")
        confirm = await wait(lambda: named("Unsaved changes"))
        confirm.set_input_focus(X.RevertToParent, X.CurrentTime)
        d.sync()
        key("Escape")
        await asyncio.sleep(0.4)
        check(
            named("Unsaved changes") is None and app.poll() is None,
            "Escape dismisses confirmation and retains unsaved work",
        )
        await click("Prepare painting mesh")
        deadline=time.monotonic()+60
        while time.monotonic()<deadline:
            nodes,_=await layout()
            if nodes["UV editor..."][1].get("enabled"):
                break
            await asyncio.sleep(.2)
        check(nodes["UV editor..."][1].get("enabled") is True,"The universal menu enables UV editing after the background mesh is ready")
        await click("UV editor...")
        uv_window=await wait(lambda:named("UV editor"))
        uv_window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        await asyncio.sleep(.3)
        nodes,_=await layout()
        check(nodes["Undo"][1].get("enabled") is False,"Opening a UV draft isolates Edit Undo from the document history")
        g=uv_window.get_geometry()
        def uv_pixels():
            raw=uv_window.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF)
            return Image.frombytes("RGB",(g.width,g.height),raw.data,"raw","BGRX").crop((20,80,g.width-20,g.height-80))
        before_uv=uv_pixels()
        xtest.fake_input(d,X.MotionNotify,x=g.x+g.width//12,y=g.y+16);d.sync()
        xtest.fake_input(d,X.ButtonPress,1);xtest.fake_input(d,X.ButtonRelease,1);d.sync()
        await asyncio.sleep(.5)
        nodes,_=await layout()
        check(nodes["Undo"][1].get("enabled") is True,"UV edits export their own available Undo command")
        await click("Undo")
        # Menu dispatch and GL redraw complete on separate event-loop turns.
        # Await the exact restored pixels before checking the exported history.
        await wait(lambda: uv_pixels().tobytes()==before_uv.tobytes())
        nodes,_=await layout()
        check(uv_pixels().tobytes()==before_uv.tobytes() and nodes["Redo"][1].get("enabled") is True,"Universal Edit Undo restores the UV draft and offers draft Redo")
        await click("Close Window")
        await asyncio.sleep(.3)
        check(named("UV editor") is None and app.poll() is None,"Universal Close Window cancels the UV draft and retains its document")
        await click("Save As...")
        chooser=await wait(lambda:named("Save sculpt project"))
        check(chooser.get_wm_transient_for() is not None and chooser.get_wm_class()[1]==window.get_wm_class()[1],"Native file choosers keep the same application identity and dialog relationship")
        await click("Close Window")
        await asyncio.sleep(.3)
        check(named("Save sculpt project") is None and not [p for p in Path(home).rglob("*.dsculpt") if Path(home, "state/dome-sculpt/recovery") not in p.parents],"Universal Close invokes the chooser's Cancel callback and never saves its default filename")
        await click("Save As...")
        chooser=await wait(lambda:named("Save sculpt project"))
        chooser.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        key("Control_L",True);key("w");key("Control_L",False)
        await asyncio.sleep(.3)
        check(named("Save sculpt project") is None and not [p for p in Path(home).rglob("*.dsculpt") if Path(home, "state/dome-sculpt/recovery") not in p.parents],"Ctrl+W cancels native Save As before its text field can consume the shortcut")
        await bus.release_name("com.canonical.AppMenu.Registrar")
        await asyncio.sleep(.7)
        raw=window.get_image(2,2,1,1,X.ZPixmap,0xFFFFFFFF)
        local_pixel=Image.frombytes("RGB",(1,1),raw.data,"raw","BGRX").getpixel((0,0))
        check(local_pixel!=(48,48,48),"Losing the universal host restores the local FLTK menu bar")
        await bus.request_name("com.canonical.AppMenu.Registrar")
        await asyncio.sleep(.7)
        raw=window.get_image(2,2,1,1,X.ZPixmap,0xFFFFFFFF)
        check(Image.frombytes("RGB",(1,1),raw.data,"raw","BGRX").getpixel((0,0))==(48,48,48),"The local bar yields again when the universal host returns")
        d.close()
    bus.disconnect()


try:
    asyncio.run(main())
finally:
    for p in reversed(processes):
        if p.poll() is None:
            p.terminate()
    for p in reversed(processes):
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
