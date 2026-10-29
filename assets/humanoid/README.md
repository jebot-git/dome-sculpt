# Embedded humanoid source assets

The shipped `src/humanoid-base.inc` contains graphical data converted from the
MakeHuman Community repository at revision
`a8bc2d54ff0ac92e78ff71431b1023eda42bf482`.

Actual reuse comprises the hm08 body/UV topology, the default skeleton's named
joint positions/relationships, authored default skin weights and 27 sparse face
shape/expression target assets. The OBJ and target files explicitly release
themselves as CC0 in their own headers. The skeleton and weights JSON explicitly
state `"license": "CC0"`. These per-asset grant/credit records are retained in
`ASSET-NOTICES.txt`; exact source paths and SHA256 hashes are in `SOURCES.json`.
`CC0-1.0.txt` and `MAKEHUMAN-LICENSE.txt` retain the upstream assets grant and
its description of the separate software/asset licenses.

The original asset credits include Data Collection AB, Joel Palmius and Jonas
Hauquier. Target notices also retain Manuel Bastioni's original credit and the
explicit 2016 copyright transfer statement. No MakeHuman AGPL program code,
Python modules or application runtime is copied, linked or invoked. The
converter and native deformation/grooming code are original GPL-3.0-or-later
project implementation; the converted graphical data remains CC0-1.0.

Regenerate from pinned, hash-checked originals with:

```sh
python3 tools/prepare-humanoid-assets.py
```

This optional offline development tool downloads into ignored artifacts and
writes deterministic native arrays and notices. Building and using Dome Sculpt
requires no Python or external model files for this creator. Native compilation
includes the generated arrays directly.

`WORKFLOW-REFERENCES.json` identifies inspected official VRoid Studio workflow
and guideline pages. VRoid is a workflow reference only. Its proprietary models,
textures, presets, program source and branding are not incorporated. The
original eye and groom geometry is generated in native C++.
