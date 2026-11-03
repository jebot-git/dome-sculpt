#!/usr/bin/env python3
"""Private, finite 16B diagnostic; NEVER changes the 500M production factory.

Requires explicitly supplied frozen libraries matching this checkout's public
headers; see docs/OWNING-CONTINUATION-WIP.md. This is a single-test-TU overlay,
not a fresh product/whole-solver-SAN/native admission proof.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
parser.add_argument("--libraries", type=Path, required=True)
parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=False)
libraries = args.libraries.resolve()
archive_names = ["render", "core", "affine", "polygon"]
archives = [libraries / ("libdome-sculpt-" + n + ".a") for n in archive_names]
archives += [libraries / ("lib" + n + ".a") for n in
             ["xatlas", "mikktspace", "mikktspace-robust", "ufbx"]]
for p in archives:
    if not p.is_file():
        raise RuntimeError("Missing explicit matching archive: " + str(p))

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError("Diagnostic staging anchor changed: " + old)
    return text.replace(old, new)

header = (root / "src/anatomical-continuation.hpp").read_text()
header = replace_once(header, "Budget budget{0, options.maximum_work, cancelled};",
                      "// TEST-ONLY 16B finite diagnostic: original 500M gate still fails.\n"
                      "  Budget budget{0, 16'000'000'000ULL, cancelled};")
(out / "anatomical-continuation-diagnostic.hpp").write_text(header)
fixture = (root / "tests/anatomical-continuation-avatars.cpp").read_text()
fixture = replace_once(fixture, '#include "../src/heat-binding.cpp"',
                       '#include "heat-binding.cpp"')
fixture = replace_once(fixture, '#include "../src/anatomical-continuation.hpp"',
                       '#include "anatomical-continuation-diagnostic.hpp"')
fixture = replace_once(fixture, '#include <iomanip>',
                       '#include <iomanip>\n#include <fstream>\n#include <bit>')
fixture = replace_once(fixture, '"work_cap=500000000 mean_limit="',
                       '"production_cap=500000000 production_cost_gate=FAILED '
                       'diagnostic_cap=16000000000 mean_limit="')
snapshot = r'''
  // Test-only snapshots, without solver or output-policy changes.
  const std::string prefix = argv[4];
  static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
  nlohmann::json stored = nlohmann::json::array();
  {
    std::ofstream raw(prefix + "-full-fields.bin", std::ios::binary);
    raw.write(reinterpret_cast<const char *>(fields.fields.data()),
              std::streamsize(fields.fields.size() * sizeof(double)));
    if (!raw) throw std::runtime_error("Field snapshot failed");
    nlohmann::json layout{{"rows", fields.report.rows}, {"joints", fields.joints},
       {"element", "IEEE754-binary64"},
       {"byte_order", std::endian::native == std::endian::little ? "little" : "big"},
       {"existing_storage", {{"max_influences", WeightLimits{}.max_influences},
                             {"prune", WeightLimits{}.prune}, {"locks", "none"}}}};
    layout["parts"] = nlohmann::json::array();
    for (const auto &part : fields.parts)
      layout["parts"].push_back({{"id", part.object->id}, {"name", part.object->name},
                                 {"raw_to_row", part.raw_to_row}});
    std::ofstream meta(prefix + "-field-layout.json");
    meta << layout.dump(2) << '\n';
    if (!meta) throw std::runtime_error("Field layout snapshot failed");
    std::ofstream input(prefix + "-input-scene.json");
    input << original.dump() << '\n';
    if (!input) throw std::runtime_error("Scene snapshot failed");
  }
'''
fixture = replace_once(fixture,
                       "  size_t parts = 0, quality = 0, consumer = 0, seamfail = 0;",
                       snapshot + "  size_t parts = 0, quality = 0, consumer = 0, seamfail = 0;")
anchor = "    weights.validate(o.painted->mesh().positions.size(),\n                     o.deformation->skeleton->bones.size());"
fixture = replace_once(fixture, anchor, anchor +
                       '\n    stored.push_back({{"id", o.id}, {"skin", weights.encode()}});')
fixture = replace_once(fixture, "  bool exact = scene.encode() == original;", r'''
  {
    std::ofstream output(prefix + "-native-output-skins.json");
    output << stored.dump() << '\n';
    if (!output) throw std::runtime_error("Stored skin snapshot failed");
  }
  bool exact = scene.encode() == original;''')
cpp = out / "anatomical-continuation-diagnostic.cpp"
cpp.write_text(fixture)
tracked = sorted(p for d in [root / "src", root / "include/dome"]
                 for p in d.iterdir() if p.is_file()) + archives + [cpp, out / "anatomical-continuation-diagnostic.hpp"]
tracked += [Path(__file__).resolve(), root / "tests/anatomical-continuation-avatars.cpp", root / "third_party/sources.json"]
before = {str(p): sha(p) for p in tracked}
(out / "before.json").write_text(json.dumps(before, indent=2) + "\n")
command = [args.compiler, "-std=c++20", "-O3", "-Wall", "-Wextra", "-Wpedantic",
           "-Werror=return-local-addr"]
command += ["-I" + str(root / d) for d in
            ["include", "src", "third_party/openbrush", "third_party/nlohmann"]]
command += [str(cpp)] + [str(p) for p in archives[:3]] + ["/usr/lib64/liblapacke.so"]
command += [str(p) for p in archives[3:]]
command += ["-lpng", "-lz", "-ljpeg", "-lEGL", "-lGL", "-pthread", "-o", str(out / "probe")]
(out / "compile-command.json").write_text(json.dumps(command, indent=2) + "\n")
(out / "compiler.txt").write_bytes(subprocess.check_output([args.compiler, "--version"]))
with (out / "build.log").open("w") as log:
    subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
binary = sha(out / "probe")
results = {}
for gender in ["male", "female"]:
    begin = time.monotonic()
    with (out / (gender + ".log")).open("w") as log:
        result = subprocess.run([str(out / "probe"), gender, "body", "connected", str(out / gender)],
                                stdout=log, stderr=subprocess.STDOUT, timeout=180)
    results[gender] = {"exit": result.returncode, "wall_seconds": time.monotonic() - begin}
after = {str(p): sha(p) for p in tracked}
(out / "after.json").write_text(json.dumps(after, indent=2) + "\n")
if before != after or sha(out / "probe") != binary:
    raise RuntimeError("Diagnostic source/archive/binary changed during proof")
(out / "result.json").write_text(json.dumps({"results": results, "binary": binary,
    "production_500M_admission": False, "diagnostic_ceiling": 16_000_000_000,
    "products": {str(p): sha(p) for p in sorted(out.iterdir()) if p.is_file()}}, indent=2) + "\n")
if any(r["exit"] != 0 for r in results.values()):
    raise SystemExit("Diagnostic refused/quality failed; inspect preserved logs")
print("Diagnostic quality passed; original 500M admission remains failed. " + str(out))
