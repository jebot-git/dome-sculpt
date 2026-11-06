# Isotropic remeshing integration — 2026-11-06

The native application now offers **Isotropic remesh** in the universal Mesh
menu and shared XR mesh page. It remeshes the displayed posed surface using
world-space edge lengths, including sheared and reflected placements, and opens
the result as editable polygon topology. The complete original object stays
hidden and restorable, with its paint, layers, rig and source data intact. The
result is explicitly unpainted; these fields are not silently transferred or
discarded. Work runs through the existing background scene transaction, with
source checks, cancellation, one-step history and ordered saves.

This integrates source `c618c8285ba5bd03c609a4efd284c5cbaaa3c59c` on the current
direct-upload and retopology-input foundation. The application-facing code and
kernel match the qualified source exactly. The kernel rejects candidate
triangles whose normals cannot be resolved reliably in the actual float frame;
the current application also passes the affine editor gate that failed in the
original historical candidate.

Validation is recorded in
[the matched receipt](validation/TRIANGLE-REMESH-CURRENT-MATCHED.json):

- Fresh complete reachable Release and ASan/UBSan app/core/render/XR provider
  cohorts: 233 project binary inputs each. Sanitizer archive packaging changed
  to thin archives without changing compiler flags. Preserved resource/setup
  failures remain recorded; the genuine completed native run is identified.
- Per profile: 2,166 native remesh passing lines, 248 affine polygon/retopology
  checks, 108 query families, 13,442 input checks and 73 direct-upload checks.
- Ordinary native FLTK universal-menu interaction: 20 normal and 22 compact
  checks per profile, including staged configuration, cancellation, Apply,
  source retention, undo/redo, save and fresh reopen.
- Actual OpenXR loader with synthetic controllers: five routes and 103 checks
  per profile covering Apply, Cancel, focus loss, reference-space change and
  undo. Held confirmation has a real button level with no second transition.
  The controller changes the edge length to 140 mm; a separately captured
  1024-pixel eye image visibly reads **140.00 mm**.
- 43 corresponding Release/sanitizer native, image and desktop/XR output pairs
  are byte-identical. Root rechecks 461 raw project binary hashes and the five
  serialized standalone artifacts and retained restore controls.
- A separate unchanged ordinary application run passes 24 checks on actual
  Intel Arc A770 rendering. Current EGL context, PCI `0000:03:00.0`, device
  `8086:56a0`, render node and open file descriptors authenticate the device.
  Its saved scene matches the software run. Controller poses remain synthetic.
- The current Release allocation-failure sweep closes all 128 partitions:
  116,337 persistent denials and one successful endpoint, with no input drift.

The current exhaustive sanitizer allocation sweep is now **complete**: all 128
partitions cover 116,337 persistent denials and the successful endpoint. Root
rechecks every completed receipt/log, the contiguous half-open range
[1,116339), and all 3,118 final protected inputs. Read
[the final receipt](validation/TRIANGLE-REMESH-SAN-FINAL.json).

The original coordinator and recorded children disappeared while its status
still said running. The separate continuation reused 24 completed receipts after
verifying the original inputs. Two passing logs without exit receipts were
preserved and rerun. That original interrupted status is not rewritten as
success. The historical 116,325-point paired sweep retains its separate scope;
later authoring candidates require their own changed-map qualification.

No fresh full-project ALL regression, complete production remeshing quality,
physical-headset acceptance, new compute acceleration or installation is claimed.
The complete requirements remain active, including texture sets, polygon-field
transport, bounded local meshing, rig/avatar interoperability and controller
drawers after the basic targets.
