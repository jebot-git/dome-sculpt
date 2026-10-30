#!/usr/bin/env python3
"""Real native FLTK/DBusMenu protection and symmetry workflow; private Xvfb, no display restart."""
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
if os.environ.get("DOME_SCULPT_BAKE_POLICY_BUS") != "1":
    os.execvpe("dbus-run-session", ["dbus-run-session", "--", sys.executable, *sys.argv],
              {**os.environ, "DOME_SCULPT_BAKE_POLICY_BUS": "1"})
ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get("DOME_SCULPT_TEST_BUILD",str(ROOT / "build")))
COMPACT = os.environ.get("DOME_SCULPT_BAKE_POLICY_COMPACT") == "1"
OUT = ROOT / ("artifacts/menu-bake-policy-compact" if COMPACT else "artifacts/menu-bake-policy-workflow")
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
    raise TimeoutError("Native resolution workflow did not reach expected state")


async def main():
    bus = await MessageBus().connect()
    await bus.request_name("com.canonical.AppMenu.Registrar")
    with tempfile.TemporaryDirectory(prefix="dome-bake-policy-menu-") as task_home:
        read, write = os.pipe()
        log = (OUT / "xvfb.log").open("w")
        processes.append(subprocess.Popen(["Xvfb", "-displayfd", str(write), "-screen", "0", "640x360x24" if COMPACT else "1280x1000x24", "-nolisten", "tcp"], pass_fds=(write,), stdout=log, stderr=log))
        os.close(write)
        with os.fdopen(read) as stream:
            screen = ":" + stream.readline().strip()
        env = {**os.environ, "HOME": task_home, "DISPLAY": screen, "FLTK_BACKEND": "x11", "XDG_CURRENT_DESKTOP": "Dome", "LIBGL_ALWAYS_SOFTWARE": "1", "XMODIFIERS": "@im=none"}
        for key in ("WAYLAND_DISPLAY", "WAYLAND_SOCKET"):
            env.pop(key, None)
        for key, suffix in [("XDG_STATE_HOME", "state"), ("XDG_CONFIG_HOME", "config"), ("XDG_CACHE_HOME", "cache"), ("XDG_DATA_HOME", "data"), ("XDG_RUNTIME_DIR", "runtime")]:
            env[key] = task_home + "/" + suffix
            Path(env[key]).mkdir(mode=0o700)
        if COMPACT:
            config=Path(task_home,"config/dome/appearance.json")
            config.parent.mkdir(parents=True,exist_ok=True)
            config.write_text(json.dumps({"font":"Unifont / Sans-Serif","font_size":16,"font_scale":2}))
        fixture = Path(task_home, "procedural.dsculpt")
        subprocess.run([str(BUILD / "dome-sculpt-bake-projection-policy-test"), "--ui-fixture", str(fixture)], check=True)
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

        window = await wait(lambda: named("Dome Sculpt"))
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
            saved = fixture.stat().st_mtime_ns if label == "Save" else None
            await menu.call_event(nodes[label][0], "clicked", Variant("i", 0), 0)
            if saved is not None:
                await wait(lambda: fixture.stat().st_mtime_ns != saved)
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

        def inspect():
            return json.loads(subprocess.check_output([str(BUILD / "dome-sculpt-bake-projection-policy-test"), "--inspect", str(fixture)], env=env))

        def enter(widget, y, text):
            geometry = widget.get_geometry()
            # The numeric field precedes its paired continuous slider.
            tap(widget, min(geometry.width/2,240) + 40, y)
            key("Control_L", True); key("a"); key("Control_L", False)
            for char in text:
                key("period" if char == "." else char)
            key("Tab")

        async def open_options():
            await enabled("Bake surface...")
            await command("Bake surface...")
            editor=await wait(lambda:named("Bake surface"))
            editor.set_input_focus(X.RevertToParent,X.CurrentTime);connection.sync()
            return editor

        async def close_options():
            await wait(lambda:named("Bake surface") is None)

        async def saved():
            await enabled("Save");await command("Save");return inspect()

        def canonical(scene):
            for obj in scene["objects"]:
                volume=obj["volume"];volume["samples"].sort()
                for layer in volume.get("sculpt_layers",[]):layer["delta"].sort()
                if "sculpt_mask" in volume:volume["sculpt_mask"]["values"].sort()
            return scene

        original=canonical(inspect());editor=await open_options();nodes=await layout()
        check(editor.get_wm_transient_for().id==window.id and editor.get_wm_class()==window.get_wm_class() and not nodes["Bake surface..."][1].get("enabled"),"Projection options preserve modal ownership and universal-menu isolation")
        image(editor).save(OUT/"projection-options.png")
        if COMPACT:
            geometry=editor.get_geometry();check(geometry.width<=600 and geometry.height<=320,"Double-size projection options fit640x360")
            before=image(editor)
            for _ in range(60):
                xtest.fake_input(connection,X.MotionNotify,x=geometry.x+geometry.width-10,y=geometry.y+70)
                xtest.fake_input(connection,X.ButtonPress,5);xtest.fake_input(connection,X.ButtonRelease,5)
            connection.sync();await asyncio.sleep(.25);after=image(editor);after.save(OUT/"projection-options-bottom.png")
            check(before.crop((0,0,geometry.width,geometry.height-100)).tobytes()!=after.crop((0,0,geometry.width,geometry.height-100)).tobytes(),"Compact scrolling reaches projection policy while action buttons stay pinned")
            key("Control_L",True);key("w");key("Control_L",False);await close_options()
            check(app.poll() is None and canonical(inspect())==original,"Compact Ctrl+W cancels options and retains all baked data")
            connection.close();bus.disconnect();return
        key("Return");await close_options();check(canonical(inspect())==original,"Default Return cancels without starting a bake")
        editor=await open_options();g=editor.get_geometry();tap(editor,g.width*.75,g.height-48)
        await wait(lambda:"failed" in str(editor.get_wm_name() or ""));image(editor).save(OUT/"strict-projection-refusal.png")
        await command("Close Window");await close_options();await command("Save")
        check(canonical(inspect())==original,"Native default strict projection reports failure and retains exact target and source")
        editor=await open_options();g=editor.get_geometry();tap(editor,40,306)
        image(editor).save(OUT/"explicit-partial-projection.png")
        tap(editor,g.width*.75,g.height-48);await wait(lambda:"complete" in str(editor.get_wm_name() or ""));image(editor).save(OUT/"partial-projection-complete.png")
        await command("Close Window");await close_options();await command("Save");accepted=canonical(inspect())
        before=original["objects"][0]["painted"]["baked"];after=accepted["objects"][0]["painted"]["baked"]
        def raw(value):return value["bytes"]
        kept=all(raw(after[channel])[4*(y*64+x):4*(y*64+x)+4]==raw(before[channel])[4*(y*64+x):4*(y*64+x)+4] for channel in ("color","orm","emission","normal") for y in range(64) for x in range(32,64))
        blue=all(raw(after["color"])[4*(y*64+x)]==0 and raw(after["color"])[4*(y*64+x)+2]==255 for y in range(64) for x in range(32))
        check(kept and blue,"Actual FLTK partial option updates hits and keeps all missed baked channels byte-exact")
        check(accepted["objects"][1:]==original["objects"][1:] and accepted["objects"][0]["painted"]["layers"]==original["objects"][0]["painted"]["layers"],"Projection policy preserves every source and editable paint layer")
        await command("Undo");await command("Save");check(canonical(inspect())==original,"One universal Undo restores exact full original bake")
        await command("Redo");await command("Save");check(canonical(inspect())==accepted,"One universal Redo restores accepted partial projection")
        connection.close()
    bus.disconnect()

try:
    asyncio.run(main())
finally:
    for process in reversed(processes):
        if process.poll() is None:process.terminate()
    for process in reversed(processes):
        try:process.wait(timeout=8)
        except subprocess.TimeoutExpired:process.kill()
