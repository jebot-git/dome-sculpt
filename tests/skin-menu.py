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
if os.environ.get("DOME_SKIN_MENU_BUS") != "1":
    os.execvpe("dbus-run-session", ["dbus-run-session", "--", sys.executable, *sys.argv],
              {**os.environ, "DOME_SKIN_MENU_BUS": "1"})
ROOT = Path(__file__).resolve().parents[1]
COMPACT = os.environ.get("DOME_SKIN_MENU_COMPACT") == "1"
OUT = ROOT / ("artifacts/menu-skin-compact" if COMPACT else "artifacts/menu-skin-workflow")
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
    with tempfile.TemporaryDirectory(prefix="dome-skin-menu-") as task_home:
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
        fixture = Path(task_home, "skin.dsculpt")
        subprocess.run([str(ROOT / "build/dome-sculpt-skin-authoring-test"), "--ui-fixture", str(fixture)], check=True)
        log = (OUT / "app.log").open("w")
        app = subprocess.Popen([str(ROOT / "build/dome-sculpt"), str(fixture)], cwd=task_home, env=env, stdout=log, stderr=log)
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
            await menu.call_event(nodes[label][0], "clicked", Variant("i", 0), 0)
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
            return json.loads(subprocess.check_output([str(ROOT / "build/dome-sculpt"), "--project-info", str(fixture)], env=env))

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
            return json.loads(subprocess.check_output([str(ROOT / "build/dome-sculpt-skin-authoring-test"), "--inspect", str(fixture)], env=env))

        def enter(widget, y, text):
            geometry = widget.get_geometry()
            tap(widget, geometry.width * .75, y)
            key("Control_L", True); key("a"); key("Control_L", False)
            for char in text:
                key("period" if char == "." else char)
            key("Tab")

        baseline = info()
        original = inspect()
        nodes = await layout()
        check(nodes["Bind or transfer skin..."][1].get("enabled") is True and not original["skinned"], "Prepared unskinned targets expose the universal skin-authoring command")
        await command("Bind or transfer skin...")
        editor = await wait(lambda: named("Bind or transfer skin"))
        editor.set_input_focus(X.RevertToParent, X.CurrentTime); connection.sync()
        nodes = await layout()
        check(editor.get_wm_transient_for().id == window.id and editor.get_wm_class() == window.get_wm_class() and not nodes["Bind or transfer skin..."][1].get("enabled"), "Native skin options inherit the application identity and isolate scene commands")
        image(editor).save(OUT / "skin-options.png")
        if COMPACT:
            geometry=editor.get_geometry()
            check(geometry.width<=600 and geometry.height<=320,"Double-size DomeD BDF skin options fit the640x360 work area")
            before=image(editor)
            for _ in range(20):
                xtest.fake_input(connection,X.MotionNotify,x=geometry.x+geometry.width-10,y=geometry.y+80)
                xtest.fake_input(connection,X.ButtonPress,5);xtest.fake_input(connection,X.ButtonRelease,5)
            connection.sync();await asyncio.sleep(.2)
            after=image(editor);after.save(OUT / "skin-options-scrolled.png")
            check(before.crop((0,0,geometry.width,geometry.height-92)).tobytes()!=after.crop((0,0,geometry.width,geometry.height-92)).tobytes(),"Compact skin options scroll to their remaining controls")
            key("Control_L",True);key("w");key("Control_L",False)
            await wait(lambda:named("Bind or transfer skin") is None)
            check(app.poll() is None and info()["scene_hash"]==baseline["scene_hash"],"Compact native Ctrl+W preserves the complete original project")
            connection.close();bus.disconnect();return

        await command("Close Window")
        await wait(lambda: named("Bind or transfer skin") is None)
        check(info()["scene_hash"] == baseline["scene_hash"] and app.poll() is None, "Universal Close cancels skin options without publishing a rig or weights")
        await command("Bind or transfer skin...")
        editor = await wait(lambda: named("Bind or transfer skin"))
        editor.set_input_focus(X.RevertToParent, X.CurrentTime); connection.sync()
        key("Return")
        await wait(lambda: named("Bind or transfer skin") is None)
        check(info()["scene_hash"] == baseline["scene_hash"], "Return initially activates Cancel rather than binding the target")
        await command("Bind or transfer skin...")
        editor = await wait(lambda: named("Bind or transfer skin"))
        editor.set_input_focus(X.RevertToParent, X.CurrentTime); connection.sync()
        enter(editor, 176, "500")
        enter(editor, 208, "0")
        enter(editor, 304, "2")
        geometry = editor.get_geometry(); tap(editor, geometry.width / 2 + 80, geometry.height - 40)
        await wait(lambda: named("Bind or transfer skin") is None)
        await enabled("Save"); await command("Save")
        automatic = info(); bound = inspect()
        check(bound["skinned"] and automatic["scene_hash"] != baseline["scene_hash"], "Native Automatic bind commits an actual complete skeleton and skin through the worker")
        check(bound["scene"]["objects"][0] == original["scene"]["objects"][0] and automatic["paint_hash"] == baseline["paint_hash"], "Automatic binding preserves the reference object and original target painting")
        await command("Undo"); await command("Save")
        check(info()["scene_hash"] == baseline["scene_hash"], "Universal Undo removes the entire native bind in one exact step")
        await command("Redo"); await command("Save")
        check(info()["scene_hash"] == automatic["scene_hash"], "Universal Redo restores the complete accepted native skin")
        await command("Bind or transfer skin...")
        editor = await wait(lambda: named("Bind or transfer skin"))
        editor.set_input_focus(X.RevertToParent, X.CurrentTime); connection.sync()
        geometry = editor.get_geometry(); tap(editor, geometry.width * .75, 112)
        key("space", True); key("Down"); key("Return"); key("space", False)
        await asyncio.sleep(.15)
        image(editor).save(OUT / "skin-transfer-options.png")
        tap(editor, geometry.width / 2 + 80, geometry.height - 40)
        await wait(lambda: named("Bind or transfer skin") is None)
        await enabled("Save"); await command("Save")
        transferred = info(); transfer = inspect()
        expected = original["scene"]["objects"][0]["deformation"]["skin"]
        check(transfer["skinned"] and [dict(row) for row in transfer["scene"]["objects"][1]["deformation"]["skin"]["vertices"]] == [dict(row) for row in expected["vertices"]], "Actual native method selection transfers the independently known reference weights exactly")
        await command("Undo"); await command("Save")
        check(info()["scene_hash"] == automatic["scene_hash"], "Transferred weights form one complete native history transaction")
        await command("Bind or transfer skin...")
        editor = await wait(lambda: named("Bind or transfer skin"))
        editor.set_input_focus(X.RevertToParent, X.CurrentTime); connection.sync()
        key("Control_L", True); key("w"); key("Control_L", False)
        await wait(lambda: named("Bind or transfer skin") is None)
        check(app.poll() is None and info()["scene_hash"] == automatic["scene_hash"], "DomeD Ctrl+W cancels native skin options and retains the application and project")
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
