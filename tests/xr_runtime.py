#!/usr/bin/env python3
"""Exercise the real loader/client with a synthetic runtime and real GL images."""

import contextlib, json, os, shutil, signal, subprocess, sys, tempfile, time
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
    committed, committed_path = run("signal-committed", 7)
    check(committed.returncode == 0, "Signal baseline completes a controller stroke before the next gesture")
    committed_info = json.loads(subprocess.check_output(
        [str(binary), "--project-info", str(committed_path)], env=env, text=True))
    check(committed_info["field_hash"] != baseinfo["field_hash"],
          "Signal baseline independently retains a real accepted controller edit")
    for stop_signal in (signal.SIGINT, signal.SIGTERM):
        name = "signal-" + stop_signal.name.lower()
        marker = root / (name + ".ready")
        target = root / (name + ".dsculpt")
        report_path = output / (name + ".json")
        e = {**env, "DOME_XR_FIXTURE_REPORT": str(report_path),
             "DOME_XR_FIXTURE_SIGNAL_READY": str(marker)}
        with (output / (name + ".log")).open("w") as log:
            process = subprocess.Popen([str(binary), "--xr", "--save", str(target)],
                                       env=e, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 40
                while not marker.is_file() and process.poll() is None and time.monotonic() < deadline:
                    time.sleep(.005)
                check(marker.is_file() and process.poll() is None,
                      name + " reaches a rendered controller stroke before interruption")
                process.send_signal(stop_signal)
                process.wait(timeout=40)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)
        check(process.returncode == 0 and target.is_file(),
              name + " exits normally and writes the requested final native save")
        report = json.loads(report_path.read_text())
        check(report["frames_waited"] == report["frames_begun"] == report["frames_ended"]
              and report["acquires"] == report["image_waits"] == report["releases"]
              and report["swapchains_destroyed"] == 2,
              name + " balances the interrupted frame and releases both eye swapchains")
        info = json.loads(subprocess.check_output([str(binary), "--project-info", str(target)],
                                                 env=env, text=True))
        check(info["field_hash"] == committed_info["field_hash"],
              name + " retains the completed stroke and cancels the unfinished gesture before final saving")
        shutil.copy2(target, output / target.name)
    for name, mode in [
        ("controller", None),
        ("stylus", {"DOME_XR_FIXTURE_STYLUS": "1"}),
        ("controller-srgb", {"DOME_XR_FIXTURE_SRGB": "1"}),
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
        check(report["swapchain_format"] == (35907 if name == "controller-srgb" else 32856),
              name + " uses the explicit sRGB or linear OpenXR swapchain transport")
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
    layers_result,layers_path=run("sculpt-layers",45,mode={"DOME_XR_FIXTURE_LAYERS":"1"})
    check(layers_result.returncode==0,"Native OpenXR sculpt-layer commands complete and save")
    layers_info=json.loads(subprocess.check_output([str(binary),"--project-info",str(layers_path)],env=env,text=True))
    check(len(layers_info["sculpt_layers"])==2 and layers_info["active_sculpt_layer"]==layers_info["sculpt_layers"][1]["id"],"Headset object/layer pages create and select a persistent sculpt layer")
    check(layers_info["sculpt_layers"][1]["samples"]>0 and layers_info["field_hash"]!=baseinfo["field_hash"],"Tracked pressure strokes populate the new sculpt layer after menu capture ends")
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
    for mode in ("accept", "cancel", "focus", "undo"):
        result, path = run("voxel-" + mode, 50 if mode == "undo" else 45,
                           mode={"DOME_XR_FIXTURE_VOXEL": mode})
        check(result.returncode == 0,
              "Native OpenXR voxel " + mode + " finishes with a saved scene")
        info = json.loads(subprocess.check_output(
            [str(binary), "--project-info", str(path)], env=env, text=True))
        if mode == "focus":
            report = json.loads((output / "voxel-focus.json").read_text())
            check(report.get("focus_losses") == 1 and
                  report.get("tracking_losses", 0) == 0,
                  "The voxel focus fixture reaches session focus loss without earlier tracking cancellation")
        if mode == "accept":
            check(info["object_count"] == 2 and not info["painted"] and
                  abs(info["voxel_size"] - .05) < 1e-6 and info["samples"] > 0,
                  "Joystick Mesh tools and aim slider create a sculpt at exactly 50 mm")
            check(info["objects"][0]["visible"] is False and
                  info["objects"][0]["payload_hash"] == baseinfo["objects"][0]["payload_hash"] and
                  info["objects"][1]["visible"] is True,
                  "Native OpenXR remeshing retains the complete source and selects a visible result")
        else:
            check(info["scene_hash"] == baseinfo["scene_hash"],
                  "Native OpenXR voxel " + mode + " restores the exact initial scene")
    bake_fixture=root / "bake-input.dsculpt"
    subprocess.run([str(binary.parent / "dome-sculpt-bake-controls-test"),"--ui-fixture",str(bake_fixture)],check=True)
    bake_base=json.loads(subprocess.check_output([str(binary),"--project-info",str(bake_fixture)],env=env,text=True))
    for mode in ("accept","cancel","focus","undo"):
        result,path=run("bake-"+mode,50 if mode=="undo" else 45,extra=["--open",str(bake_fixture)],mode={"DOME_XR_FIXTURE_BAKE":mode})
        check(result.returncode==0,"Native OpenXR bake "+mode+" completes with a saved scene")
        info=json.loads(subprocess.check_output([str(binary),"--project-info",str(path)],env=env,text=True))
        if mode=="accept":
            check(info["objects"][0]["baked"] and info["paint_blue_texels"]>1000,
                  "Joystick baking page and world-distance aim slider publish visible baked source paint")
            check(info["objects"][1:]==bake_base["objects"][1:],"Native OpenXR baking retains both complete hidden sources")
        else:
            check(info["scene_hash"]==bake_base["scene_hash"],"Native OpenXR bake "+mode+" retains or restores the exact pre-bake scene")

    skin_helper=binary.parent / "dome-sculpt-skin-authoring-test"
    for mode in ("transfer","fitted","focus"):
        skin_fixture=root / ("skin-"+mode+"-input.dsculpt")
        subprocess.run([str(skin_helper),"--ui-unbound-fixture" if mode=="fitted" else "--ui-fixture",str(skin_fixture)],check=True)
        before=json.loads(subprocess.check_output([str(skin_helper),"--inspect",str(skin_fixture)],env=env,text=True))
        result,path=run("skin-"+mode,45,extra=["--open",str(skin_fixture)],mode={"DOME_XR_FIXTURE_SKIN":mode})
        check(result.returncode==0,"Native OpenXR skin "+mode+" completes and drains accepted authoring before saving")
        after=json.loads(subprocess.check_output([str(skin_helper),"--inspect",str(path)],env=env,text=True))
        if mode=="focus":
            check(after==before,"Actual OpenXR focus loss cancels the staged skin method without binding or modifying native data")
            report=json.loads((output / "skin-focus.json").read_text())
            check(report.get("focus_losses")==1 and report.get("tracking_losses",0)==0,"Skin focus fixture reaches focus loss without an earlier controller tracking cancellation")
        else:
            check(after["skinned"],"Actual OpenXR joystick and ray controls publish persistent "+mode+" skin weights")
            target=0 if mode=="fitted" else 1
            original_target=dict(before["scene"]["objects"][target]);new_target=dict(after["scene"]["objects"][target])
            old_deformation=original_target.pop("deformation",None);deformation=new_target.pop("deformation")
            check(original_target==new_target,"OpenXR skin authoring retains exact target geometry, paint and source properties")
            other=1-target
            check(before["scene"]["objects"][other]==after["scene"]["objects"][other],"OpenXR skin authoring leaves the complete reference/other object untouched")
            if mode=="transfer":
                source=before["scene"]["objects"][0]["deformation"]
                check([dict(row) for row in deformation["skin"]["vertices"]]==[dict(row) for row in source["skin"]["vertices"]],"OpenXR method selector transfers independently known reference weights exactly")
            else:
                check(all(deformation.get(key)==value for key,value in old_deformation.items() if key!="skin"),"OpenXR binds a previously unweighted fitted skeleton while preserving exact pose, morphs and source identities")

    # Real loader/controller retopology: inspect native files through the
    # existing generic Scene output in the skin helper, not a product test API.
    retopo_fixture = root / "retopo-input.dsculpt"
    subprocess.run([str(binary.parent / "dome-sculpt-retopo-authoring-test"),
                    "--ui-fixture", str(retopo_fixture), "--retain-sculpt"], check=True)
    def inspect_scene(path):
        scene = json.loads(subprocess.check_output(
            [str(skin_helper), "--inspect", str(path)], env=env, text=True))["scene"]
        # Lattice maps are unordered storage. Compare every key/value exactly,
        # independent of rehash order across separate native load/save processes.
        for obj in scene["objects"]:
            volume = obj["volume"]
            volume["samples"].sort()
            for layer in volume.get("sculpt_layers", []):
                layer["delta"].sort()
        return scene
    retopo_original = inspect_scene(retopo_fixture)
    prefix, prefix_path = run("retopo-committed", 14,
                              extra=["--open", str(retopo_fixture)],
                              mode={"DOME_XR_FIXTURE_RETOPO": "committed"})
    check(prefix.returncode == 0, "Actual OpenXR joystick commits a scene edit before opening retopology")
    retopo_committed = inspect_scene(prefix_path)
    source = retopo_committed["objects"][0]
    check(source["material"]["color"][2] > .7 and
          source["material"]["color"][0] < .1 and
          source["volume"] == retopo_original["objects"][0]["volume"] and
          source["painted"] == retopo_original["objects"][0]["painted"],
          "Committed OpenXR prefix changes the material while retaining the original sculpt and prepared source")
    for mode in ("accept", "focus", "unconfirmed"):
        result, path = run("retopo-" + mode, 110,
                           extra=["--open", str(retopo_fixture)],
                           mode={"DOME_XR_FIXTURE_RETOPO": mode})
        check(result.returncode == 0,
              "Real-loader retopology " + mode + " drains or cancels its workspace and writes the requested native save")
        after = inspect_scene(path)
        report = json.loads((output / ("retopo-" + mode + ".json")).read_text())
        check(report.get("retopo_source_ready") is True and
              report.get("retopo_script_step", 0) >= 63 and
              report.get("tracking_losses", 0) == 0,
              "Retopology " + mode + " reaches enabled Resume on menu 16 without a tracking-loss shortcut")
        check(report["frames_waited"] == report["frames_begun"] == report["frames_ended"] and
              report["acquires"] == report["image_waits"] == report["releases"] and
              report["swapchains_destroyed"] == 2,
              "Retopology " + mode + " balances real frame, stereo-image and shutdown ownership")
        if mode == "unconfirmed":
            check(after == retopo_committed,
                  "Standalone --save cancels an unconfirmed retopology draft and preserves the exact earlier committed scene")
            check(report.get("retopo_pressure_samples", 0) >= 3,
                  "Unconfirmed retopology shutdown follows actual pressure placement rather than an empty workspace")
            continue
        check(report.get("retopo_apply_ready") is True and len(after["objects"]) == 2 and
              after["objects"][0]["visible"] is False and after["objects"][1]["visible"] is True,
              "Enabled retopology Face and Apply controls create a visible native patch and retain its hidden reference")
        before_source = dict(source)
        retained = dict(after["objects"][0])
        for item in (before_source, retained):
            item.pop("visible", None)
            # The combined scene8 schema explicitly adds null optional slots.
            item.setdefault("material_bindings", None)
            item.setdefault("retopology", None)
        check(retained == before_source,
              "Actual retopology Apply preserves the exact source sculpt, paint, material, transform and identities")
        patch = after["objects"][1]["retopology"]
        topology = json.loads(patch["topology"])
        positions = sorted(vertex[1] for vertex in topology["vertices"])
        expected = sorted([[-.2, -.2, 0], [.2, -.2, 0], [0, .2, 0]])
        check(patch["source"] == source["id"] and len(topology["faces"]) == 1 and
              len(positions) == 3 and all(abs(a-b) < 1e-5 for p,q in zip(positions, expected) for a,b in zip(p,q)),
              "Controller pressure projects independently expected triangle corners onto the original reference plane")
        if mode == "focus":
            check(report.get("focus_losses") == 1 and report.get("focus_regains") == 1,
                  "Retopology focus fixture crosses actual OpenXR focus loss and regain")
            check(all(abs(p[0]-.35) > .01 or abs(p[1]-.25) > .01 for p in positions),
                  "Focus loss cancels the held point while retaining the earlier draft point for later Face and Apply")

    # Cut waits are driven by the actually enabled Apply sector in GL images,
    # not a product-side shortcut or a fixed geometry-worker sleep.
    cut_helper = binary.parent / "dome-sculpt-cut-authoring-test"
    cut_fixture = root / "cut-input.dsculpt"
    subprocess.run([str(cut_helper), "--ui-fixture", str(cut_fixture)], check=True)
    def inspect_cut(path):
        return json.loads(subprocess.check_output([str(cut_helper), "--inspect", str(path)], env=env, text=True))
    cut_original = inspect_cut(cut_fixture)
    for mode in ("accept", "unconfirmed"):
        result, path = run("cut-"+mode, 80, extra=["--open", str(cut_fixture)],
                           mode={"DOME_XR_FIXTURE_CUT": mode})
        check(result.returncode == 0, "Actual OpenXR plane cut "+mode+" exits and saves through the native standalone path")
        report = json.loads((output / ("cut-"+mode+".json")).read_text())
        check(report.get("cut_preview_ready") is True and report.get("cut_slider_samples", 0) > 0 and
              report.get("cut_script_step", 0) >= 33 and report.get("tracking_losses", 0) == 0,
              "Plane cut "+mode+" reaches an enabled page17 preview and adjusts its real ray slider without losing tracking")
        check(report["frames_waited"] == report["frames_begun"] == report["frames_ended"] and
              report["acquires"] == report["image_waits"] == report["releases"] and report["swapchains_destroyed"] == 2,
              "Plane cut "+mode+" balances stereo image, frame and shutdown ownership")
        after = inspect_cut(path)
        if mode == "unconfirmed":
            check(after == cut_original,
                  "Standalone save cancels an unconfirmed live cut adjustment and preserves the exact original scene")
            continue
        objects = after["scene"]["objects"]
        check(report.get("cut_apply_ready") is True and len(objects) == 3 and not objects[0]["visible"] and
              all(obj["visible"] for obj in objects[1:]) and objects[1]["group"] == objects[2]["group"] != 0,
              "Controller-only plane Apply publishes separate shell and cap in one persistent group")
        retained = dict(objects[0]); before = dict(cut_original["scene"]["objects"][0])
        for item in (retained, before):
            item.pop("visible", None)
            item.setdefault("material_bindings", None)
            item.setdefault("retopology", None)
        check(retained == before,
              "Actual OpenXR cut retains the exact original paint, geometry, source identities and material")
        expected_offset = .2 * (3 ** .5) * .2
        check(after["closed"] and abs(after["x_min"]-expected_offset) < 1e-5 and abs(after["x_max"]-.2) < 1e-5,
              "Saved cut geometry is closed and matches the independently expected 60-percent ray-slider half-space")

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
