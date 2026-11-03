#!/usr/bin/env python3
"""Real native FLTK/DBusMenu UV workflow; private Xvfb, no display restart."""
import asyncio
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
from dbus_next import Variant
from dbus_next.aio import MessageBus
from Xlib import X, XK, display, error
from Xlib.ext import xtest
from PIL import Image

signal.pthread_sigmask(signal.SIG_SETMASK, [])
if os.environ.get("DOME_UV_MENU_BUS") != "1":
    os.execvpe("dbus-run-session", ["dbus-run-session", "--", sys.executable, *sys.argv],
              {**os.environ, "DOME_UV_MENU_BUS": "1"})
ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / os.environ.get("DOME_UV_BUILD", "build")
HELPER = BUILD / os.environ.get("DOME_UV_HELPER", "dome-sculpt-uv-workflow-test")
OUT = ROOT / os.environ.get("DOME_UV_MENU_OUT", "artifacts/menu-uv-workflow")
OUT.mkdir(parents=True, exist_ok=True)
checks = []
processes = []


def check(value, message):
    checks.append({"passed": bool(value), "check": message})
    (OUT / "report.json").write_text(json.dumps({"checks": checks, "failed": sum(not c["passed"] for c in checks)}, indent=2) + "\n")
    print(("PASS " if value else "FAIL ") + message, flush=True)
    assert value, message


async def wait(operation, seconds=30):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        result = operation()
        if result:
            return result
        await asyncio.sleep(.04)
    raise TimeoutError("Native UV workflow did not reach expected state")


