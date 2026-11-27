#!/usr/bin/env python3
"""Exercise affine weight painting through the OpenXR loader and controller."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile

signal.pthread_sigmask(signal.SIG_SETMASK, [])
parser = argparse.ArgumentParser(description=__doc__)
for argument in ("application", "runtime", "fixture", "output"):
    parser.add_argument(argument, type=lambda value: Path(value).resolve())
parser.add_argument("--renderer", default="llvmpipe")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
fixtures = args.output / "fixtures"
subprocess.run([args.fixture, "--affine-xr-fixture", fixtures], check=True, timeout=45)
before = fixtures / "before.dsculpt"
after = fixtures / "after.dsculpt"
assert before.read_bytes() != after.read_bytes()
results = []


def check(value, label):
    if not value:
        raise AssertionError(label)
    results.append(label)
    print("PASS", label, flush=True)


with tempfile.TemporaryDirectory(prefix="dome-affine-weights-") as directory:
    home = Path(directory)
    manifest = home / "runtime.json"
    manifest.write_text(json.dumps({"file_format_version": "1.0.0", "runtime": {
        "library_path": str(args.runtime)}}))
    env = {**os.environ, "HOME": directory, "XR_RUNTIME_JSON": str(manifest), "LP_NUM_THREADS": "2"}
    if args.renderer == "llvmpipe":
        env["LIBGL_ALWAYS_SOFTWARE"] = "1"
    for name in ("config", "state", "data", "cache", "runtime"):
        target = home / name
        target.mkdir(mode=0o700)
        suffix = "DIR" if name == "runtime" else "HOME"
        env[f"XDG_{name.upper()}_{suffix}"] = str(target)

    for mode, expected in (("accept", after), ("focus", before)):
        report = args.output / f"{mode}.json"
        result = args.output / f"{mode}.dsculpt"
        with (args.output / f"{mode}.log").open("w") as log:
            subprocess.run([args.application, "--xr", "--open", before, "--frames", "75", "--save", result],
                           env={**env, "DOME_XR_FIXTURE_AFFINE_WEIGHT": mode,
                                "DOME_XR_FIXTURE_REPORT": str(report)},
                           stdout=log, stderr=subprocess.STDOUT, timeout=120, check=True)
        evidence = json.loads(report.read_text())
        check(args.renderer.casefold() in evidence["gl_renderer"].casefold() and
              evidence["acquires"] == evidence["image_waits"] == evidence["releases"] > 0 and
              evidence["stereo_submissions"] > 0,
              f"{mode}: the selected renderer submits both eyes and drains the worker Save")
        check(result.read_bytes() == expected.read_bytes(),
              f"{mode}: saved affine weight gesture matches the expected complete native project")
        check(evidence.get("reference_changes") == 1,
              f"{mode}: the loader rebases the scene before controller contact")
        if mode == "focus":
            check(evidence.get("focus_losses") == evidence.get("focus_regains") == 1,
                  "focus: the loader actually delivers loss and regain during the attempted gesture")

(args.output / "report.json").write_text(json.dumps({"physical_headset": False, "checks": results}, indent=2) + "\n")
