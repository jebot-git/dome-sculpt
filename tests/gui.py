#!/usr/bin/env python3
"""Drive native FLTK/GL on a private display; no user document or session is used."""

import json, os, signal, subprocess, sys, tempfile, time
from pathlib import Path
from Xlib import X, XK, display
from Xlib.ext import xtest
from PIL import Image, ImageChops

signal.pthread_sigmask(signal.SIG_SETMASK, [])
if os.environ.get("DOME_SCULPT_TEST_BUS") != "1":
    os.execvpe(
        "dbus-run-session",
        ["dbus-run-session", "--", sys.executable, *sys.argv],
        {**os.environ, "DOME_SCULPT_TEST_BUS": "1"},
    )
root = Path(__file__).resolve().parents[1]
out = root / "artifacts/gui"
out.mkdir(parents=True, exist_ok=True)
processes = []
checks = []


def check(value, label):
    checks.append({"passed": bool(value), "check": label})
    print(("PASS " if value else "FAIL ") + label, flush=True)
    (out / "report.json").write_text(
        json.dumps(
            {"checks": checks, "failed": sum(not c["passed"] for c in checks)}, indent=2
        )
        + "\n"
    )
    assert value, label


try:
    with tempfile.TemporaryDirectory(prefix="dome-sculpt-gui-") as home:
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
        with os.fdopen(rd) as stream:
            screen = ":" + stream.readline().strip()
        env = {
            **os.environ,
            "HOME": home,
            "XDG_RUNTIME_DIR": home,
            "XDG_CONFIG_HOME": home + "/config",
            "XDG_CACHE_HOME": home + "/cache",
            "XDG_DATA_HOME": home + "/data",
            "DISPLAY": screen,
            "FLTK_BACKEND": "x11",
            "LIBGL_ALWAYS_SOFTWARE": "1",
            "XDG_CURRENT_DESKTOP": "Dome",
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
        until = time.monotonic() + 90
        window = None
        while time.monotonic() < until:
            assert app.poll() is None, (out / "app.log").read_text()
            window = next(
                (
                    w
                    for w in d.screen().root.query_tree().children
                    if "Dome Sculpt" in str(w.get_wm_name() or "")
                ),
                None,
            )
            if window:
                break
            time.sleep(0.05)
        assert window, "No native sculpt window appeared"
        window.set_input_focus(X.RevertToParent, X.CurrentTime)
        d.sync()
        time.sleep(0.5)

        def motion(x, y):
            g = window.get_geometry()
            xtest.fake_input(d, X.MotionNotify, x=g.x + x, y=g.y + y)
            d.sync()
            time.sleep(0.2)

        def key(name, down=None):
            kinds = (
                [X.KeyPress, X.KeyRelease]
                if down is None
                else [X.KeyPress if down else X.KeyRelease]
            )
            for kind in kinds:
                xtest.fake_input(
                    d, kind, d.keysym_to_keycode(XK.string_to_keysym(name))
                )
                d.sync()
                time.sleep(0.05)

        def button(down):
            xtest.fake_input(d, X.ButtonPress if down else X.ButtonRelease, 1)
            d.sync()
            time.sleep(0.2)

        def shot(name):
            time.sleep(0.25)
            g = window.get_geometry()
            raw = window.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
            image = Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX")
            image.save(out / (name + ".png"))
            return image.crop((80, 100, g.width - 80, g.height - 80))

        before = shot("01-native-sculpt")
        check(
            before.getcolors(before.width * before.height) is not None
            and len(before.getcolors(before.width * before.height)) > 100,
            "FLTK/OpenGL renders a shaded native sculpt surface",
        )
        key("2")
        motion(550, 365)
        key("Shift_L", True)
        button(True)
        for x in (565, 580, 595):
            motion(x, 365)
        button(False)
        key("Shift_L", False)
        carved = shot("02-carved")
        check(
            ImageChops.difference(before, carved).getbbox() is not None,
            "Shift and trackball drag applies the selected carving tool",
        )
        key("Control_L", True)
        key("z")
        key("Control_L", False)
        restored = shot("03-undo")
        check(
            ImageChops.difference(before, restored).getbbox() is None,
            "Native Ctrl+Z restores the pre-stroke surface exactly",
        )
        motion(550, 400)
        button(True)
        motion(650, 460)
        button(False)
        orbited = shot("04-trackball")
        check(
            ImageChops.difference(restored, orbited).getbbox() is not None,
            "Default left drag uses trackball navigation",
        )
        key("f")
        reset = shot("05-view-reset")
        check(
            ImageChops.difference(restored, reset).getbbox() is None,
            "Fit/reset restores the trackball view without changing the volume",
        )
        key("Control_L", True)
        key("d")
        key("Control_L", False)
        time.sleep(0.5)
        duplicate = shot("06-duplicate")
        check(
            ImageChops.difference(reset, duplicate).getbbox() is not None,
            "Native Scene command duplicates and displays an independent object",
        )
        key("Control_L", True)
        key("z")
        key("Control_L", False)
        time.sleep(0.5)
        undo_object = shot("07-undo-object")
        check(
            ImageChops.difference(reset, undo_object).getbbox() is None,
            "Object creation undo restores the original viewport exactly",
        )
        key("Control_L", True)
        key("Shift_L", True)
        key("n")
        key("Shift_L", False)
        key("Control_L", False)
        time.sleep(0.5)
        key("Alt_L", True)
        key("Return")
        key("Alt_L", False)
        until = time.monotonic() + 5
        editor = None
        while time.monotonic() < until:
            editor = next(
                (
                    w
                    for w in d.screen().root.query_tree().children
                    if "Object and material" in str(w.get_wm_name() or "")
                ),
                None,
            )
            if editor:
                break
            time.sleep(0.05)
        check(
            editor is not None,
            "Object properties open as a native FLTK material editor",
        )
        editor.set_input_focus(X.RevertToParent, X.CurrentTime)
        d.sync()
        time.sleep(0.3)

        def editor_field(y, text):
            g = editor.get_geometry()
            xtest.fake_input(d, X.MotionNotify, x=g.x + 340, y=g.y + y)
            d.sync()
            button(True)
            button(False)
            key("Control_L", True)
            key("a")
            key("Control_L", False)
            for ch in text:
                key("period" if ch == "." else ch)

        editor_field(42, "azure")
        editor_field(322, "0.02")
        editor_field(350, "0.06")
        editor_field(378, "0.9")
        g = editor.get_geometry()
        raw = editor.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
        Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX").save(
            out / "08-material-editor.png"
        )
        xtest.fake_input(d, X.MotionNotify, x=g.x + 360, y=g.y + g.height - 34)
        d.sync()
        button(True)
        button(False)
        time.sleep(0.6)
        colored = shot("09-material-applied")
        blue_pixels = sum(
            b > 80 and b > r * 1.5 for r, g, b in colored.get_flattened_data()
        )
        check(
            blue_pixels > 500,
            "Material editor applies an independently blue PBR material to the selected sculpt",
        )
        key("Control_L", True)
        key("z")
        key("Control_L", False)
        time.sleep(0.5)
        uncolored = shot("10-undo-material")
        check(
            sum(b > 80 and b > r * 1.5 for r, g, b in uncolored.get_flattened_data())
            < blue_pixels / 2,
            "Material edits participate in native scene undo",
        )
        key("p")
        until = time.monotonic() + 8
        while time.monotonic() < until:
            assert app.poll() is None, (out / "app.log").read_text()
            time.sleep(0.1)
        frozen = shot("11-paint-mesh")
        key("Alt_L", True)
        key("p")
        key("Alt_L", False)
        until = time.monotonic() + 5
        brush_editor = None
        while time.monotonic() < until:
            brush_editor = next(
                (
                    w
                    for w in d.screen().root.query_tree().children
                    if "Paint brush" in str(w.get_wm_name() or "")
                ),
                None,
            )
            if brush_editor:
                break
            time.sleep(0.05)
        check(
            brush_editor is not None,
            "Texture brush settings open in a native FLTK editor",
        )
        editor = brush_editor
        editor.set_input_focus(X.RevertToParent, X.CurrentTime)
        d.sync()
        time.sleep(0.3)
        editor_field(68, "0.02")
        editor_field(96, "0.04")
        editor_field(124, "0.95")
        g = editor.get_geometry()
        raw = editor.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
        Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX").save(
            out / "12-paint-brush.png"
        )
        xtest.fake_input(d, X.MotionNotify, x=g.x + 360, y=g.y + g.height - 34)
        d.sync()
        button(True)
        button(False)
        time.sleep(0.3)
        motion(800, 400)
        key("Shift_L", True)
        button(True)
        for x in (805, 815, 825):
            motion(x, 400)
        button(False)
        key("Shift_L", False)
        time.sleep(0.8)
        inked = shot("13-texture-paint")
        check(
            sum(b > 80 and b > r * 1.5 for r, g, b in inked.get_flattened_data()) > 500,
            "Shift and trackball drag paints a blue texture patch on the prepared mesh",
        )
        key("Control_L", True)
        key("z")
        key("Control_L", False)
        time.sleep(0.8)
        restored_paint = shot("14-undo-paint")
        check(
            ImageChops.difference(frozen, restored_paint).getbbox() is None,
            "Native paint undo restores exact pre-stroke viewport pixels",
        )
        key("Alt_L", True)
        key("u")
        key("Alt_L", False)
        deadline = time.monotonic() + 30
        uv_window = None
        while time.monotonic() < deadline:
            uv_window = next((w for w in d.screen().root.query_tree().children if "UV editor" in str(w.get_wm_name() or "")), None)
            if uv_window:
                break
            time.sleep(.05)
        check(uv_window is not None, "The editable UV atlas opens as a native FLTK dialog")
        uv_window.set_input_focus(X.RevertToParent, X.CurrentTime)
        d.sync()
        def uv_shot(name):
            time.sleep(.3)
            g = uv_window.get_geometry()
            raw = uv_window.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF)
            image = Image.frombytes("RGB",(g.width,g.height),raw.data,"raw","BGRX")
            image.save(out / (name + ".png"))
            return image.crop((20,80,g.width-20,g.height-80))
        def uv_button(column):
            g = uv_window.get_geometry()
            xtest.fake_input(d,X.MotionNotify,x=g.x+int(g.width*(column+.5)/6),y=g.y+16)
            d.sync()
            button(True);button(False)
            time.sleep(.3)
        def wait_uv_image(before, name, changed):
            # Native event processing and mesh validation finish asynchronously
            # relative to XTest. Assert rendered results, not a fixed sleep.
            deadline = time.monotonic() + 30
            while True:
                assert app.poll() is None, (out / "app.log").read_text()
                image = uv_shot(name)
                differs = ImageChops.difference(before, image).getbbox() is not None
                if differs == changed or time.monotonic() >= deadline:
                    return image
        # A drag contains many native events, but must be one draft undo.
        g = uv_window.get_geometry()
        px, py = g.x + g.width // 2, g.y + g.height // 2
        xtest.fake_input(d, X.MotionNotify, x=px, y=py)
        d.sync(); button(True)
        drag_before = uv_shot("15-uv-drag-start")
        for i in range(1, 21):
            xtest.fake_input(d, X.MotionNotify, x=px+i, y=py+i//2)
            d.sync(); time.sleep(.015)
        button(False)
        drag_after = wait_uv_image(drag_before, "15-uv-dragged", True)
        check(ImageChops.difference(drag_before, drag_after).getbbox() is not None,
              "A native UV drag publishes continuous chart previews")
        uv_button(4)
        check(ImageChops.difference(drag_before, wait_uv_image(drag_before, "15-uv-drag-undo", False)).getbbox() is None,
              "One native UV undo restores a complete multi-event drag")
        # Wheel events inside the inactivity interval share an undo step.
        xtest.fake_input(d, X.MotionNotify, x=px, y=py); d.sync()
        wheel_before = uv_shot("15-uv-wheel-start")
        for _ in range(8):
            xtest.fake_input(d, X.ButtonPress, detail=5)
            xtest.fake_input(d, X.ButtonRelease, detail=5)
            d.sync(); time.sleep(.04)
        time.sleep(.4)
        check(ImageChops.difference(wheel_before, wait_uv_image(wheel_before, "15-uv-wheel", True)).getbbox() is not None,
              "A burst of native wheel samples visibly scales the UV chart")
        uv_button(4)
        check(ImageChops.difference(wheel_before, wait_uv_image(wheel_before, "15-uv-wheel-undo", False)).getbbox() is None,
              "One native UV undo restores the complete wheel burst")
        uv_before = uv_shot("15-uv-editor")
        uv_button(0)
        uv_scaled = uv_shot("16-uv-scaled")
        check(ImageChops.difference(uv_before,uv_scaled).getbbox() is not None, "A native UV scale control visibly edits the selected island")
        uv_button(4)
        uv_undo = uv_shot("17-uv-undo")
        check(ImageChops.difference(uv_before,uv_undo).getbbox() is None, "Draft UV undo restores exact chart pixels")
        uv_button(0)
        g = uv_window.get_geometry()
        xtest.fake_input(d,X.MotionNotify,x=g.x+g.width//2+100,y=g.y+g.height-16)
        d.sync();button(True);button(False)
        time.sleep(3)
        window.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync()
        applied_uv = shot("18-uv-applied")
        check(ImageChops.difference(frozen,applied_uv).getbbox() is None, "Applying UV edits retains the existing three-dimensional surface appearance")
        key("Control_L",True);key("z");key("Control_L",False)
        time.sleep(.8)
        check(ImageChops.difference(frozen,shot("19-uv-global-undo")).getbbox() is None, "A committed UV edit participates in native global undo")
        d.close()
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
