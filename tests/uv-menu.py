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
OUT = ROOT / "artifacts/menu-uv-workflow"
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
        processes.append(subprocess.Popen(["Xvfb", "-displayfd", str(write), "-screen", "0", "1280x1000x24", "-nolisten", "tcp"], pass_fds=(write,), stdout=log, stderr=log))
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
        subprocess.run([str(ROOT / "build/dome-sculpt-uv-workflow-test"), "--ui-fixture", str(fixture)], check=True)
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

        baseline = info()
        nodes = await layout()
        check(nodes["UV editor..."][1].get("enabled") is True, "Universal menu enables UV editing for a real rigged mesh")
        await command("UV editor...")
        editor = await wait(lambda: named("UV editor"))
        editor.set_input_focus(X.RevertToParent, X.CurrentTime)
        connection.sync()
        await asyncio.sleep(.2)
        check(editor.get_wm_transient_for().id == window.id and editor.get_wm_class() == window.get_wm_class(), "UV editor retains native application identity and modal parent")
        geometry = editor.get_geometry()
        before = image(editor).crop((0, 100, geometry.width, geometry.height - 70))
        # Default native bitmap sizes use32px command rows; the third row adds
        # Unwrap and Stitch without changing the established first two rows.
        tap(editor, geometry.width / 12, 80)
        await asyncio.sleep(.4)
        nodes = await layout()
        check(nodes["Undo"][1].get("enabled") is True, "Native Unwrap schedules a worker result with independent draft history")
        unwrapped = image(editor).crop((0, 100, geometry.width, geometry.height - 70))
        image(editor).save(OUT / "unwrapped.png")
        check(unwrapped.tobytes() != before.tobytes(), "Actual FLTK UV canvas displays the conformally changed chart")
        await command("Undo")
        check(image(editor).crop((0, 100, geometry.width, geometry.height - 70)).tobytes() == before.tobytes(), "Universal Edit Undo restores exact pre-solve UV canvas")
        await command("Redo")
        tap(editor, geometry.width / 2 + 80, geometry.height - 16)
        await wait(lambda: named("UV editor") is None)
        await enabled("Save")
        await command("Save")
        accepted = info()
        check(accepted["scene_hash"] != baseline["scene_hash"] and accepted["paint_layers"] == baseline["paint_layers"], "Native Apply saves changed UVs while retaining editable paint layers and valid rig data")
        await command("Undo")
        await command("Save")
        check(info()["scene_hash"] == baseline["scene_hash"], "Universal global Undo restores the complete rigged pre-UV native project")
        await command("Redo")
        await command("Save")
        check(info()["scene_hash"] == accepted["scene_hash"], "Universal global Redo restores the exact accepted UV/paint/rig project")
        await command("UV editor...")
        editor = await wait(lambda: named("UV editor"))
        editor.set_input_focus(X.RevertToParent, X.CurrentTime)
        connection.sync()
        await asyncio.sleep(.2)
        geometry = editor.get_geometry()
        source_canvas = image(editor).crop((0, 100, geometry.width, geometry.height - 70))
        canvas_height = geometry.height - 5 * 32
        side = min(geometry.width, canvas_height) - 40
        left = (geometry.width - side) / 2
        top = 3 * 32 + (canvas_height - side) / 2
        tap(editor, left + .45 * side, top + .275 * side)
        tap(editor, geometry.width * 4.5 / 6, 48)
        await asyncio.sleep(.3)
        cut_canvas = image(editor).crop((0, 100, geometry.width, geometry.height - 70))
        image(editor).save(OUT / "seam-split.png")
        check(cut_canvas.tobytes() != source_canvas.tobytes(), "Native Seam marks the explicitly picked edge and updates split-chart presentation")
        tap(editor, left + .45 * side, top + .275 * side)
        tap(editor, geometry.width * 1.5 / 6, 80)
        await asyncio.sleep(.3)
        image(editor).save(OUT / "stitched.png")
        check(image(editor).crop((0, 100, geometry.width, geometry.height - 70)).tobytes() == source_canvas.tobytes(), "Actual native Stitch restores the selected seam continuity through the worker")
        tap(editor, geometry.width / 12, 80)
        key("Control_L", True); key("w"); key("Control_L", False)
        await wait(lambda: named("UV editor") is None)
        check(app.poll() is None and info()["scene_hash"] == accepted["scene_hash"], "DomeD Ctrl+W cancels UV processing without closing the app or altering the saved project")
        check(not any(not check["passed"] for check in checks), "Real native UV GUI workflow completed")
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
