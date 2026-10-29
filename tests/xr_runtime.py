#!/usr/bin/env python3
"""Exercise the real loader/client with a synthetic runtime and real GL images."""

import json, os, signal, subprocess, sys, tempfile
from pathlib import Path

signal.pthread_sigmask(signal.SIG_SETMASK, [])
binary, runtime, output = map(lambda p: Path(p).resolve(), sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
checks = []


def check(ok, label):
    checks.append({"passed": bool(ok), "check": label})
    print(("PASS " if ok else "FAIL ") + label, flush=True)
    assert ok, label


with tempfile.TemporaryDirectory(prefix="dome-xr-client-") as directory:
    root = Path(directory)
    manifest = root / "runtime.json"
    manifest.write_text(
        json.dumps(
            {"file_format_version": "1.0.0", "runtime": {"library_path": str(runtime)}}
        )
    )
    env = {
        **os.environ,
        "XR_RUNTIME_JSON": str(manifest),
        "LIBGL_ALWAYS_SOFTWARE": "1",
        "HOME": directory,
        "XDG_RUNTIME_DIR": directory,
        "XDG_CONFIG_HOME": directory + "/config",
        "XDG_STATE_HOME": directory + "/state",
    }

    def run(name, frames, extra=(), mode=None):
        e = {**env, "DOME_XR_FIXTURE_REPORT": str(output / (name + ".json"))}
        if mode:
            e.update(mode)
        target = root / (name + ".dsculpt")
        with (output / (name + ".log")).open("w") as log:
            p = subprocess.run(
                [
                    str(binary),
                    "--xr",
                    "--frames",
                    str(frames),
                    "--save",
                    str(target),
                    *extra,
                ],
                env=e,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=40,
            )
        return p, target

    baseline, base = run("baseline", 1)
    check(
        baseline.returncode == 0,
        "Native XR client initializes EGL/OpenXR through the real loader",
    )
    baseinfo = json.loads(
        subprocess.check_output(
            [str(binary), "--project-info", str(base)], env=env, text=True
        )
    )
    for name, mode in [
        ("controller", None),
        ("stylus", {"DOME_XR_FIXTURE_STYLUS": "1"}),
    ]:
        result, path = run(name, 45, mode=mode)
        check(
            result.returncode == 0,
            name + " completes tracked editing and saves a native project",
        )
        report = json.loads((output / (name + ".json")).read_text())
        info = json.loads(
            subprocess.check_output(
                [str(binary), "--project-info", str(path)], env=env, text=True
            )
        )
        check(
            report["frames_waited"]
            == report["frames_begun"]
            == report["frames_ended"]
            == 45,
            name + " balances native OpenXR frame calls",
        )
        check(
            report["acquires"] == report["image_waits"] == report["releases"]
            and report["swapchains_destroyed"] == 2,
            name + " balances swapchain acquisition, waits, releases and cleanup",
        )
        check(
            report["stereo_submissions"] > 20
            and report["left_eye_nonblank"] > 20
            and report["right_eye_nonblank"] > 20,
            name + " submits real rendered images for both tracked eyes",
        )
        check(
            report.get("empty_submissions", 0) > 0
            and report.get("focus_losses", 0) == 1
            and report.get("tracking_losses", 0) > 0,
            name + " handles skipped rendering, focus loss and invalid tracking",
        )
        check(
            info["field_hash"] != baseinfo["field_hash"],
            name + " pressure and pose input changes the saved sculpt volume",
        )
        check(
            report.get("reference_changes", 0) == 1,
            name
            + " schedules a native reference-space rebase at the runtime change time",
        )
        check(
            report.get("palette_images", 0) > 0,
            name + " joystick menu is rasterized into submitted eye images",
        )
        if name == "stylus":
            bindings = report["bindings"][
                "/interaction_profiles/logitech/mx_ink_stylus_logitech"
            ]
            check(
                "/user/hand/right/input/tip_logitech/pose" in bindings["grip_pose"]
                and "/user/hand/right/input/tip_logitech/force" in bindings["tip_force"]
                and report.get("tip_pressure_samples", 0) > 0,
                "MX Ink uses calibrated tip pose and actual tip-force action paths",
            )
    files, _ = run("files", 45, mode={"DOME_XR_FIXTURE_FILES": "1"})
    check(
        files.returncode == 0,
        "Joystick file commands complete without running I/O in the XR frame thread",
    )
    saved = list((root / "state/dome-sculpt").glob("*.dsculpt"))
    exported = list((root / "state/dome-sculpt").glob("*.glb"))
    check(
        bool(saved) and saved[0].stat().st_size > 100,
        "Joystick radial Save writes an untitled native recovery project",
    )
    check(
        bool(exported) and exported[0].read_bytes()[:4] == b"glTF",
        "Joystick radial Export writes a glTF mesh through the background file worker",
    )
    rejected, _ = run("no-overlay", 1, mode={"DOME_XR_FIXTURE_NO_OVERLAY": "1"})
    check(
        rejected.returncode != 0,
        "Unavailable overlay support does not silently create a primary XR session",
    )
    explicit, _ = run(
        "exclusive", 2, extra=["--exclusive"], mode={"DOME_XR_FIXTURE_NO_OVERLAY": "1"}
    )
    check(
        explicit.returncode == 0,
        "Primary XR sessions require the explicit exclusive option",
    )
(output / "report.json").write_text(
    json.dumps(
        {
            "checks": checks,
            "failed": sum(not c["passed"] for c in checks),
            "synthetic_tracking": True,
        },
        indent=2,
    )
    + "\n"
)
