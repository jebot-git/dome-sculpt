#!/usr/bin/env python3
"""Open a generated scene-v5 fixture in real FLTK; paint/undo and cancel UVs."""
import json, os, signal, subprocess, sys, tempfile, time
from pathlib import Path
from Xlib import X, XK, display, error
from Xlib.ext import xtest
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK, [])
if os.environ.get("DOME_BAKED_GUI_BUS") != "1":
    os.execvpe("dbus-run-session", ["dbus-run-session", "--", sys.executable, *sys.argv],
              {**os.environ, "DOME_BAKED_GUI_BUS": "1"})
root = Path(__file__).resolve().parents[1]
out = root / "artifacts/gui-baked-surface"
out.mkdir(parents=True, exist_ok=True)
checks, processes = [], []
def write_report(complete=False, error=None):
    failed=sum(not c["passed"] for c in checks)
    report={"checks":checks,"failed":failed,"complete":complete,
            "status":"failed" if error or failed else "passed" if complete else "running"}
    if error: report["error"]=error
    (out / "report.json").write_text(json.dumps(report,indent=2)+"\n")
def failure(kind,value,traceback):
    write_report(error=f"{kind.__name__}: {value}")
    sys.__excepthook__(kind,value,traceback)
sys.excepthook=failure
write_report()
project = Path(sys.argv[1]).resolve()
assert project.is_file(), "Generate the native bake-workflow --artifacts fixture first"
def check(ok, label):
    checks.append({"check": label, "passed": bool(ok)})
    print(("PASS " if ok else "FAIL ") + label, flush=True)
    write_report()
    assert ok, label
def wait(condition):
    until = time.monotonic()+90
    while time.monotonic()<until:
        result = condition()
        if result: return result
        time.sleep(.05)
    raise AssertionError("Native baked-surface UI state timed out")
try:
    with tempfile.TemporaryDirectory(prefix="dome-baked-gui-") as directory:
        rd, wr = os.pipe()
        with (out / "xvfb.log").open("w") as log:
            server = subprocess.Popen(["Xvfb", "-displayfd", str(wr), "-screen", "0", "1280x900x24", "-nolisten", "tcp"], pass_fds=(wr,), stdout=log, stderr=log)
        processes.append(server); os.close(wr)
        with os.fdopen(rd) as stream: screen = ":"+stream.readline().strip()
        env = {**os.environ, "HOME": directory, "DISPLAY": screen, "XDG_RUNTIME_DIR": directory,
               "XDG_CONFIG_HOME": directory+"/config", "XDG_CACHE_HOME": directory+"/cache",
               "XDG_DATA_HOME": directory+"/data", "XDG_STATE_HOME": directory+"/state",
               "FLTK_BACKEND": "x11", "LIBGL_ALWAYS_SOFTWARE": "1", "XDG_CURRENT_DESKTOP": "Dome",
               "XMODIFIERS": "@im=none"}
        for name in ("WAYLAND_DISPLAY", "WAYLAND_SOCKET"): env.pop(name, None)
        with (out / "app.log").open("w") as log:
            app = subprocess.Popen([str(root / "build/dome-sculpt"), str(project)], env=env, stdout=log, stderr=log)
        processes.append(app)
        d = display.Display(screen)
        def named(text):
            for w in d.screen().root.query_tree().children:
                try:
                    if text in str(w.get_wm_name() or ""): return w
                except error.BadWindow: pass
            return None
        window = wait(lambda: named(project.name))
        window.set_input_focus(X.RevertToParent, X.CurrentTime); d.sync()
        def key(name, down=None):
            for kind in ([X.KeyPress, X.KeyRelease] if down is None else [X.KeyPress if down else X.KeyRelease]):
                xtest.fake_input(d, kind, d.keysym_to_keycode(XK.string_to_keysym(name))); d.sync(); time.sleep(.05)
        def shot():
            g = window.get_geometry()
            raw = window.get_image(0,0,g.width,g.height,X.ZPixmap,0xFFFFFFFF)
            return Image.frombytes("RGB",(g.width,g.height),raw.data,"raw","BGRX").crop((80,100,g.width-80,g.height-80))
        def blue(image):
            raw=image.tobytes()
            return sum(b>90 and b>r*1.5 and b>g*1.2 for r,g,b in zip(raw[0::3],raw[1::3],raw[2::3]))
        wait(lambda: blue(shot())>1000)
        time.sleep(.3); baseline = shot(); baseline.save(out / "baked-baseline.png")
        check(blue(baseline)>1000, "FLTK opens a scene-v5 baked sculpt with visible transferred surface paint")
        g = window.get_geometry()
        key("Shift_L",True)
        xtest.fake_input(d,X.MotionNotify,x=g.x+550,y=g.y+365); d.sync(); time.sleep(.15)
        xtest.fake_input(d,X.ButtonPress,1); d.sync(); time.sleep(.15)
        for x in (565,580,595):
            xtest.fake_input(d,X.MotionNotify,x=g.x+x,y=g.y+365); d.sync(); time.sleep(.15)
        xtest.fake_input(d,X.ButtonRelease,1); d.sync(); key("Shift_L",False)
        wait(lambda: shot().tobytes()!=baseline.tobytes())
        painted=shot(); painted.save(out / "baked-painted.png")
        def red(image):
            raw=image.tobytes()
            return sum(r>80 and r>g*1.5 and r>b*1.5 for r,g,b in zip(raw[0::3],raw[1::3],raw[2::3]))
        check(red(painted)>red(baseline)+100, "Native strokes paint over the baked base without replacing its normal/AO data")
        key("Control_L",True);key("z");key("Control_L",False)
        wait(lambda: shot().tobytes()==baseline.tobytes())
        check(shot().tobytes()==baseline.tobytes(), "Native Undo restores exact baked-surface GPU pixels")
        key("Alt_L",True);key("u");key("Alt_L",False)
        editor=wait(lambda:named("UV editor"))
        check(editor.get_wm_transient_for().id==window.id, "Baked assets retain native UV modal ownership")
        editor.set_input_focus(X.RevertToParent,X.CurrentTime);d.sync();key("Escape")
        wait(lambda:named("UV editor") is None)
        wait(lambda:shot().tobytes()==baseline.tobytes())
        check(app.poll() is None and shot().tobytes()==baseline.tobytes(), "Cancelling the native UV draft preserves the baked asset and normal-mapped preview")
        d.close()
finally:
    for p in reversed(processes):
        if p.poll() is None: p.terminate()
    for p in reversed(processes):
        try: p.wait(timeout=5)
        except subprocess.TimeoutExpired: p.kill();p.wait()
write_report(complete=True)
