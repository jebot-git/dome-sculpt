# Private explicit canonical normal conversion kernel

This is a detached native C++ conversion candidate, not an enabled user action.
The full production objective and all original normal/export guards remain.

The legacy native canonical shader interpolates raw tangent/bitangent/normal
columns. A standard PBR normal consumer normalizes the columns. Native
position-only morphs also recompute geometric normals at intermediate weights,
whereas standard target attributes interpolate their stored deltas. The earlier
curved nonzero native morph consumer discrepancy remains 13 > 4; emitting a base
Mikk tangent alone does not solve it.

`convert_native_canonical_normals` constructs a genuinely new explicit target
attribute model and converts the actual resolved historical normal bitmap. It
retains the exact original paint and deformation owners, native topology,
source/bake/layer/history data and full target identities. No Scene, material or
source owner changes. This is not a claim that the old dynamic evaluation is
identical to the newly chosen attribute contract at every pose.

A first center-only candidate passed its interior checks but failed an
independent bilinear/boundary probe at 0.0705364 radians against the unchanged
0.02 limit. The fix enumerates every texel whose continuous bilinear footprint
intersects each actual triangle. It extends that triangle's true affine varying
fields to neighboring texel centers, converts with explicit basis conditioning
and RGB8 quantization checks, and verifies shared-footprint compatibility.
Unreferenced pixels retain their original bytes. Ordinary nearest-interior
padding is not substituted for the original native edge samples. Bounded work,
memory and cancellation apply before publication of the detached value.

Final Release and owned ASan/UBSan/leak checks each pass 210 assertions across
sixteen genuine baked models: flat/curved, mirrored tangent islands, skin,
position morphs, and protected/fractional/full legacy coverage. Each profile
independently checks 10,400 interior centers and 417,312 dense bilinear/boundary
samples under the fixed 0.02 bound. All sixteen normal images and sixteen frame
archives are bit-identical between profiles. The curved conversion changes real
pixel data; zero-tolerance and resource controls refuse explicitly.

A separate persistent C++ allocation sweep passes all 69 measured prefixes,
with exact full native source preservation and identical frame/image/quality
retry. Every one of 32 measured cancellation polls passes the same preservation
and retry checks. This warmed-composite conversion proof is not a C allocator,
Mikk or whole-export allocation sweep. Final wrappers close0 in 10.87 seconds
Release and 34.21 seconds sanitizer with1821/1819 input hashes exact.

The proof history retains initial nested-main/index compile errors, a sanitizer
harness archive-member error that appended `rig.o` instead of replacing
`rig.cpp.o`, and both actual center-only filtered failures. The final sanitizer
archive contains exactly one `rig.cpp.o`, verified byte-for-byte against the
compiled complete current TU. No negative is relabeled green.

Read [validation/CANONICAL-NORMAL-CONVERSION.json](validation/CANONICAL-NORMAL-CONVERSION.json)
for exact scopes, original negatives, complete logs and output hashes. This
kernel is not in the default application build and has no CMake/worker/UI/archive
admission. The next owning implementation must serialize and replay an explicit
conversion lease that retains the historical paint and declares the new target
contract, adopt the converted normal role without dropping other baked channels
or editable paint, and integrate actual Document transactions, history, ordered
Save and shared desktop/XR controls. It then needs native restart/re-edit and
independent morph/skin export images before enabling the operation. Typed,
retained-source and corner-owner adapters, device/interop/performance gates and
all other unfinished requirements remain.
