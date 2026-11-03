# Repeated native creator interchange — private checkpoint

This isolated child of 1f6658f repairs two actual creator→native import→fit/pose→
export→native reimport defects. It changes no public layouts or Scene schema.
The prior qualified bone-length/source-graph component remains frozen separately.

Native `node.extras.domeSculptHumanoid` version 1 stores actual node references.
The source graph previously copied those old IDs while correctly remapping the
standard VRM humanoid. A genuine creator output had one 54-reference hint that
was correct before reimport and wrong after re-export (chest 8 versus 10). The
new private helper remaps the known version through the actual complete
original-node→emitted-node table. It preserves every unrelated extra and raw
source byte. Unknown versions/fields/roles, invalid or missing target IDs refuse
precisely before output publication. All changes occur on the detached export
JSON; no source archive is rewritten. The original negative and corrected
sample differ only in 54 known reference values; all other JSON and the BIN
payload are byte-identical before the later image-resource change.

The repeated export also exposed image-resource accounting. Distinct retained
image names and unnamed generated images can describe exactly the same encoded
pixels. The female sample declared 66 images with 263,783,424 decoded RGBA bytes,
but only 182,223,616 bytes were unique encoded-byte+MIME resources. Independent
ownership pushed a native project over 256 MiB. Counting all aliases against the
old remaining-pixel decoder then rejected the real male sample even after leaf
sharing.

A worker-local private decoder now shares immutable pixel leaves only when MIME
and encoded bytes match exactly (hash candidates are verified by exact bytes).
Every original image descriptor, name, URI, MIME, byte payload, image/texture/
sampler index and role-dependent color-space interpretation remains separate.
Every record still receives MIME/header/dimension validation. Each distinct
decoded owner is charged before allocation against the unchanged 64-million-
pixel cap. Existing 256-record,128 MiB per-record and 256 MiB encoded aggregate caps
remain; parser-specific smaller source/metadata limits remain in force. This is
physical ownership accounting, not a larger cap or lossy image conversion.

Current Release workflow qualification uses both genuine CC0 VRoid creator bases
under ordinary and reflected/sheared placement. It exercises real Document
fitting, pose publication, one Undo/Redo, ordered Save, file-worker VRM export,
independent own-IBM/corner/typed-metadata evaluation, every 54-reference hint,
another native reimport and Save/restart. Every decoded RGBA/alpha slot is
compared with an uncached native decode, and raw original bytes stay exact.
Final repeated projects are 232,567,895/232,595,038 bytes for female and
243,398,514/243,425,655 bytes for male, below the unchanged 256 MiB limit.
The current 80-check Release workflow passes in 162.25 s. The exact unchanged
asserted ASan/UBSan/leak binary passes the controlled final workflow in 653.22 s,
with native wait 0, authoritative outer 0 and 1164 code/support/binary hashes
unchanged before/after. The first attempt completed native 80/exit 0 in 878.20 s
but the outer session reported 143; both outcomes remain recorded separately.
No native failure was hidden or relabeled. The final outer was exec Python,
recorded native wait+afterhashes before exit and was explicitly polled at less
than 60-second intervals. An idle transport lease is an inference, not a proven
cause of the first outer termination.

Focused helper 58 checks include 4 measured Cancel and 11 persistent allocation
failure boundaries. Decoded-resource 46 checks include 14 Cancel and 10 persistent
allocation failures,256 alias-expanded descriptors, PNG/JPEG, distinct bytes,
MIME/dimension negatives and near-budget distinct-owner refusal. Software and
strict PCI 03 A 770 each pass 34 checks/eight independent images with max error 0 at
unchanged tolerance 2. No GUI/HMD claim or installation is added; the existing
shared native controls are exercised through their Document/worker APIs.

Khronos 2.0.0-dev.3.10 currently reports 20 emitted outputs with 0 errors and 400
unfiltered warnings. The 20 VRM 0 outputs pass the pinned official schema set
with 0 errors. Final current-source Release and sanitizer scopes each pass 218 checks
(80 workflow, 58 hint helper, 46 image resource and 34 graphics). See
validation/SOURCE-NATIVE-HINTS.json for source/support hashes, controlled exit
proof and all earlier negative evidence. This is a four-TU overlay with matched
fully owned/instrumented prior support, not a new whole-branch build.

The full production goal remains active. Source 24/25/26 semantic integration,
full VRM 1 export, distinct per-part pose reconciliation, broader role/axis and
source-rig authoring, arbitrary spring/collider radius/shear/physics transport,
FBX skin/deformer and generic multipart graph workflows remain separate work.
