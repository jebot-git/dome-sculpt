# Explicit atlasing of unused source coordinates — private dependency

Sourcef62 permits explicit atlas preparation of sampled UV1 materials but still
constructs a Strict paint map from existing UVs before preparation when there are
no sampled PBR texture roles. A valid untextured mesh with degenerate unused UVs
therefore fails before the chosen atlas operation can start.

This small change unwraps before paint construction only for an explicit atlas
choice with no sampled color/metal-rough/normal/emission/occlusion roles. The
captured original asset, unused coordinates and original ordered vertex/corner
identity remain immutable. Textured inputs retain the existing per-role source
sampling path; default unprepared import retains its original refusal.

A valid negative against exactf62 core archives fails at the real degenerate-UV
constructor. A whole owned import-authoring.cpp O3 object overlay excludes the
old owner from the linker map and passes56 native checks: authored and implicit
materials, authored and generated-flat normals, both instances, exact source/
coordinate ownership, actual background Import/paint/immediately queued Save,
restart and separate paint/import Undo/Redo. All headers matchf62; no public C++
layout changes. This is an isolated object-overlay proof, not yet a new coherent
whole-core build or production qualification. The first fixture attempt added
an unaligned accessor after PNG bytes and is preserved separately; the valid
fixture aligns its bytes and authenticates the exact expected UV refusal.

The f62 parent has267 Release suite passes, separate XR19/FLTK12/Arc135 and
Khronos36-file evidence; those passes are not relabeled as this new product.
Full native/renderer/sanitizer integration remains required. No installation,
headset launch or active-session changes were made.

A subsequent fresh coherent optimized whole-core/support build passes three
selected native suites in0.21s. A separate fresh own C/C++ core/support
ASan/UBSan/leak build passes the same three in1.91s. UI/XR are disabled in both
configurations; no renderer, frontend or device qualification is inferred. Exact
source/log hashes and remaining typed-MToon integration caveat are in
validation/UNMAPPED-ATLAS-IMPORT.json. This private component is ready for
combined native import integration, not full production completion.
