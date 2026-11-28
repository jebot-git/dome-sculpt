#!/usr/bin/env python3
"""Drive volume binding and its cell-size control through the native OpenXR client."""
import json
import signal
import subprocess
import sys
import tempfile
from pathlib import Path

from xr_test_graphics import hardware_environment, hardware_renderer

signal.pthread_sigmask(signal.SIG_SETMASK, [])
app, runtime, fixture = [Path(arg).resolve() for arg in sys.argv[1:4]]
unbound = fixture / "unbound.dsculpt"
original = unbound.read_bytes()
expected = (fixture / "bound.dsculpt").read_bytes()
report = fixture / "volume-xr.json"
saved = fixture / "volume-xr.dsculpt"
assert expected != original

with tempfile.TemporaryDirectory(prefix="dome-volume-xr-") as path:
    home = Path(path)
    runtime_spec = home / "runtime.json"
    runtime_spec.write_text(json.dumps({"file_format_version": "1.0.0", "runtime": {
        "library_path": str(runtime),
    }}))
    environment = {**hardware_environment(), "XR_RUNTIME_JSON": str(runtime_spec), "HOME": path,
                   "DOME_XR_FIXTURE_SKIN": "volume", "DOME_XR_FIXTURE_REPORT": str(report)}
    for key in ("CONFIG_HOME", "STATE_HOME", "CACHE_HOME", "DATA_HOME", "RUNTIME_DIR"):
        folder = home / key.lower()
        folder.mkdir(mode=0o700)
        environment["XDG_" + key] = str(folder)
    with (fixture / "volume-xr.log").open("w") as log:
        subprocess.run([app, "--xr", "--open", unbound, "--frames", "78", "--save", saved],
                       env=environment, timeout=95, check=True, stdout=log, stderr=subprocess.STDOUT)

metrics = json.loads(report.read_text())
assert hardware_renderer(metrics) and metrics["stereo_submissions"] > 0
assert metrics["acquires"] == metrics["image_waits"] == metrics["releases"] > 0
assert saved.read_bytes() == expected
assert unbound.read_bytes() == original
print("OpenXR skin method and cell-size selection reproduce the expected volume bind byte for byte")
