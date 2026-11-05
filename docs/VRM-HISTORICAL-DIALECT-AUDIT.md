# Historical official VRM schema audit

The declared-dialect validator now also accepts an explicit pinned VRM0 schema
root. It resolves each local resource under its declared Draft4/2020 dialect;
undeclared VRM0 resources retain Draft4. This is an offline test utility change.
Product code, schemas, archives and validated outputs are unchanged.

240 historical files match their original per-file SHA256 records and pass all
957 extension-schema instances: 127 VRM0, 113 VRMC_vrm, 96 springBone, 288
nodeConstraint and 333 MToon. This includes both CC0 bases, original VRM1 source
export/reimport, HDR packages, prepared-normal/post-UV outputs and published
expression cohorts. The earlier root audit of 20 main Rim files is excluded
from this new cohort; its 44-file Normal/Rim result remains separately scoped.

Real negative controls reject a missing MToon texture index, a string texture
coordinate through the referenced core2020 schema, and a string VRM0 humanoid
node through nested Draft4 references. All 15 VRM0 schema blobs match the pinned
94e82dd specification ledger. Input and schema hashes remain exact after audit.

The machine receipt records every actual path, expected hash, original report
and validation result. It also retains unavailable original paths and reports
without recoverable per-file hashes; those entries are not retroactively
qualified. The first collector stopped on a list-shaped report before
validation; only its layout handling was corrected. No export data was repaired.
These results update standards evidence; they add no new rendering, export,
application, device or full-goal admission claim.