async def main():
    bus = await MessageBus().connect()
    await bus.request_name("com.canonical.AppMenu.Registrar")
    with tempfile.TemporaryDirectory(prefix="dome-uv-menu-") as task_home:
        read, write = os.pipe()
        log = (OUT / "xvfb.log").open("w")
        processes.append(subprocess.Popen(["Xvfb", "-displayfd", str(write), "-screen", "0", os.environ.get("DOME_UV23_SCREEN", "1280x1000x24"), "-nolisten", "tcp"], pass_fds=(write,), stdout=log, stderr=log))
        os.close(write)
        with os.fdopen(read) as stream:
            screen = ":" + stream.readline().strip()
        env = {**os.environ, "HOME": task_home, "DISPLAY": screen, "FLTK_BACKEND": "x11", "XDG_CURRENT_DESKTOP": "Dome", "LIBGL_ALWAYS_SOFTWARE": "1", "XMODIFIERS": "@im=none"}
        for key in ("WAYLAND_DISPLAY", "WAYLAND_SOCKET"):
            env.pop(key, None)
        for key, suffix in [("XDG_STATE_HOME", "state"), ("XDG_CONFIG_HOME", "config"), ("XDG_CACHE_HOME", "cache"), ("XDG_DATA_HOME", "data"), ("XDG_RUNTIME_DIR", "runtime")]:
            env[key] = task_home + "/" + suffix
            Path(env[key]).mkdir(mode=0o700)
        fixture = Path(task_home, "uv.dsculpt")
        subprocess.run([str(HELPER), "--ui-fixture", str(fixture)], check=True)
        log = (OUT / "app.log").open("w")
        app = subprocess.Popen([str(BUILD / "dome-sculpt"), str(fixture)], cwd=task_home, env=env, stdout=log, stderr=log)
        processes.append(app)
        connection = display.Display(screen)

        def named(title):
            for window in connection.screen().root.query_tree().children:
                try:
                    if title in str(window.get_wm_name() or ""):
                        return window
                except error.BadWindow:
                    pass
            return None

        window = await wait(lambda: named("Dome Sculpt"), seconds=90)
        window.set_input_focus(X.RevertToParent, X.CurrentTime)
        connection.sync()

        def property_value(name):
            value = window.get_full_property(connection.intern_atom(name), X.AnyPropertyType)
            return bytes(value.value).decode() if value else None

        service = await wait(lambda: property_value("_KDE_NET_WM_APPMENU_SERVICE_NAME"))
        path = await wait(lambda: property_value("_KDE_NET_WM_APPMENU_OBJECT_PATH"))
        proxy = bus.get_proxy_object(service, path, await bus.introspect(service, path))
        menu = proxy.get_interface("com.canonical.dbusmenu")

        async def layout():
            result = await menu.call_get_layout(0, -1, [])
            nodes = {}
            def walk(node):
                ident, properties, children = node
                properties = {key: value.value for key, value in properties.items()}
                nodes[properties.get("label", "").replace("_", "")] = (ident, properties)
                for child in children:
                    walk(child.value)
            walk(result[1])
            return nodes

        async def command(label):
            nodes = await layout()
            previous=fixture.stat().st_mtime_ns if label=="Save" else None
            await menu.call_event(nodes[label][0], "clicked", Variant("i", 0), 0)
            if previous is not None:
                # Atomic file replacement is the actual Save completion ack.
                await wait(lambda:fixture.stat().st_mtime_ns!=previous)
            await asyncio.sleep(.25)

        def tap(widget, x, y):
            geometry = widget.get_geometry()
            xtest.fake_input(connection, X.MotionNotify, x=geometry.x + int(x), y=geometry.y + int(y))
            xtest.fake_input(connection, X.ButtonPress, 1)
            xtest.fake_input(connection, X.ButtonRelease, 1)
            connection.sync()

        def key(name, down=None):
            kinds = [X.KeyPress, X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]
            for kind in kinds:
                xtest.fake_input(connection, kind, connection.keysym_to_keycode(XK.string_to_keysym(name)))
                connection.sync()

        def info():
            return json.loads(subprocess.check_output([str(BUILD / "dome-sculpt"), "--project-info", str(fixture)], env=env))

        def image(widget):
            geometry = widget.get_geometry()
            raw = widget.get_image(0, 0, geometry.width, geometry.height, X.ZPixmap, 0xFFFFFFFF)
            return Image.frombytes("RGB", (geometry.width, geometry.height), raw.data, "raw", "BGRX")

        async def enabled(label):
            for _ in range(400):
                nodes = await layout()
                if nodes[label][1].get("enabled", True):
                    return
                await asyncio.sleep(.02)
            raise TimeoutError("Native command remained busy: " + label)

        baseline = info()
        (OUT/"baseline-info.json").write_text(json.dumps(baseline,indent=2))
        def inspect():
            value=json.loads(subprocess.check_output([str(HELPER), "--inspect", str(fixture)], env=env))
            def stable(item):
                if isinstance(item,dict):
                    if "voxel_size" in item and "samples" in item:item["samples"].sort()
                    for value in item.values():stable(value)
                elif isinstance(item,list):
                    for value in item:stable(value)
            stable(value);return value
        before_source=inspect()
        await enabled("UV editor...")
        nodes=await layout()
        check(nodes["UV editor..."][1].get("enabled") is True,"Universal menu admits UV authoring for the actual prepared corner target")
        await command("UV editor...")
        editor=await wait(lambda:named("UV editor"));editor.set_input_focus(X.RevertToParent,X.CurrentTime);connection.sync()
        geometry=editor.get_geometry()
        check(editor.get_wm_transient_for().id==window.id and editor.get_wm_class()==window.get_wm_class(),"Colour UV uses the existing native modal identity and parent")
        await wait(lambda:len(set(image(editor).getdata()))>5)
        before=image(editor).crop((0,100,geometry.width,geometry.height-70));image(editor).save(OUT/"before.png")
        tap(editor,geometry.width/12,16)
        await enabled("Undo")
        await wait(lambda:image(editor).crop((0,100,geometry.width,geometry.height-70)).tobytes()!=before.tobytes())
        image(editor).save(OUT/"scaled.png")
        scaled=image(editor).crop((0,100,geometry.width,geometry.height-70)).tobytes()
        check(True,"Actual visible FLTK Scale changes the prepared corner UV draft")
        await command("Undo")
        await wait(lambda:image(editor).crop((0,100,geometry.width,geometry.height-70)).tobytes()==before.tobytes())
        check(True,"Universal Undo restores the exact pre-edit colour UV canvas")
        await enabled("Redo")
        await command("Redo")
        # DBus returns before the FLTK callback; X11 Apply belongs to another
        # transport. A rendered exact draft is the actual client-processing ack.
        await wait(lambda:image(editor).crop((0,100,geometry.width,geometry.height-70)).tobytes()==scaled)
        tap(editor,geometry.width/2+80,geometry.height-16)
        await wait(lambda:named("UV editor") is None)
        await enabled("Save")
        await command("Save")
        accepted=info();after=inspect()
        (OUT/"accepted-info.json").write_text(json.dumps(accepted,indent=2))
        (OUT/"after-scene.json").write_text(json.dumps(after,indent=2))
        (OUT/"before-scene.json").write_text(json.dumps(before_source,indent=2))
        image(window).save(OUT/"after-apply.png")
        target_index=next(i for i,o in enumerate(before_source["objects"]) if o["id"]==before_source["active"])
        current=after["source_materials"];previous=before_source["source_materials"]
        print("DIAGNOSTIC",after["version"],current["version"],baseline["scene_hash"],accepted["scene_hash"],flush=True)
        check(after["version"]==23 and current["version"]==8 and accepted["scene_hash"]!=baseline["scene_hash"],"Actual FLTK Apply and Save produce owning Scene23/material8 colour UV state")
        recipes=current["color_preparations"];latest=next(p for p in reversed(recipes) if "uv_reprojection" in p)
        check(latest["receipt"]==previous["color_preparations"][0]["receipt"] and after["polygon_sources"]["color_fields"]==before_source["polygon_sources"]["color_fields"] and after["polygon_sources"]["color_bases"]==before_source["polygon_sources"]["color_bases"],"Saved colour UV retains immutable initial quality and the complete independent corner field/basis")
        check(all(after["isolated_objects"][i]==before_source["isolated_objects"][i] for i in range(len(after["objects"])) if i!=target_index),"Native colour UV never changes the independently archived read-only source")
        await command("Undo");await command("Save")
        check(info()["scene_hash"]==baseline["scene_hash"],"Global native Undo restores exact complete pre-UV authoring state")
        await command("Redo");await command("Save")
        check(info()["scene_hash"]==accepted["scene_hash"],"Global native Redo restores exact accepted colour UV state")
        await command("UV editor...")
        editor=await wait(lambda:named("UV editor"));editor.set_input_focus(X.RevertToParent,X.CurrentTime);connection.sync();geometry=editor.get_geometry()
        tap(editor,geometry.width/12,16)
        key("Control_L",True);key("w");key("Control_L",False)
        await wait(lambda:named("UV editor") is None)
        await command("Save")
        check(app.poll() is None and info()["scene_hash"]==accepted["scene_hash"],"DomeD Ctrl+W cancels the changed colour UV draft while retaining the saved application")
        image(window).save(OUT/"accepted.png")
        check(True,"Actual prepared-colour FLTK workflow completed")
        connection.close()
    bus.disconnect()


try:
    asyncio.run(main())
finally:
    for process in reversed(processes):
        if process.poll() is None:
            process.terminate()
    for process in reversed(processes):
        try:
            process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            process.kill()
