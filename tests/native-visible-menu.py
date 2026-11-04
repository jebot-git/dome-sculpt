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
BUILD=Path(os.environ["DOME_NATIVE_SKIN_BUILD"])
HELPER=BUILD / "dome-sculpt-native-visible-document-test"
GATED = os.environ.get("DOME_NATIVE_SKIN_GATED") == "1"
LINE = os.environ.get("DOME_NATIVE_LINE") == "1"
APP=BUILD / ("dome-sculpt-native-visible-ui-test" if GATED else "dome-sculpt")
COMPACT = os.environ.get("DOME_SKIN_MENU_COMPACT") == "1"
AFFINE = os.environ.get("DOME_SKIN_MENU_AFFINE") == "1"
FIXTURE_TOOL = "source-visible-menu-fixture"
OUT=ROOT / ("artifacts/native-menu-compact" if COMPACT else "artifacts/native-menu-normal")
if GATED: OUT=OUT.with_name(OUT.name+"-gated")
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
        env = {**os.environ, "HOME": task_home, "DISPLAY": screen, "FLTK_BACKEND": "x11", "XDG_CURRENT_DESKTOP": "Dome", "LIBGL_ALWAYS_SOFTWARE": "1", "XMODIFIERS": "@im=none", "XR_RUNTIME_JSON": "/nonexistent/dome-native-test-no-physical-runtime.json"}
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
        gate=Path(task_home,"worker")
        if GATED: env["DOME_NATIVE_WEIGHT_GATE"]=str(gate)
        subprocess.run([str(HELPER), "--ui-fixture", str(fixture)], check=True)
        log = (OUT / "app.log").open("w")
        app = subprocess.Popen([str(APP), str(fixture)], cwd=task_home, env=env, stdout=log, stderr=log)
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
            return json.loads(subprocess.check_output([str(HELPER), "--inspect", str(fixture)], env=env))

        def enter(widget, y, text):
            geometry = widget.get_geometry()
            tap(widget, geometry.width * .75, y)
            key("Control_L", True); key("a"); key("Control_L", False)
            for char in text:
                key("period" if char == "." else char)
            key("Tab")

        def observer():
            try: return json.loads(Path(str(gate)+".ui.json").read_text())
            except (FileNotFoundError,json.JSONDecodeError): return {}

        def arm():
            for suffix in (".entered",".release",".cancelled"):
                Path(str(gate)+suffix).unlink(missing_ok=True)
            Path(str(gate)+".arm").write_text("inside real helper callback64\n")

        original=inspect()
        check("unskinned" in original["name"] and not original["rig_source"] and not original["bound"], "Genuinely unskinned affine target opens beside the full native reference")
        expected=json.loads(subprocess.check_output([str(HELPER),"--expected",str(fixture)],env=env))
        check(expected["rig_source"] and expected["scene"]!=original["scene"], "Independent actual native helper supplies exact changed native expectation")
        nodes=await layout()
        check(nodes["Bind or transfer skin..."][1].get("enabled") is True,"Actual native target exposes native universal skin authoring")
        await command("Bind or transfer skin...")
        editor=await wait(lambda:named("Bind or transfer skin"))
        editor.set_input_focus(X.RevertToParent,X.CurrentTime);connection.sync()
        image(editor).save(OUT/"source-options-original.png")
        check(editor.get_wm_transient_for().id==window.id,"Native finite-handle options retain native parent identity")
        if COMPACT:
            g=editor.get_geometry()
            check(g.width<=600 and g.height<=320,"Double-size bitmap source options fit640x360")
        key("Return")
        await wait(lambda:named("Bind or transfer skin") is None)
        check(inspect()["scene"]==original["scene"],"Return defaults to Cancel without native mutation")
        await command("Bind or transfer skin...")
        editor=await wait(lambda:named("Bind or transfer skin"))
        editor.set_input_focus(X.RevertToParent,X.CurrentTime);connection.sync()
        g=editor.get_geometry()
        # Native Fl_Choice, not a test command or production hook.
        y=112 if not COMPACT else 160
        tap(editor,g.width*.75,y)
        key("Down");key("Down");key("Down");key("Return")
        await asyncio.sleep(.25)
        image(editor).save(OUT/"native-finite-options.png")
        if LINE:
            # Real keyboard focus traverses the native choice controls; disabled
            # Automatic-only fields are skipped. It also scrolls the compact
            # dialog to the actual contact row without fixed y coordinates.
            for _ in range(5): key("Tab")
            key("space"); key("Down"); key("Return")
            await asyncio.sleep(.25)
            image(editor).save(OUT/"native-line-contact-choice.png")
        g=editor.get_geometry()
        if GATED: arm()
        tap(editor,g.width/2+80,g.height-40 if not COMPACT else g.height-46)
        await wait(lambda:named("Bind or transfer skin") is None)
        if GATED:
            await wait(lambda:Path(str(gate)+".entered").exists())
            if LINE: check(observer().get("native_contact_policy")==1,"Actual desktop contact choice reaches typed native request unchanged")
            check(observer().get("busy") and observer().get("can_save"),"Actual worker is gated inside real Native preparation and exposes accepted Save")
            before_bytes=fixture.read_bytes()
            nodes=await layout()
            await menu.call_event(nodes["Save"][0],"clicked",Variant("i",0),0)
            await wait(lambda:observer().get("saving"))
            check(fixture.read_bytes()==before_bytes and observer().get("busy"),"Actual App accepted Save while helper is running without overwriting the prior file")
            Path(str(gate)+".release").write_text("release real helper\n")
            await wait(lambda:not observer().get("saving",True) and not observer().get("busy",True),120)
            check(Path(str(gate)+".entered").read_text().strip()=="64","Gate measured actual preparation callback64 rather than a fabricated result")
        else:
            await command("Save")
        actual=inspect()
        check(actual["scene"]==expected["scene"],"Native finite-handle active Apply and Save matches complete independent native output")
        restored=json.loads(subprocess.check_output([str(HELPER),"--restore",str(fixture)],env=env))
        check(restored["scene"]==original["scene"],"Saved first source capsule restores complete original unskinned morph/paint/material/placement state")
        await command("Undo");await command("Save")
        check(inspect()["scene"]==original["scene"],"One native Undo restores exact original Scene")
        await command("Redo");await command("Save")
        check(inspect()["scene"]==expected["scene"],"One native Redo restores exact accepted native-finite weights")
        await command("Bind or transfer skin...")
        editor=await wait(lambda:named("Bind or transfer skin"))
        editor.set_input_focus(X.RevertToParent,X.CurrentTime);connection.sync()
        key("Escape")
        await wait(lambda:named("Bind or transfer skin") is None)
        check(inspect()["scene"]==expected["scene"],"Actual Escape options cancellation preserves weights and original capsule")
        if GATED:
            await command("Undo"); await command("Save")
            check(inspect()["scene"]==original["scene"],"Pre-cancellation native Undo restores complete original")
            await command("Bind or transfer skin...")
            editor=await wait(lambda:named("Bind or transfer skin"))
            editor.set_input_focus(X.RevertToParent,X.CurrentTime);connection.sync()
            arm(); g=editor.get_geometry()
            tap(editor,g.width/2+80,g.height-40 if not COMPACT else g.height-46)
            await wait(lambda:Path(str(gate)+".entered").exists())
            await enabled("Cancel background work")
            await command("Cancel background work")
            await wait(lambda:Path(str(gate)+".cancelled").exists())
            await wait(lambda:not observer().get("busy",True),120)
            await command("Save")
            check(inspect()["scene"]==original["scene"],"Actual Cancel background work interrupts real preparation and preserves Scene/file/history")
            await command("Redo"); await command("Save")
            check(inspect()["scene"]==expected["scene"],"Cancelled native preparation leaves the original Redo endpoint intact")
        await command("Close Window")
        await wait(lambda:app.poll() is not None)
        check(app.returncode==0,"Saved native finite-handle app closes gracefully")
        log.flush()
        # Reopen the actually saved file, using the same native application.
        again=subprocess.Popen([str(APP),str(fixture)],cwd=task_home,env=env,stdout=log,stderr=log)
        processes.append(again)
        second=await wait(lambda:named("Dome Sculpt"))
        check(again.poll() is None and inspect()["scene"]==expected["scene"],"Actual fresh application reopens durable accepted Scene and source capsule")
        image(second).save(OUT/"native-finite-reopened.png")
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
