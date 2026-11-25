# Filesystem-SysV

Independent System V, Xenix and Coherent filesystem family provider for InfiltratorOS. Target: one native C/C++ engine and one distributable `SYSV.so`.

**Status: documentation only; source extraction pending.** [Central source](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/sysv). Editable Linux v6.12.107-derived source remains centrally. No independent driver or release exists.

## Common V2

Use only `infiltrator_filesystem_api_v2` from [Libraries](https://github.com/Infiltrator-Projects/Libraries). Own explicit variant identification, superblocks, byte order, inode and block trees, free lists and directory records in the same engine for hosted and freestanding execution. No FUSE, separate per-format kernel/EFI driver or legacy ABI.

## Build and install

Planned: `/System/Filesystems/SYSV.so` and `/System/Resources/Documentation/Filesystem-SysV.pdf`. Generic amd64 .deb, source-downloading CPU-native .run and GitHub-generated source ZIP only. No build or installer exists yet.

See [migration status](MIGRATION_STATUS.md) and [engineering philosophy](docs/PHILOSOPHY.md).
