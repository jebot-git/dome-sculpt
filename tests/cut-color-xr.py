#!/usr/bin/env python3
"""Actual OpenXR loader/GL plane-cut workflow with current corner authority."""
import json,os,signal,subprocess,sys,tempfile
from pathlib import Path
signal.pthread_sigmask(signal.SIG_SETMASK,[])
binary,runtime,out=map(lambda s:Path(s).resolve(),sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
checks=[]
def check(ok,label):
    checks.append({"passed":bool(ok),"check":label})
    (out/"report.json").write_text(json.dumps({"checks":checks,"failed":sum(not c["passed"] for c in checks),"synthetic_tracking":True},indent=2))
    print(("PASS " if ok else "FAIL ")+label,flush=True)
    assert ok,label
with tempfile.TemporaryDirectory(prefix="projects-",dir=out) as temporary:
    root=Path(temporary);manifest=root/"runtime.json"
    manifest.write_text(json.dumps({"file_format_version":"1.0.0","runtime":{"library_path":str(runtime)}}))
    env={**os.environ,"XR_RUNTIME_JSON":str(manifest),"LIBGL_ALWAYS_SOFTWARE":"1","LP_NUM_THREADS":"2","HOME":str(root),"XDG_RUNTIME_DIR":str(root),"XDG_CONFIG_HOME":str(root/"config"),"XDG_STATE_HOME":str(root/"state")}
    fixture=root/"input.dsculpt"
    subprocess.run([str(binary.parent/"dome-sculpt-cut-color-test"),"--ui-fixture",str(fixture)],env=env,check=True)
    def inspect(path):return json.loads(subprocess.check_output([str(binary.parent/"dome-sculpt-cut-authoring-test"),"--inspect",str(path)],env=env,text=True))
    original=inspect(fixture)
    for mode in ("accept","unconfirmed"):
        target=out/(mode+".dsculpt");report=out/(mode+".json")
        with (out/(mode+".log")).open("w") as log:
            result=subprocess.run([str(binary),"--xr","--frames","80","--save",str(target),"--open",str(fixture)],env={**env,"DOME_XR_FIXTURE_REPORT":str(report),"DOME_XR_FIXTURE_CUT":mode},stdout=log,stderr=subprocess.STDOUT,timeout=60)
        check(result.returncode==0,"Real-loader current-color "+mode+" exits through the actual standalone save path")
        activity=json.loads(report.read_text())
        check(activity.get("cut_preview_ready") is True and activity.get("cut_slider_samples",0)>0 and activity.get("cut_script_step",0)>=33 and activity.get("tracking_losses",0)==0,"Actual page17 joystick/large ray slider author a colored cut with valid tracking")
        check(activity["frames_waited"]==activity["frames_begun"]==activity["frames_ended"] and activity["acquires"]==activity["image_waits"]==activity["releases"] and activity["swapchains_destroyed"]==2,"Both real GL eyes and OpenXR shutdown ownership remain balanced")
        saved=inspect(target)
        if mode=="unconfirmed":
            check(saved==original,"Unconfirmed colored cut is cancelled exactly before standalone save")
            continue
        scene=saved["scene"];objects=scene["objects"];archive=scene["polygon_sources"]
        check(activity.get("cut_apply_ready") is True and len(objects)==3 and not objects[0]["visible"] and objects[1]["group"]==objects[2]["group"]!=0,"Controller Apply persists separate colored shell and cap in one native group")
        retained=dict(objects[0]);baseline=dict(original["scene"]["objects"][0])
        for value in (retained,baseline):value.pop("visible",None)
        check(retained==baseline,"Real-loader cut retains exact original editable paint, shape, raw source and corner authority")
        source=archive["authorities"][objects[0]["polygon"]];shell=archive["authorities"][objects[1]["polygon"]];cap=archive["authorities"][objects[2]["polygon"]];fields=archive["color_fields"]
        check(shell["colors"] is not None and cap["colors"] is None and fields[shell["colors"]]["basis"]==fields[source["colors"]]["basis"] and shell["origin"]==cap["origin"],"Saved XR clipped corners share the actual source RGBA basis and complete source capsule; cap has independent material")
        check(archive["color_bases"]==original["scene"]["polygon_sources"]["color_bases"],"XR cut neither rasterizes nor rewrites original straight linear corner RGBA")
        expected=.2*(3**.5)*.2
        check(saved["closed"] and abs(saved["x_min"]-expected)<1e-5 and abs(saved["x_max"]-.2)<1e-5,"Independent saved geometry inspection confirms the actual slider half-space and closed shell/cap")
print(len(checks),"actual real-loader colored cut checks",flush=True)
