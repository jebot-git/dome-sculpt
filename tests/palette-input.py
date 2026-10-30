#!/usr/bin/env python3
"""Native UTF-8 input/clipboard/model roundtrip; private FLTK display and bus."""
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
if os.environ.get("DOME_PALETTE_INPUT_BUS") != "1":
    os.execvpe("dbus-run-session", ["dbus-run-session", "--", sys.executable, *sys.argv],
              {**os.environ, "DOME_PALETTE_INPUT_BUS": "1"})
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "artifacts/menu-palette-input"
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
    raise TimeoutError("Native Unicode workflow did not reach expected state")


async def main():
    bus = await MessageBus().connect()
    await bus.request_name("com.canonical.AppMenu.Registrar")
    with tempfile.TemporaryDirectory(prefix="dome-skin-menu-") as task_home:
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

        def image(widget):
            geometry = widget.get_geometry()
            raw = widget.get_image(0, 0, geometry.width, geometry.height, X.ZPixmap, 0xFFFFFFFF)
            return Image.frombytes("RGB", (geometry.width, geometry.height), raw.data, "raw", "BGRX")

        async def clipboard_text():
            reader = connection.screen().root.create_window(0, 0, 1, 1, 0, X.CopyFromParent)
            target = connection.intern_atom("UTF8_STRING")
            property_atom = connection.intern_atom("DOME_SCULPT_UNICODE_TEST")
            try:
                reader.convert_selection(connection.intern_atom("CLIPBOARD"), target,
                                         property_atom, X.CurrentTime)
                connection.flush()
                deadline = time.monotonic() + 8
                while time.monotonic() < deadline:
                    while connection.pending_events():
                        event = connection.next_event()
                        if event.type == X.SelectionNotify and event.requestor.id == reader.id:
                            if event.property == X.NONE:
                                raise AssertionError("Native clipboard refused UTF-8 input")
                            data = reader.get_full_property(property_atom, target)
                            return bytes(data.value).decode("utf-8")
                    await asyncio.sleep(.01)
                raise TimeoutError("Native UTF-8 clipboard did not acknowledge input")
            finally:
                reader.destroy()
                connection.flush()

        async def enabled(label):
            for _ in range(400):
                nodes = await layout()
                if nodes[label][1].get("enabled", True):
                    return
                await asyncio.sleep(.02)
            raise TimeoutError("Native command remained busy: " + label)

        def inspect():
            return json.loads(subprocess.check_output([str(ROOT / "build/dome-sculpt-skin-authoring-test"), "--inspect", str(fixture)], env=env))

        original = inspect()
        target = next(o for o in original["scene"]["objects"] if o["id"] == 2)
        original_name = target["name"]
        await enabled("Object and material...")
        await command("Object and material...")
        editor = await wait(lambda: named("Object and material"))
        editor.set_input_focus(X.RevertToParent, X.CurrentTime); connection.sync()
        geometry = editor.get_geometry()
        tap(editor, geometry.width * .7, 40)
        await command("Select All")
        # The private Xvfb keymap provides real Unicode keysyms. FLTK performs
        # its normal Xutf8LookupString input conversion, without test-only app APIs.
        value = "Žluťoučký kůň 中文 日本語 é"
        characters = list(dict.fromkeys(value))
        first_code = connection.display.info.max_keycode - len(characters) + 1
        previous = connection.get_keyboard_mapping(first_code, len(characters))
        mappings = [(ord(char) if ord(char) < 256 else 0x01000000 | ord(char),) for char in characters]
        try:
            # Install once: recycling one keycode between queued events races
            # the application's MappingNotify/keysym cache updates.
            connection.change_keyboard_mapping(first_code, mappings)
            connection.sync()
            await asyncio.sleep(.2)
            for char in value:
                code = first_code + characters.index(char)
                xtest.fake_input(connection, X.KeyPress, code)
                xtest.fake_input(connection, X.KeyRelease, code)
                connection.sync()
                if os.environ.get("DOME_SCULPT_UNICODE_BURST") != "1":
                    await asyncio.sleep(.025)
            # XSync acknowledges the server, not FLTK's input processing. The
            # D-Bus menu is a separate input channel, so first copy through the
            # same X11 event queue as typing. A new clipboard owner proves FLTK
            # processed that copy before we request its UTF-8 selection data.
            clipboard = connection.intern_atom("CLIPBOARD")
            assert not connection.get_selection_owner(clipboard), "Private clipboard unexpectedly already owned"
            key("Control_L", True); key("a"); key("Control_L", False)
            key("Control_L", True); key("c"); key("Control_L", False)
            await wait(lambda: connection.get_selection_owner(clipboard), seconds=8)
            received = await clipboard_text()
            print("NATIVE_INPUT", repr(received), "EXPECTED", repr(value), flush=True)
            check(received == value,
                  "Native clipboard acknowledges all Unicode input before test keymap restoration")
            await command("Select All"); await command("Copy"); await command("Cut"); await command("Paste")
        finally:
            connection.change_keyboard_mapping(first_code, previous)
            connection.sync()
        image(editor).save(OUT / "utf8-input-clipboard.png")
        tap(editor, geometry.width / 2 + 80, geometry.height - 40)
        await wait(lambda: named("Object and material") is None)
        await enabled("Save"); await command("Save")
        result = inspect()
        edited = next(o for o in result["scene"]["objects"] if o["id"] == target["id"])
        print("INPUT", repr(edited["name"]), "EXPECTED", repr(value), flush=True)
        check(edited["name"] == value, "Native FLTK Unicode input and universal Copy/Cut/Paste preserve exact UTF-8 model names")
        await command("Undo"); await command("Save")
        restored = inspect()
        check(next(o for o in restored["scene"]["objects"] if o["id"] == target["id"])["name"] == original_name,
              "Unicode native naming is a single model-history transaction")
        await command("Redo"); await command("Save")
        check(next(o for o in inspect()["scene"]["objects"] if o["id"] == target["id"])["name"] == value,
              "Unicode names survive native save/reload and redo")
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
