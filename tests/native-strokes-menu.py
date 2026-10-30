#!/usr/bin/env python3
"""Real native background strokes, cancellation and ordered Save/Close; private bus/display."""
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
if os.environ.get("DOME_NATIVE_GUI_BUS") != "1":
    os.execvpe("dbus-run-session", ["dbus-run-session", "--", sys.executable, *sys.argv],
              {**os.environ, "DOME_NATIVE_GUI_BUS": "1"})
ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get("DOME_SCULPT_TEST_BUILD",str(ROOT / "build")))
COMPACT = os.environ.get("DOME_NATIVE_GUI_COMPACT") == "1"
PAINT=os.environ.get("DOME_NATIVE_GUI_PAINT")=="1"
OUT = ROOT / ("artifacts/menu-native-strokes-failure" if os.environ.get("DOME_NATIVE_FAIL") else "artifacts/menu-native-strokes-paint" if PAINT else "artifacts/menu-native-strokes-accept" if os.environ.get("DOME_NATIVE_GUI_ACCEPT")=="1" else "artifacts/menu-native-strokes-cancel")
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
    with tempfile.TemporaryDirectory(prefix="dome-native-menu-") as task_home:
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
        fixture = Path(task_home, "resolution.dsculpt")
        subprocess.run([str(BUILD / "dome-sculpt-native-strokes-test"), "--paint-fixture" if PAINT else "--ui-fixture", str(fixture)], check=True)
        log = (OUT / "app.log").open("w")
        env["LD_PRELOAD"]=str(BUILD/"libdome-sculpt-native-gui-gate.so");env["DOME_NATIVE_GATE"]=str(Path(task_home,"gate"))
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
            return json.loads(subprocess.check_output([str(BUILD / "dome-sculpt-native-strokes-test"), "--inspect", str(fixture)], env=env))

        def enter(widget, y, text):
            geometry = widget.get_geometry()
            # The numeric field precedes its paired continuous slider.
            tap(widget, min(geometry.width/2,240) + 40, y)
            key("Control_L", True); key("a"); key("Control_L", False)
            for char in text:
                key("period" if char == "." else char)
            key("Tab")

        async def open_options():
            await enabled("Resolution...")
            await command("Resolution...")
            editor=await wait(lambda:named("Sculpt resolution"))
            editor.set_input_focus(X.RevertToParent,X.CurrentTime);connection.sync()
            return editor

        async def close_options():
            await wait(lambda:named("Sculpt resolution") is None)

        async def saved():
            await enabled("Save");await command("Save");return inspect()

        def canonical(scene):
            for obj in scene["objects"]:
                volume=obj["volume"];volume["samples"].sort()
                for layer in volume.get("sculpt_layers",[]):layer["delta"].sort()
                if "sculpt_mask" in volume:volume["sculpt_mask"]["values"].sort()
            return scene

        original=canonical(inspect());baseline=info()
        await enabled("Save" if PAINT else "Resolution...")
        point=(window.get_geometry().width//2+(190 if PAINT else 0),window.get_geometry().height//2)
        await wait(lambda:max(image(window).getpixel(point))-min(image(window).getpixel(point))>20)
        gate=Path(task_home,"gate");gate.touch()
        geometry=window.get_geometry();cx=geometry.width//2;cy=geometry.height//2
        baseline_status=image(window).crop((0,geometry.height-28,geometry.width,geometry.height)).tobytes()
        key("Shift_L",True)
        xtest.fake_input(connection,X.MotionNotify,x=geometry.x+cx,y=geometry.y+cy)
        xtest.fake_input(connection,X.ButtonPress,1);connection.sync()
        await wait(lambda:Path(str(gate)+".hit").exists())
        def status_pixels():return image(window).crop((0,geometry.height-28,geometry.width,geometry.height)).tobytes()
        await wait(lambda:status_pixels()!=baseline_status)
        await enabled("Undo")
        # The actual dedicated worker is blocked inside detached preparation.
        # XSync alone is insufficient; menu state must be exported by FLTK's
        # native timer after it handles the real Escape/Save/Close events.
        if os.environ.get("DOME_NATIVE_GUI_ACCEPT")!="1":
            start=time.monotonic();key("Escape")
            async def cancelled():
                nodes=await layout();return not nodes["Undo"][1].get("enabled",True)
            end=time.monotonic()+2
            while not await cancelled() and time.monotonic()<end:await asyncio.sleep(.01)
            latency=time.monotonic()-start
            check(latency<.75 and await cancelled(),"FLTK processes Escape and exports cancelled Undo state while native worker is blocked")
            xtest.fake_input(connection,X.ButtonRelease,1);key("Shift_L",False);connection.sync()
            await command("Save")
            check(canonical(inspect())==original,"Save stays usable and retains exact source while cancelled worker preparation is blocked")
            image(window).save(OUT/"cancel-responsive.png")
            Path(str(gate)+".release").touch()
            await command("Close Window");await wait(lambda:app.poll() is not None)
            check(app.returncode==0,"Cancelled preparation drains through normal native Close/shutdown")
        else:
            for dx in (4,8,12,16):
                xtest.fake_input(connection,X.MotionNotify,x=geometry.x+cx+dx,y=geometry.y+cy)
            held_status=status_pixels()
            xtest.fake_input(connection,X.ButtonRelease,1);key("Shift_L",False);connection.sync()
            await wait(lambda:status_pixels()!=held_status) # Native rendered release acknowledgement.
            stamp=fixture.stat().st_mtime_ns;released_status=status_pixels()
            nodes=await layout();await menu.call_event(nodes["Save"][0],"clicked",Variant("i",0),0)
            await wait(lambda:status_pixels()!=released_status) # FLTK acknowledged queued Save.
            save_status=status_pixels();nodes=await layout();await menu.call_event(nodes["Close Window"][0],"clicked",Variant("i",0),0)
            await wait(lambda:status_pixels()!=save_status) # FLTK acknowledged queued Close.
            check(app.poll() is None and fixture.stat().st_mtime_ns==stamp,"Accepted release and Save/Close stay pending while ordered native preparation is blocked")
            queued_status=status_pixels();Path(str(gate)+".release").touch()
            if os.environ.get("DOME_NATIVE_FAIL"):
                async def failed():
                    nodes=await layout();return not nodes["Undo"][1].get("enabled",True)
                end=time.monotonic()+20
                while not await failed() and time.monotonic()<end:await asyncio.sleep(.01)
                await wait(lambda:status_pixels()!=queued_status)
                check(app.poll() is None and fixture.stat().st_mtime_ns==stamp and canonical(inspect())==original,"Worker allocation failure cancels deferred Save/Close and retains exact original project with visible status")
                image(window).save(OUT/"failure-retained.png")
                await command("Close Window");await wait(lambda:app.poll() is not None)
                check(app.returncode==0,"Explicit Close remains usable after failed native transaction")
                connection.close();bus.disconnect();return
            await wait(lambda:app.poll() is not None)
            check(app.returncode==0,"Native accepted stroke, Save and Close drain in order without blocking FLTK")
            after=canonical(inspect())
            if PAINT:
                a=after["objects"][0].copy();b=original["objects"][0].copy();painted=a.pop("painted");before=b.pop("painted")
                check(a==b and painted["mesh"]==before["mesh"] and painted!=before,"Actual posed FLTK painting changes original UV layers while preserving exact rest/skin/morph/transform/source fields")
            else:check(after!=original and info()["field_hash"]!=baseline["field_hash"],"Final native save contains the complete accepted geometry gesture")
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
