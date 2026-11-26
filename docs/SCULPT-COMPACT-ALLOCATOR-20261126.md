# Compact sculpt allocator qualification

Candidate `7a06dbe5` keeps one live charge-owner pointer in the numeric account,
clears it when the operation ends, and rejects allocation through an expired
account. Each shared node stores only the weak account and reservation size.
Deallocation reads no charge-owner pointer. The original owning limits,
retained input charges, cancellation and exact-peak/one-byte-denial tests stay
unchanged.

Actual GCC class layouts verify that all five shared control blocks fall by
8 bytes: 160→152, 216→208, 248→240, 200→192 and 160→152. The original complete
translation unit was queried without producing an admitted replacement object;
the new sizes come from the actual Release and ASan/UBSan compiler dumps.
Both builds verify one complete changed core translation unit, 196 qualified
existing owners, 197 archive payloads and four executable links.

Release passes all 167 full independent native checks in 75.538 seconds, with
2,681,860,096 bytes peak owned RSS. Both Release and ASan/UBSan pass 26 surface
and 92 Document pipeline checks, including the exact local working peak,
refusal with one byte less, cancellation and retained immutable results.
Native accepted/recovery projects remain byte-identical to the earlier
50,061,799-byte `d32c725a…` payload.

The full strict workflow is **not accepted**: the unchanged 4 GiB RSS guard
stopped it after 842.217 seconds at 4,305,338,368 bytes, after check 164 and
before the final quota-refusal/cancellation endpoint. There is no sanitizer
diagnostic, input/provider drift, DRM access or owned survivor. Smaller
allocation sizes do not establish an RSS improvement. No original limit,
sanitizer behavior or dataset is weakened, and this separate component result
does not extend the `8c3b9aea` sustained Arc result to this executable or to the
current integrated application.

The [record](validation/sculpt-index-compact-20261126.json) indexes the
[evidence archive](validation/sculpt-index-compact-20261126.tar.xz), 108 members,
365,492 bytes, SHA-256
`1ac3f5a7f0c73cc41655dc04e646f33ad85e66919f135d93c832f4d1c54beb0c`.
Every member and actual local authority was rehashed after packing. Identical
native project payloads reference the full payload already preserved by the
`9cc18990` milestone archive rather than duplicating it.
