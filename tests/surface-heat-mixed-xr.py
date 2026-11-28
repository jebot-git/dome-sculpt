#!/usr/bin/env python3
"""Select a retained avatar part and a native accessory through the OpenXR loader."""
import argparse
import json
from pathlib import Path
import signal
import subprocess
import tempfile

from xr_test_graphics import hardware_environment, hardware_renderer


signal.pthread_sigmask(signal.SIG_SETMASK, [])
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("application", type=Path)
parser.add_argument("runtime", type=Path)
parser.add_argument("fixture", type=Path)
args = parser.parse_args()
scene = args.fixture / "unbound.dsculpt"
original = scene.read_bytes()
expected = (args.fixture / "bound-active-accessory.dsculpt").read_bytes()
assert expected != original
contact = json.loads((args.fixture / "accessory-contact.json").read_text())
report = args.fixture / "mixed-xr.json"
saved = args.fixture / "mixed-xr.dsculpt"

with tempfile.TemporaryDirectory(prefix="dome-xr-mixed-skin-") as directory:
    home = Path(directory)
    manifest = home / "runtime.json"
    manifest.write_text(json.dumps({"file_format_version": "1.0.0", "runtime": {
        "library_path": str(args.runtime.resolve()),
    }}))
    env = {**hardware_environment(), "XR_RUNTIME_JSON": str(manifest), "HOME": directory,
           "DOME_XR_FIXTURE_SKIN": "mixed-surface-heat", "DOME_XR_FIXTURE_REPORT": str(report.resolve()),
           "DOME_XR_FIXTURE_ACCESSORY_POINT": " ".join(map(str, contact))}
    for variable in ("CONFIG_HOME", "CACHE_HOME", "DATA_HOME", "STATE_HOME", "RUNTIME_DIR"):
        path = home / variable.lower()
        path.mkdir(mode=0o700)
        env["XDG_" + variable] = str(path)
    with (args.fixture / "mixed-xr.log").open("w") as log:
        subprocess.run([args.application, "--xr", "--open", scene, "--frames", "105", "--save", saved],
                       env=env, stdout=log, stderr=subprocess.STDOUT, timeout=95, check=True)

metrics = json.loads(report.read_text())
assert hardware_renderer(metrics) and metrics["stereo_submissions"] > 0
assert metrics["acquires"] == metrics["image_waits"] == metrics["releases"] > 0
assert saved.read_bytes() == expected, "Additive controller selection and Bind selected must reproduce both fields"
assert scene.read_bytes() == original
print("Mixed Surface Heat uses controller selection and saves both expected skin fields")
