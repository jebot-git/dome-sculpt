#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exhaustive absolute-prefix export denial tests. Python is test-only.

The native consumer retains the persistent denial hook and exact file/source/
package checks. Each partition repeats the complete uninjected measurement three
times. This tool partitions work, never allocation points. Old invocations and
failed runs are retained in separate run directories.
"""
import argparse
import contextlib
import copy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
import uuid

FORMAT = 1
TIMEOUT = 3500
COMMON = ("format", "baseline", "baseline_counts", "stable_baseline",
          "fixture_sha256", "scene_sha256", "baseline_package_sha256",
          "cache_budget", "profile")
ENV_KEYS = {"PATH", "LC_ALL", "LANG", "LOCPATH", "TZ", "LP_NUM_THREADS",
            "GLIBC_TUNABLES"}
ENV_PREFIXES = ("LD_", "ASAN_", "UBSAN_", "LSAN_", "MALLOC_", "DOME_",
                "LIBGL_", "MESA_", "DRI_", "EGL_", "GL_", "XR_")


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(data)
    return h.hexdigest()


def json_digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True,
                                    separators=(",", ":")).encode()).hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def write(path, value):
    path = Path(path)
    temp = path.with_name(path.name + ".tmp-" + uuid.uuid4().hex)
    temp.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    os.replace(temp, path)


def native_environment():
    env = dict(os.environ)
    env.update(LC_ALL="C", LIBGL_ALWAYS_SOFTWARE="1", LP_NUM_THREADS="2")
    env.setdefault("ASAN_OPTIONS", "detect_leaks=1:halt_on_error=1")
    env.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")
    return env


def pinned_environment(env):
    return {k: v for k, v in env.items()
            if k in ENV_KEYS or k.startswith(ENV_PREFIXES)}


@contextlib.contextmanager
def lock(path, exclusive=False):
    with Path(path).open("a") as stream:
        try:
            fcntl.flock(stream, (fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH)
                        | fcntl.LOCK_NB)
        except BlockingIOError:
            raise ValueError("Evidence is in use; do not replace or restart a live run")
        yield


def protected_inputs(binary, fixture, source, extra, env):
    files = {binary.resolve(), fixture.resolve(), Path(__file__).resolve()}
    tracked = subprocess.check_output(["git", "-C", str(source), "ls-files", "-z"])
    files.update(source / os.fsdecode(p) for p in tracked.split(b"\0") if p)
    files.update(Path(p).resolve() for p in extra)
    result = subprocess.run(["ldd", str(binary)], env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
    require("not found" not in result.stdout, "Unresolved native dependency")
    dependencies = set()
    for line in result.stdout.splitlines():
        match = re.search(r"(/[^\s]+)", line)
        if match:
            dependencies.add(Path(match.group(1)).resolve())
    require(dependencies, "No dynamic dependencies were resolved")
    files.update(dependencies)
    return {str(p): digest(p) for p in sorted(files)}, result.stdout


def check_inputs(plan):
    require(pinned_environment(native_environment()) == plan["environment"],
            "Native environment differs from the frozen plan")
    drift = [p for p, h in plan["inputs"].items()
             if not Path(p).is_file() or digest(p) != h]
    require(not drift, "Protected input drift: " + ", ".join(drift))
    require(json_digest(plan["inputs"]) == plan["inputs_digest"],
            "Frozen input manifest digest is invalid")


def elf_mappings(pid):
    result = set()
    try:
        lines = Path(f"/proc/{pid}/maps").read_text().splitlines()
    except FileNotFoundError:
        return result
    for line in lines:
        fields = line.split(None, 5)
        if len(fields) < 6 or not fields[5].startswith("/"):
            continue
        path = Path(fields[5])
        try:
            with path.open("rb") as f:
                if f.read(4) == b"\x7fELF":
                    result.add(str(path.resolve()))
        except FileNotFoundError:
            continue
    return result


def run_native(plan, run, case, arguments):
    """Own, time-limit and reap the actual native process, including on SIGTERM."""
    output = run / case
    require(not output.exists(), "Native output already exists: " + str(output))
    command = [plan["binary"], plan["fixture"], str(output), *arguments]
    before = time.monotonic()
    observed = set()
    child = None
    timed_out = False
    handlers = {}

    def interrupt(signum, frame):
        raise InterruptedError("Interrupted by signal " + str(signum))

    try:
        for signum in (signal.SIGINT, signal.SIGTERM):
            handlers[signum] = signal.signal(signum, interrupt)
        with (run / (case + ".log")).open("x") as log:
            child = subprocess.Popen(command, env=native_environment(), stdout=log,
                                     stderr=subprocess.STDOUT, start_new_session=True)
            write(run / (case + ".process.json"),
                  {"pid": child.pid, "wrapper_pid": os.getpid(), "command": command})
            while child.poll() is None:
                observed.update(elf_mappings(child.pid))
                if time.monotonic() - before > TIMEOUT:
                    timed_out = True
                    os.killpg(child.pid, signal.SIGKILL)
                    break
                time.sleep(0.05)
            code = child.wait()
    finally:
        if child is not None and child.poll() is None:
            os.killpg(child.pid, signal.SIGKILL)
            child.wait()
        for signum, handler in handlers.items():
            signal.signal(signum, handler)
    unknown = sorted(observed - plan["inputs"].keys())
    receipt = {"state": "terminal", "exit_code": code, "timed_out": timed_out,
               "command": command, "seconds": time.monotonic() - before,
               "mapped_elf": sorted(observed), "unprotected_elf": unknown,
               "maps_verified": bool(observed) and not unknown,
               "inputs_digest": plan["inputs_digest"],
               "environment_digest": json_digest(plan["environment"])}
    report = output / ("measurement.json" if arguments[0] == "--oom-measure" else "range.json")
    if report.is_file():
        receipt.update(native=read(report), native_report_sha256=digest(report))
    return receipt


def measurement_ok(native, plan=None):
    require(native["format"] == FORMAT and native["terminal_exit"] == 0,
            "Native report did not terminate successfully")
    n = native["baseline"]
    require(type(n) is int and n > 0 and native["baseline_counts"] == [n, n, n]
            and native["stable_baseline"] is True, "Unstable allocation baseline")
    require(native["cache_budget"] == 0 and native["profile"] in ("release", "asan-ubsan"),
            "Wrong profile or allocation cache policy")
    if plan:
        require({k: native[k] for k in COMMON} == plan["measurement"],
                "Mixed baseline, profile, fixture, scene or successful package")


def receipt_ok(receipt, plan, part=None):
    require(receipt["state"] == "terminal" and receipt["exit_code"] == 0
            and receipt["timed_out"] is False, "Missing, failed or timed-out native process")
    require(receipt["inputs_digest"] == plan["inputs_digest"]
            and receipt["environment_digest"] == json_digest(plan["environment"]),
            "Mixed binary, source, dependency or environment inputs")
    require(receipt["maps_verified"] is True and receipt["mapped_elf"]
            and not receipt["unprotected_elf"], "Native ELF mappings were not protected")
    require(receipt.get("input_checks") == {"before": True, "after": True},
            "Native process lacks complete before/after input verification")
    native = receipt["native"]
    measurement_ok(native, plan if "measurement" in plan else None)
    if part is None:
        require(native["mode"] == "measure", "Wrong calibration mode")
        return
    begin, end = part["begin"], part["end"]
    require(receipt["part"] == part["id"] and native["mode"] == "range"
            and native["begin"] == begin and native["end"] == end,
            "Receipt belongs to a different absolute prefix partition")
    for key in ("completed", "denied", "successful", "translated"):
        require(type(native[key]) is int and native[key] >= 0, "Invalid native counter")
    require(native["completed"] == end - begin
            and native["denied"] + native["successful"] == end - begin
            and native["translated"] <= native["denied"]
            and native["first_completed"] == begin and native["last_completed"] == end - 1,
            "Incomplete absolute prefix coverage")
    n = native["baseline"]
    require(native["retried_prefixes"] == [p for p in sorted({0, n // 2, n}) if begin <= p < end],
            "Missing early/middle/final complete-package retry")
    require(native["endpoint_successful"] is (begin <= n < end),
            "Final uninjected endpoint was not successful")


def layout_ok(plan):
    require(plan["format"] == FORMAT, "Wrong plan format")
    measurement_ok({**plan["measurement"], "terminal_exit": 0})
    cursor = 0
    require(len(plan["parts"]) >= 2, "An exhaustive plan needs multiple partitions")
    for i, part in enumerate(plan["parts"]):
        require(part["id"] == i and type(part["begin"]) is int
                and type(part["end"]) is int and part["begin"] == cursor
                and part["end"] > cursor, "Missing, duplicated, overlapping or empty partition")
        cursor = part["end"]
    require(cursor == plan["measurement"]["baseline"] + 1,
            "Plan does not cover the successful endpoint")


def current_run(root):
    name = read(root / "active.json")["run"]
    require(re.fullmatch(r"run-[0-9a-f]{32}", name), "Invalid active run directory")
    run = root / name
    plan = read(run / "plan.json")
    layout_ok(plan)
    return run, plan


def prepare(args):
    root = args.root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    with lock(root / "run.lock", True):
        require(2 <= args.partitions <= 512, "Partition count must be in [2,512]")
        run = root / ("run-" + uuid.uuid4().hex)
        run.mkdir()
        # A failed new setup cannot accidentally reuse an earlier green plan.
        write(root / "active.json", {"run": run.name})
        env = native_environment()
        inputs, ldd = protected_inputs(args.binary.resolve(), args.fixture.resolve(),
                                       args.source.resolve(), args.protect, env)
        plan = {"format": FORMAT, "binary": str(args.binary.resolve()),
                "fixture": str(args.fixture.resolve()), "inputs": inputs,
                "inputs_digest": json_digest(inputs), "environment": pinned_environment(env),
                "child_timeout_seconds": TIMEOUT, "ctest_timeout_seconds": 3600}
        write(run / "inputs.json", plan)
        (run / "ldd.log").write_text(ldd)
        check_inputs(plan)
        measured = run_native(plan, run, "case-9999", ["--oom-measure"])
        measured["input_checks"] = {"before": True, "after": False}
        write(run / "calibration.json", measured)
        check_inputs(plan)
        measured["input_checks"]["after"] = True
        write(run / "calibration.json", measured)
        receipt_ok(measured, plan)
        plan["measurement"] = {k: measured["native"][k] for k in COMMON}
        require(plan["measurement"]["fixture_sha256"] == inputs[plan["fixture"]],
                "Native fixture digest differs from protected input")
        n = plan["measurement"]["baseline"]
        tail = min(32, n)
        split = n + 1 - tail
        require(split >= args.partitions - 1, "Too many partitions for the measured baseline")
        parts = [{"id": i, "begin": i * split // (args.partitions - 1),
                  "end": (i + 1) * split // (args.partitions - 1)}
                 for i in range(args.partitions - 1)]
        parts.append({"id": args.partitions - 1, "begin": split, "end": n + 1})
        plan["parts"] = parts
        layout_ok(plan)
        part = parts[-1]
        pilot = run_native(plan, run, f"case-{part['id']:04d}",
                           ["--oom-range", str(part["begin"]), str(part["end"]), str(n)])
        pilot["part"] = part["id"]
        pilot["input_checks"] = {"before": True, "after": False}
        write(run / f"part-{part['id']:04d}.json", pilot)
        check_inputs(plan)
        pilot["input_checks"]["after"] = True
        write(run / f"part-{part['id']:04d}.json", pilot)
        receipt_ok(pilot, plan, part)
        # This is a conservative scheduling estimate, never a relaxed timeout.
        # The real tail remains an actual covered partition in the final proof.
        max_width = max(p["end"] - p["begin"] for p in parts)
        estimate = measured["seconds"] + 2 * pilot["seconds"] / tail * max_width + 30
        plan["sizing"] = {"tail_prefixes": tail, "tail_seconds": pilot["seconds"],
                          "max_partition_width": max_width,
                          "conservative_seconds": estimate, "margin": 2}
        write(run / "plan.json", plan)
        check_inputs(plan)
        require(estimate < TIMEOUT,
                f"Measured tail forecasts {estimate:.1f}s; increase partition count, not timeout")
        write(run / "ready.json", {"plan_sha256": digest(run / "plan.json")})
        print(json.dumps({"run": str(run), "baseline": n, "partitions": len(parts),
                          "pilot_seconds": pilot["seconds"], "estimate_seconds": estimate}), flush=True)


def ready_plan(root):
    run, plan = current_run(root)
    require(read(run / "ready.json")["plan_sha256"] == digest(run / "plan.json"),
            "Plan is incomplete or changed")
    return run, plan


def verify_stored(run, plan, part):
    receipt = read(run / f"part-{part['id']:04d}.json")
    receipt_ok(receipt, plan, part)
    report = run / f"case-{part['id']:04d}" / "range.json"
    require(digest(report) == receipt["native_report_sha256"]
            and read(report) == receipt["native"], "Native range report changed")
    return receipt


def execute_part(args):
    root = args.root.resolve()
    with lock(root / "run.lock"):
        run, plan = ready_plan(root)
        require(0 <= args.part < len(plan["parts"]), "Partition index is out of range")
        part = plan["parts"][args.part]
        with lock(run / f"part-{args.part:04d}.lock", True):
            check_inputs(plan)
            path = run / f"part-{args.part:04d}.json"
            if path.exists():
                # Only an already verified terminal run can be reused, e.g. the
                # actual tail pilot. Partial and failed invocations are retained.
                receipt = verify_stored(run, plan, part)
            else:
                receipt = run_native(plan, run, f"case-{args.part:04d}",
                                     ["--oom-range", str(part["begin"]), str(part["end"]),
                                      str(plan["measurement"]["baseline"])])
                receipt["part"] = args.part
                receipt["input_checks"] = {"before": True, "after": False}
                write(path, receipt)
                check_inputs(plan)
                receipt["input_checks"]["after"] = True
                write(path, receipt)
                receipt_ok(receipt, plan, part)
            check_inputs(plan)
            print(f"PASS partition {args.part}: [{part['begin']},{part['end']}) "
                  f"{receipt['seconds']:.3f}s", flush=True)


def validate_coverage(plan, receipts):
    layout_ok(plan)
    require(len(receipts) == len(plan["parts"]), "Missing or additional partition receipts")
    totals = {k: 0 for k in ("completed", "denied", "successful", "translated")}
    for part, receipt in zip(plan["parts"], receipts):
        receipt_ok(receipt, plan, part)
        for key in totals:
            totals[key] += receipt["native"][key]
    require(totals["completed"] == plan["measurement"]["baseline"] + 1
            and totals["denied"] > 0 and totals["successful"] > 0,
            "Exhaustive denial/success coverage is incomplete")
    return totals


def aggregate(args):
    root = args.root.resolve()
    with lock(root / "run.lock"):
        run, plan = ready_plan(root)
        check_inputs(plan)
        calibration = read(run / "calibration.json")
        receipt_ok(calibration, plan)
        calibration_file = run / "case-9999" / "measurement.json"
        require(digest(calibration_file) == calibration["native_report_sha256"]
                and read(calibration_file) == calibration["native"], "Calibration report changed")
        receipts = [verify_stored(run, plan, part) for part in plan["parts"]]
        totals = validate_coverage(plan, receipts)
        check_inputs(plan)
        report = {"format": FORMAT, "state": "terminal", "exit_code": 0,
                  "plan_sha256": digest(run / "plan.json"), "totals": totals,
                  "measurement": plan["measurement"], "inputs_digest": plan["inputs_digest"],
                  "partition_receipts": {f"part-{p['id']:04d}.json":
                                         digest(run / f"part-{p['id']:04d}.json")
                                         for p in plan["parts"]}}
        write(run / "coverage.json", report)
        print(json.dumps(report), flush=True)


def run_all(args):
    """Bounded orchestration; CTest can independently run the same part commands."""
    require(1 <= args.jobs <= 8, "Use between one and eight concurrent native processes")
    root = args.root.resolve()
    with lock(root / "run.lock"):
        run, plan = ready_plan(root)
        check_inputs(plan)
        count = len(plan["parts"])
        require(not (run / "batch.json").exists(), "Batch already attempted; preserve it")
        status = {"state": "running", "pid": os.getpid(), "jobs": args.jobs,
                  "completed": [], "failed": [], "live": {}}
        children = {}
        next_part = 0
        handlers = {}

        def interrupted(signum, frame):
            raise InterruptedError("Batch interrupted by signal " + str(signum))

        try:
            for sig in (signal.SIGINT, signal.SIGTERM):
                handlers[sig] = signal.signal(sig, interrupted)
            while next_part < count or children:
                while next_part < count and len(children) < args.jobs and not status["failed"]:
                    log = (run / f"part-{next_part:04d}.wrapper.log").open("x")
                    command = [sys.executable, str(Path(__file__).resolve()), "part", str(root), str(next_part)]
                    child = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
                    children[next_part] = (child, log)
                    status["live"][str(next_part)] = child.pid
                    next_part += 1
                write(run / "batch.json", status)
                for part, (child, log) in list(children.items()):
                    code = child.poll()
                    if code is not None:
                        log.close()
                        del children[part]
                        del status["live"][str(part)]
                        status["completed" if code == 0 else "failed"].append({"part": part, "exit_code": code})
                        print(f"partition {part}: exit {code}", flush=True)
                if status["failed"] and not children:
                    break
                time.sleep(0.2)
            status.update(state="terminal", exit_code=1 if status["failed"] else 0)
        except BaseException as exc:
            status.update(state="terminal", exit_code=1, error=str(exc))
            raise
        finally:
            for child, log in children.values():
                child.terminate()
            for child, log in children.values():
                child.wait()
                log.close()
            for sig, handler in handlers.items():
                signal.signal(sig, handler)
            write(run / "batch.json", status)
        require(not status["failed"] and len(status["completed"]) == count,
                "At least one real partition failed; aggregate is not valid")
        aggregate(args)


def controls(args):
    """Synthetic verifier controls; these do not claim native package coverage."""
    measure = {"format": 1, "baseline": 8, "baseline_counts": [8, 8, 8],
               "stable_baseline": True, "fixture_sha256": "fixture", "scene_sha256": "scene",
               "baseline_package_sha256": "package", "cache_budget": 0, "profile": "release"}
    plan = {"format": 1, "measurement": measure, "inputs_digest": "inputs", "environment": {},
            "parts": [{"id": 0, "begin": 0, "end": 4}, {"id": 1, "begin": 4, "end": 9}]}
    receipts = []
    for part in plan["parts"]:
        a, b = part["begin"], part["end"]
        success = int(b == 9)
        native = {**measure, "terminal_exit": 0, "mode": "range", "begin": a, "end": b,
                  "completed": b-a, "denied": b-a-success, "successful": success, "translated": 0,
                  "first_completed": a, "last_completed": b-1, "endpoint_successful": b == 9,
                  "retried_prefixes": [p for p in (0, 4, 8) if a <= p < b]}
        receipts.append({"state": "terminal", "exit_code": 0, "timed_out": False,
                         "inputs_digest": "inputs", "environment_digest": json_digest({}),
                         "maps_verified": True, "mapped_elf": ["native"], "unprotected_elf": [],
                         "native": native, "part": part["id"],
                         "input_checks": {"before": True, "after": True}})
    require(validate_coverage(plan, receipts)["completed"] == 9, "Positive verifier control failed")
    checks = 1

    def negative(name, mutate):
        nonlocal checks
        p, r = copy.deepcopy(plan), copy.deepcopy(receipts)
        mutate(p, r)
        try:
            validate_coverage(p, r)
        except (ValueError, KeyError, TypeError):
            checks += 1
            return
        raise ValueError("Verifier accepted " + name)

    negative("gap", lambda p,r: p["parts"][1].update(begin=5))
    negative("overlap", lambda p,r: p["parts"][1].update(begin=3))
    negative("duplicate", lambda p,r: p["parts"].append(p["parts"][0]))
    negative("missing endpoint", lambda p,r: p["parts"][1].update(end=8))
    negative("missing receipt", lambda p,r: r.pop())
    negative("duplicate receipt", lambda p,r: r.append(r[0]))
    negative("timeout", lambda p,r: r[0].update(timed_out=True))
    negative("nonzero native", lambda p,r: r[0].update(exit_code=1))
    negative("live native", lambda p,r: r[0].update(state="running"))
    negative("wrong binary/source", lambda p,r: r[0].update(inputs_digest="other"))
    negative("wrong environment", lambda p,r: r[0].update(environment_digest="other"))
    negative("unchecked inputs", lambda p,r: r[0].update(input_checks={"before": True, "after": False}))
    negative("unprotected library", lambda p,r: r[0].update(unprotected_elf=["unknown"]))
    for key, value in (("baseline", 9), ("baseline_counts", [8,9,8]), ("profile", "asan-ubsan"),
                       ("fixture_sha256", "other"), ("scene_sha256", "other"),
                       ("baseline_package_sha256", "other"), ("cache_budget", 1),
                       ("completed", 3), ("successful", 2), ("terminal_exit", 1),
                       ("retried_prefixes", []), ("first_completed", 1)):
        negative("wrong " + key, lambda p,r,k=key,v=value: r[0]["native"].update({k:v}))
    negative("unsuccessful endpoint", lambda p,r: r[1]["native"].update(endpoint_successful=False))
    negative("shuffled receipt", lambda p,r: r.reverse())
    print(f"PASS {checks} synthetic coverage-verifier controls (not native allocation coverage)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    p = sub.add_parser("prepare")
    p.add_argument("root", type=Path)
    p.add_argument("--binary", required=True, type=Path)
    p.add_argument("--fixture", required=True, type=Path)
    p.add_argument("--source", type=Path, default=Path(__file__).resolve().parent.parent)
    p.add_argument("--protect", action="append", default=[], type=Path)
    p.add_argument("--partitions", type=int, default=128)
    p.set_defaults(function=prepare)
    for name, function in (("part", execute_part), ("aggregate", aggregate), ("run", run_all)):
        p = sub.add_parser(name)
        p.add_argument("root", type=Path)
        if name == "part":
            p.add_argument("part", type=int)
        if name == "run":
            p.add_argument("--jobs", type=int, default=2)
        p.set_defaults(function=function)
    sub.add_parser("controls").set_defaults(function=controls)
    args = parser.parse_args()
    args.function(args)


if __name__ == "__main__":
    try:
        main()
    except (Exception, KeyboardInterrupt) as exc:
        print("FAIL " + str(exc), file=sys.stderr, flush=True)
        sys.exit(1)
