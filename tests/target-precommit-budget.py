#!/usr/bin/env python3
"""Complete only timed-out immutable target precommit ranges, without replay."""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess
import sys

binary, output, expected_hash = Path(sys.argv[1]).resolve(), Path(sys.argv[2]), sys.argv[3]
output.mkdir(parents=True, exist_ok=True)
actual_hash = hashlib.sha256(binary.read_bytes()).hexdigest()
if actual_hash != expected_hash:
    raise RuntimeError("The exact frozen sanitizer binary changed")

def run(shard):
    command = ["nice", "-n", "15", str(binary), "precommit", str(shard), "16"]
    path = output / f"precommit-{shard}-of16.log"
    with path.open("w") as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=600)
    text = path.read_text()
    first, last = 1 + 9488 * shard // 16, 9488 * (shard + 1) // 16
    if result.returncode or f"Actual range {first}..{last} of 9488" not in text or f"PASS all {last-first+1} actual target precommit failures" not in text:
        raise RuntimeError(f"Actual repair range {first}..{last} failed; see {path}")
    print(f"PASS exact actual range {first}..{last}, frozen binary unchanged", flush=True)
    return {"shard": shard, "count": 16, "range": [first, last], "command": command, "log": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
    results = list(executor.map(run, [12, 13, 14, 15]))
if hashlib.sha256(binary.read_bytes()).hexdigest() != actual_hash:
    raise RuntimeError("Frozen binary changed during execution")
(output / "report.json").write_text(json.dumps({"scope": "Timeout-only range completion with the same immutable binary and actual full helper per fault", "binary": str(binary), "binary_sha256": actual_hash, "cases": results, "boundaries": 2372, "preparation_replay": False, "original_44_run_unchanged": True}, indent=2) + "\n")
