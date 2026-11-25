# SysV Common V2 migration status

## Source and ownership

[Exact central source baseline](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/sysv). Editable Linux v6.12.107-derived source remains centrally. Extract and adapt the existing implementation; preserve original licensing, attribution and provenance. New project-authored code uses GPL-3.0-or-later. No source transferred by this documentation commit.

## Qualification plan

1. Enumerate supported on-disk variants, validate geometry and features; implement bounded CORE probe.
2. Provide CORE mount/stat/list/read and VOLUME query using native explicit variant identification, superblocks, byte order, inode and block trees, free lists and directory records.
3. Test independent images for each admitted variant, ambiguous geometry, corrupt free lists and malformed records; compare independent payloads and unchanged-image hashes; inject malformed media and I/O faults.
4. Implement mutation, format, check/repair and DEFRAG map/plan/execute/verify/recover only with proven allocation, durability and interruption/recovery semantics. Unsupported operations must refuse before writing.
5. Qualify exact packaged Common V2 ABI, hosted and import-free freestanding profiles, then actual BOOT and kernel integration independently.

All independent stages, PDF manual and packaging are pending. No runtime capability is claimed.

The [design and qualification reference](docs/SysV.pdf) is available. It is not an operational manual; release and installation qualification remain pending.
