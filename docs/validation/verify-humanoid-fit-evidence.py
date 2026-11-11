#!/usr/bin/env python3
"""Verify a sealed fitting evidence archive without extracting or running code."""
import hashlib
import json
import pathlib
import sys
import tarfile

receipt = pathlib.Path(sys.argv[1])
record = json.loads(receipt.read_text())["evidence_archive"]
archive = receipt.parent / record["file"]
assert hashlib.sha256(archive.read_bytes()).hexdigest() == record["sha256"]
with tarfile.open(archive) as stream:
    members = stream.getmembers()
    assert all(item.isfile() for item in members)
    assert len(members) == len({item.name for item in members}) == record["decoded_members"]
    payloads = {item.name: stream.extractfile(item).read() for item in members}
manifest = payloads.pop("MANIFEST.json")
assert hashlib.sha256(manifest).hexdigest() == record["decoded_manifest_sha256"]
expected = json.loads(manifest)["members"]
assert payloads.keys() == expected.keys()
for name, payload in payloads.items():
    assert len(payload) == expected[name]["bytes"], name
    assert hashlib.sha256(payload).hexdigest() == expected[name]["sha256"], name
assert len(manifest) + sum(map(len, payloads.values())) == record["decoded_payload_bytes"]
print(f"PASS {len(members)} decoded evidence members; archive and all payload hashes match")
