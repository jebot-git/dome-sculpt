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
            [str(root / "build/dome-sculpt")], env=env, stdout=log, stderr=log
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
        raw = window.get_image(2, 2, 1, 1, X.ZPixmap, 0xFFFFFFFF)
        pixel = Image.frombytes("RGB", (1, 1), raw.data, "raw", "BGRX").getpixel((0, 0))
        check(
            pixel == (48, 48, 48),
            "The local bar yields to the universal host and exposes the DomeD grey viewport",
        )
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
