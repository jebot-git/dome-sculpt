#!/usr/bin/env python3
"""Exercise the real loader/client with a synthetic runtime and real GL images."""

import contextlib, json, os, shutil, signal, subprocess, sys, tempfile
from pathlib import Path

signal.pthread_sigmask(signal.SIG_SETMASK, [])
binary, runtime, output = map(lambda p: Path(p).resolve(), sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
checks = []
project_directory = None


def write_report(complete=False, error=None):
    report = {
        "checks": checks,
        "failed": sum(not c["passed"] for c in checks),
        "synthetic_tracking": True,
        "complete": complete,
        "status": "failed" if error is not None or any(not c["passed"] for c in checks) else ("passed" if complete else "running"),
        "project_directory": project_directory,
    }
    if error is not None:
        report["error"] = error
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


def harness_error(kind, error, traceback):
    write_report(error=f"{kind.__name__}: {error}")
    sys.__excepthook__(kind, error, traceback)


sys.excepthook = harness_error
write_report()


def check(ok, label):
    checks.append({"passed": bool(ok), "check": label})
    print(("PASS " if ok else "FAIL ") + label, flush=True)
    write_report()
    assert ok, label


with contextlib.nullcontext(tempfile.mkdtemp(prefix="projects-", dir=output)) as directory:
    project_directory = directory
    write_report()
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
        if target.is_file():
            shutil.copy2(target, output / (name + ".dsculpt"))
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
    prefix, prefix_path = run("restart-prefix", 23)
    check(prefix.returncode == 0, "Session restart comparison captures the completed pre-stop sculpt")
    restarted, restart_path = run("restart", 45, mode={"DOME_XR_FIXTURE_RESTART": "1"})
    check(restarted.returncode == 0, "Native OpenXR resumes after STOPPING, IDLE and a new READY state")
    restart_report = json.loads((output / "restart.json").read_text())
    check(
        restart_report.get("sessions_begun") == 2
        and restart_report.get("sessions_ended") == 1
        and restart_report.get("restart_states") == 8,
        "The client ends the old running session and begins the resumed session exactly once",
    )
    restart_info = json.loads(subprocess.check_output([str(binary), "--project-info", str(restart_path)], env=env, text=True))
    prefix_info = json.loads(subprocess.check_output([str(binary), "--project-info", str(prefix_path)], env=env, text=True))
    (output / "restart-comparison.json").write_text(json.dumps({"prefix": prefix_info, "resumed": restart_info}, indent=2) + "\n")
    check(
        restart_info["field_hash"] != prefix_info["field_hash"],
        "A released and re-pressed controller sculpts again after the resumed session",
    )
    scene, scene_path = run("scene", 45, mode={"DOME_XR_FIXTURE_SCENE": "1"})
    check(
        scene.returncode == 0,
        "Headset object and material menu pages complete through native OpenXR actions",
    )
    info = json.loads(
        subprocess.check_output(
            [str(binary), "--project-info", str(scene_path)], env=env, text=True
        )
    )
    check(
        info["object_count"] == 2,
        "Joystick object page creates and persists a second independent sculpt",
    )
    check(
        info["active_color"][2] > 0.7 and info["active_color"][0] < 0.1,
        "Joystick material page updates and persists the active object's blue material",
    )
    paintbase, paintbasepath = run("paint-baseline", 1, extra=["--paint"])
    check(
        paintbase.returncode == 0,
        "Native XR painting starts from a prepared mesh with persistent UVs",
    )
    basepaint = json.loads(
        subprocess.check_output(
            [str(binary), "--project-info", str(paintbasepath)], env=env, text=True
        )
    )
    painted, paintpath = run(
        "paint", 45, extra=["--paint"], mode={"DOME_XR_FIXTURE_PAINT": "1"}
    )
    check(
        painted.returncode == 0,
        "Tracked texture painting and nested paint palette complete in native OpenXR",
    )
    info = json.loads(
        subprocess.check_output(
            [str(binary), "--project-info", str(paintpath)], env=env, text=True
        )
    )
    check(
        info["painted"]
        and info["paint_hash"] != basepaint["paint_hash"]
        and info["field_hash"] == basepaint["field_hash"],
        "XR pressure strokes change persistent texture pixels while preserving sculpt geometry",
    )
    check(
        info["paint_blue_texels"] > 20,
        "Nested joystick brush palette selects blue ink for the tracked paint stroke",
    )
    uv_result, uv_path = run("uv",45,extra=["--paint"],mode={"DOME_XR_FIXTURE_UV":"1"})
    check(uv_result.returncode==0,"Native OpenXR UV panel supports joystick entry and ray-operated edit/Apply controls")
    uv_info=json.loads(subprocess.check_output([str(binary),"--project-info",str(uv_path)],env=env,text=True))
    (output / "uv-comparison.json").write_text(json.dumps({"uv_info":uv_info,"basepaint":basepaint,"project_directory":directory},indent=2)+"\n")
    check(uv_info["uv_hash"]!=basepaint["uv_hash"],"XR UV coordinates change and persist after Apply")
    check(uv_info["field_hash"]==basepaint["field_hash"],"XR UV editing preserves the source sculpt field exactly")
    check(uv_info["paint_hash"]==basepaint["paint_hash"],"XR UV editing retains the uniform surface material appearance")
    controlled, controlpath = run(
        "controls", 45, mode={"DOME_XR_FIXTURE_CONTROLS": "1"}
    )
    check(
        controlled.returncode == 0,
        "Native OpenXR parameter controls complete with thumbstick and aim-ray input",
    )
    info = json.loads(
        subprocess.check_output(
            [str(binary), "--project-info", str(controlpath)], env=env, text=True
        )
    )
    check(
        abs(info["active_metallic"] - 0.75) < 0.002
        and info["field_hash"] == baseinfo["field_hash"],
        "Controller ray dragging commits the material slider while preserving geometry",
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
write_report(complete=True)
