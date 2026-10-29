#!/usr/bin/env python3
"""Exercise real FLTK DBusMenu export with a private host/bus/display."""

import asyncio, json, os, signal, subprocess, sys, tempfile, time
from pathlib import Path
from dbus_next.aio import MessageBus
from dbus_next import Variant
from Xlib import X, XK, display
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
out = root / "artifacts/menu"
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


async def wait(fn):
    until = time.monotonic() + 30
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
                "1280x900x24",
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
        log = (out / "app.log").open("w")
        app = subprocess.Popen(
            [str(root / "build/dome-sculpt")], env=env, cwd=home, stdout=log, stderr=log
        )
        processes.append(app)
        d = display.Display(screen)

        def named(text):
            return next(
                (
                    w
                    for w in d.screen().root.query_tree().children
                    if text in str(w.get_wm_name() or "")
                ),
                None,
            )

        window = await wait(lambda: named("Dome Sculpt"))
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
            await menu.call_event(nodes[label][0], "clicked", Variant("i", 0), 0)
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
        raw = window.get_image(2, 2, 1, 1, X.ZPixmap, 0xFFFFFFFF)
        pixel = Image.frombytes("RGB", (1, 1), raw.data, "raw", "BGRX").getpixel((0, 0))
        check(
            pixel == (48, 48, 48),
            "The local bar yields to the universal host and exposes the DomeD grey viewport",
        )
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
