# Optional Mesa dependency patch

`0001-rtasm-release-empty-exec-heap.patch` targets the checksum-pinned official
Mesa26.2.5 release recorded in SOURCES.json. It fixes POSIX rtasm pool lifetime
and partial-initialization cleanup. Original MIT source notices are retained.
It is not automatically applied, built or installed by Dome Sculpt.

See [the investigation and evidence](../../docs/MESA-XR-RTASM-LIFETIME.md).
