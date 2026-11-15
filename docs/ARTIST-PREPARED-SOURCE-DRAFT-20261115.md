# Prepared normal Bake source continuation — draft, 2026-11-15

New child merges artist source `2f89410c` and current combined source `3d164382`
through `f1a9970e`, preserving the missing logical emission UV1 refusal and all
source histories. Current application sources/providers remain unchanged.

The draft adds a prepared-source resolver that consumes actual current dual
PBR/MToon normal planes and their owned frame/coordinate recipe. It uses the
prepared vertex bitangent policy, independently retained sampling coordinates
when present, existing morph/skin evaluation and bounded cancellation. Material
channels use the actual composed preview; prepared normals are not relabeled as
raw original images. Canonical/native-paint/current-independent-field guards
remain. The ordinary artist source route is retained.

A new native fixture constructs a genuine first artist Bake and reuses that
prepared result in a second Bake. Its independent double-precision consumer
implements normal basis/image decoding separately, compares both callback roles
and actual second-Bake texels, and exercises native roundtrip, real Document
worker/ordered Save, Undo/Redo, memory refusal and capture cancellation for
MToon0/MToon1 and rest/posed original artist inputs. This describes the fixture,
not an executed pass.

All three complete changed owning TUs and the new fixture compile under the
current compiler and actual canonical Release context, sequentially with a2GiB
address-space limit. Fresh preprocess dependency snapshots and exact literal
compiler defines are retained. The initial compile-planv1 failed preprocessing
before any compile because the core command lacked the fixture's explicit src
include path; that failure remains preserved. Newv2 adds the include declared
by its actual CMake target, passes four preprocesses/four compilations and has
zero input drift.

No archive/link/runtime or graphics execution has occurred. Old compiler
archives are not used as current qualification. Link/runtime checks await the
parent's independent fresh current-toolchain193-member core snapshot. The full
production goal remains incomplete; this draft is not admitted to main.

See [compile-only receipt](validation/ARTIST-PREPARED-SOURCE-DRAFT-20261115.json).
