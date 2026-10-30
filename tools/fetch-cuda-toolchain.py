#!/usr/bin/env python3
"""Stage the pinned optional CUDA development toolkit without system writes."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import urllib.request
import tarfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--destination", type=Path, required=True)
parser.add_argument("--cache", type=Path)
args = parser.parse_args()
destination = args.destination.resolve()
if destination.exists():
    parser.error("destination already exists; choose a new directory")
destination.parent.mkdir(parents=True, exist_ok=True)
cache = (args.cache or destination.parent / "cuda-downloads-13.4.1").resolve()
cache.mkdir(parents=True, exist_ok=True)
pin = json.loads((Path(__file__).resolve().parents[1] /
                 "docs/validation/CUDA-13.4.1-TOOLCHAIN.json").read_text())
with tempfile.TemporaryDirectory(prefix="cuda-stage-", dir=destination.parent) as temporary:
    stage = Path(temporary)
    sdk = stage / "sdk"
    (sdk / "licenses").mkdir(parents=True)
    for component in pin["components"]:
        archive = cache / Path(component["relative_path"]).name
        if not archive.exists():
            partial = archive.with_suffix(archive.suffix + ".partial")
            with urllib.request.urlopen(component["url"], timeout=60) as source, partial.open("wb") as output:
                shutil.copyfileobj(source, output)
            partial.replace(archive)
        with archive.open("rb") as source:
            checksum = hashlib.file_digest(source, "sha256").hexdigest()
        if archive.stat().st_size != int(component["size"]) or checksum != component["sha256"]:
            raise RuntimeError(f"CUDA archive checksum/size mismatch: {archive}")
        unpack = stage / component["component"]
        unpack.mkdir()
        with tarfile.open(archive) as package:
            package.extractall(unpack, filter="data")
        roots = list(unpack.iterdir())
        if len(roots) != 1 or not roots[0].is_dir():
            raise RuntimeError("Unexpected CUDA component archive layout")
        for entry in roots[0].iterdir():
            if entry.name == "LICENSE":
                shutil.copy2(entry, sdk / "licenses" / component["component"])
            elif entry.is_dir():
                shutil.copytree(entry, sdk / entry.name, dirs_exist_ok=True, symlinks=True)
            else:
                shutil.copy2(entry, sdk / entry.name)
    (sdk / "lib64").symlink_to("lib", target_is_directory=True)
    sdk.rename(destination)
print(f"Staged CUDA {pin['release']} in {destination}")
