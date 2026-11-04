#!/usr/bin/env python3
"""Build native allocator lifetime/fault tests from a pinned Mesa source/build.

No download, patching, dependency installation or system-library mutation.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shlex
import subprocess
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--build", required=True, type=Path,
                        help="matching configured Mesa build with compile_commands.json")
    parser.add_argument("--output", required=True, type=Path,
                        help="new directory for objects, executable, logs and receipt")
    parser.add_argument("--sanitizers", action="store_true")
    parser.add_argument("--original", action="store_true",
                        help="require the original source and its retained-pool counterexample")
    args = parser.parse_args()
    source, build, out = (p.resolve() for p in (args.source, args.build, args.output))
    repo = Path(__file__).resolve().parents[1]
    pins_file = repo / "third_party/mesa/SOURCES.json"
    pins = json.loads(pins_file.read_text())
    changed = "src/gallium/auxiliary/rtasm/rtasm_execmem.c"
    for name, expected in pins["original_sha256"].items():
        if name == changed and not args.original:
            expected = pins["patched_rtasm_sha256"]
        if sha(source / name) != expected:
            raise RuntimeError("Mesa source differs from pinned input: " + name)
    commands_file = build / "compile_commands.json"
    commands = json.loads(commands_file.read_text())
    out.mkdir(parents=True, exist_ok=False)
    fixture = repo / "tests/mesa-rtasm-pool.c"
    inputs = [pins_file, fixture, Path(__file__).resolve(), commands_file]
    inputs += [source / name for name in pins["original_sha256"]]
    before = {str(p): sha(p) for p in inputs}
    result = {"state": "running", "pid": os.getpid(), "steps": [],
              "source": str(source), "build": str(build),
              "sanitizers": args.sanitizers, "original_counterexample": args.original,
              "before": before,
              "scope": "Complete pinned u_mm, futex and rtasm TUs, native real-allocation "
                       "link wrapping, partial-initialization failures, executable function "
                       "and concurrent owners. Not a full driver build or application test."}
    receipt = out / "result.json"
    start = time.monotonic()

    def save():
        receipt.write_text(json.dumps(result, indent=2) + "\n")

    def run(name, command, expected=0):
        then = time.monotonic()
        env = dict(os.environ)
        if args.sanitizers:
            env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1",
                       UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        run_result = subprocess.run(command, cwd=build, env=env,
                                    capture_output=True, text=True)
        (out / (name + ".log")).write_text(run_result.stdout + run_result.stderr)
        result["steps"].append({"name": name, "command": command,
                                "exit_code": run_result.returncode,
                                "seconds": time.monotonic() - then})
        save()
        if run_result.returncode != expected:
            raise RuntimeError(name + ": unexpected exit; see " + str(out / (name + ".log")))
        return run_result

    save()
    try:
        objects = []
        for relative in ("src/util/u_mm.c", "src/util/futex.c", changed):
            entry = next(e for e in commands if e["file"].endswith("/" + Path(relative).name))
            if Path(entry["directory"]).resolve() != build:
                raise RuntimeError("Compile database belongs to another build directory")
            command = entry.get("arguments") or shlex.split(entry["command"])
            command = list(command)
            obj = out / (Path(relative).name + ".o")
            command[command.index("-c") + 1] = str(source / relative)
            command[command.index("-o") + 1] = str(obj)
            if "-MD" in command:
                command.remove("-MD")
            for option in ("-MQ", "-MF"):
                if option in command:
                    index = command.index(option)
                    del command[index:index + 2]
            if args.sanitizers:
                command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1", "-g"]
            run(Path(relative).name, command)
            objects.append(obj)
        binary = out / "mesa-rtasm-pool-test"
        command = ["cc", "-std=c11", "-O2", "-g", "-pthread", str(fixture)]
        command += [str(obj) for obj in objects]
        command += ["-Wl,--wrap=" + name for name in ("calloc", "free", "mmap", "mmap64", "munmap")]
        command += ["-o", str(binary)]
        if args.sanitizers:
            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        run("link", command)
        test = run("test", [str(binary)], 1 if args.original else 0)
        if args.original and "FAIL last owner frees both mapping and heap metadata (metadata=2 mappings=1)" not in test.stderr:
            raise RuntimeError("Original source did not reproduce the expected ownership counterexample")
        if not args.original and not re.search(r"PASS [0-9]+ allocator checks; 2000 concurrent cycles; no retained metadata or executable mappings", test.stdout):
            raise RuntimeError("Missing complete qualified-host allocator result")
        result["exit_code"] = 0
    except Exception as error:
        result.update(exit_code=1, error=str(error))
    finally:
        result.update(state="terminal", seconds=time.monotonic() - start,
                      drift=[str(p) for p in inputs if sha(p) != before[str(p)]])
        save()
    print(json.dumps({key: result.get(key) for key in ("exit_code", "error", "seconds", "drift")}))
    return result["exit_code"] or bool(result["drift"])


if __name__ == "__main__":
    raise SystemExit(main())
