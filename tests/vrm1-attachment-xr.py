#!/usr/bin/env python3
"""Exercise actual OpenXR attachment authoring with synthetic controllers."""
import argparse
import json
from pathlib import Path
from xr_test_graphics import hardware_environment, hardware_renderer
import signal
import subprocess
import tempfile

signal.pthread_sigmask(signal.SIG_SETMASK, [])
parser = argparse.ArgumentParser(description=__doc__)
for name in ("application", "runtime", "fixtures", "output"):
    parser.add_argument(name, type=lambda value: Path(value).resolve())
parser.add_argument("--renderer", help="Require this substring in the hardware renderer name")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
results = []


def check(condition, description):
    results.append({"passed": bool(condition), "check": description})
    (args.output / "report.json").write_text(json.dumps({
        "physical_headset": False, "checks": results,
    }, indent=2) + "\n")
    print(("PASS " if condition else "FAIL ") + description, flush=True)
    if not condition:
        raise AssertionError(description)


source = args.fixtures / "xr-before-attach.dsculpt"
original = source.read_bytes()
attached = (args.fixtures / "xr-attached.dsculpt").read_bytes()
assert original != attached
with tempfile.TemporaryDirectory(prefix="dome-xr-attachment-") as directory:
    home = Path(directory)
    manifest = home / "runtime.json"
    manifest.write_text(json.dumps({"file_format_version": "1.0.0", "runtime": {
        "library_path": str(args.runtime),
    }}))
    env = {**hardware_environment(), "XR_RUNTIME_JSON": str(manifest), "LP_NUM_THREADS": "2", "HOME": directory}

    for name in ("config", "data", "state", "cache", "runtime"):
        target = home / name
        target.mkdir(mode=0o700)
        suffix = "DIR" if name == "runtime" else "HOME"
        env[f"XDG_{name.upper()}_{suffix}"] = str(target)
    for mode in ("accept", "cancel", "focus", "tracking"):
        report = args.output / f"{mode}.json"
        target = args.output / f"{mode}.dsculpt"
        with (args.output / f"{mode}.log").open("w") as log:
            result = subprocess.run([
                str(args.application), "--xr", "--open", str(source), "--frames", "90", "--save", str(target),
            ], stdout=log, stderr=subprocess.STDOUT,
                env={**env, "DOME_XR_FIXTURE_ATTACHMENT": mode, "DOME_XR_FIXTURE_REPORT": str(report)}, timeout=75)
        check(result.returncode == 0, f"{mode}: OpenXR client exits and drains its Save")
        metrics = json.loads(report.read_text())
        check(hardware_renderer(metrics, args.renderer) and
              metrics["acquires"] == metrics["image_waits"] == metrics["releases"] > 0,
              f"{mode}: requested renderer and balanced OpenXR swapchains")
        pixels = metrics["attachment_palette_pixels"]
        check(all(pixels[step][eye] > pixels["58"][eye] + 200
                  for step in ("34", "46") for eye in ("0", "1")),
              f"{mode}: both eyes render the picker, which closes after the final action")
        check(target.read_bytes() == (attached if mode == "accept" else original) and source.read_bytes() == original,
              f"{mode}: saved scene matches the exact expected attachment and paint state")
        if mode == "focus":
            check(metrics.get("focus_losses") == metrics.get("focus_regains") == 1,
                  "Actual loader events cause the focus loss and regain")
        if mode == "tracking":
            check(metrics.get("attachment_tracking_loss") and metrics.get("tracking_losses", 0) > 0,
                  "The loader reports tracking loss for the attachment controller")

    for mode, fixture in (("transfer", "skin"), ("texture-set-heat", "heat")):
        source = args.fixtures / f"{fixture}-before.dsculpt"
        original = source.read_bytes()
        bound = (args.fixtures / f"{fixture}-bound.dsculpt").read_bytes()
        assert original != bound
        report = args.output / f"skin-{mode}.json"
        target = args.output / f"skin-{mode}.dsculpt"
        with (args.output / f"skin-{mode}.log").open("w") as log:
            result = subprocess.run([
                str(args.application), "--xr", "--open", str(source), "--frames", "90", "--save", str(target),
            ], stdout=log, stderr=subprocess.STDOUT,
                env={**env, "DOME_XR_FIXTURE_SKIN": mode, "DOME_XR_FIXTURE_REPORT": str(report)}, timeout=75)
        check(result.returncode == 0, f"{mode}: OpenXR client exits and drains its Save")
        metrics = json.loads(report.read_text())
        check(hardware_renderer(metrics, args.renderer) and metrics["stereo_submissions"] > 0 and
              metrics["acquires"] == metrics["image_waits"] == metrics["releases"] > 0,
              f"{mode}: requested renderer and balanced OpenXR stereo swapchains")
        check(target.read_bytes() == bound and source.read_bytes() == original,
              f"{mode}: controller binds to the original rig and retains the exact native project")
print(f"{len(results)} actual-loader attachment checks passed", flush=True)
