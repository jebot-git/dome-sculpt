# Prepared authoring integration merge

Branch `work/authoring-main-merge-staging-20261112` combines main39b27170 with
the frozen application candidate5c820f34. This is a prepared merge for review;
it does not admit the runtime to main or establish build/test acceptance.

The only merge conflict was `docs/HANDOFF.md`. The resolution keeps the latest
main handoff and the candidate's separate freeze document. Both parent histories
and all current main evidence remain available.

An independent comparison finds every non-documentation Git record identical
to the frozen candidate:1709 records, including mode, type, blob and path, with
canonical SHA256
`19a23af6c8f6f9ad0340ab109ac3f09ba16b0542865bd8edbb33804dabe094e7`.
Only documentation differs. The [identity receipt](validation/authoring-main-merge-staging-20261112.json)
records the comparison and the remaining admission gates.

The current5c owning Release build and sanitizer qualification are independent
of this preparation. Their actual outcomes, exact native/helper/raw/manual
checks, fresh producers/font and public desktop/OpenXR checks must be reviewed
before this merge changes main. Further production requirements remain open
even if those integration checks pass. Frozen sources/providers, current main
runtime and active desktop/headset sessions remain unchanged.
