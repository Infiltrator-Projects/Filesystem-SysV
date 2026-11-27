# Filesystem-SysV

## 1. Purpose and format scope

System V and related historical Unix filesystem variants. This repository is the designated independent implementation home for InfiltratorOS. **Independent read-only Common V2 development provider. No released installable package yet.**

## 2. On-disk architecture and invariants

Explicit variant identification and byte order govern superblocks, inode and block trees, free lists and directory records. Geometry or magic shared by several historical variants is insufficient to select a decoder safely. The qualified development profile is narrower than the complete format.

## 3. Common V2 implementation and capabilities

The local implementation exports only `infiltrator_filesystem_api_v2`. CORE provides probe, mount, unmount, stat, enumeration and read; VOLUME and METADATA are negotiated from the descriptor. Hosted and import-free AMD64 firmware builds share the native engine. Clean Release 2 and Release 4, both byte orders and 512/1024/2048-byte filesystem blocks. Direct and single/double/triple indirect addressing, explicit sparse files, nested directories, links and free-list validation. Byte-authored tests passed; third-party historical media and signed firmware boot remain unqualified.

## 4. Integrity, mutation, allocation and recovery

Mutation, formatting, check/repair and DEFRAG map/plan/execute/verify/recover require format-specific allocation and ordering invariants, exclusive writable host access and demonstrated interruption recovery. Until qualified, reject them before writing. Preserve native disk semantics and do not add a kernel or FUSE fallback as an alternative filesystem authority.

## 5. Build, installation and release

The target installation is `/System/Filesystems/SYSV.so` and `/System/Resources/Documentation/Filesystem-SysV.pdf`. Neither file is currently distributed as a release. For development, check out the pinned Libraries revision shown in CI, then run `make carrier` (or the exact CI build steps for UDF). The intended release remains a portable amd64 DEB and CPU-native source-first RUN, with GitHub's automatic source archive.

## 6. Qualification and limitations

Exact tested source [6729ade16d57](https://github.com/Infiltrator-Projects/Filesystem-SysV/commit/6729ade16d578216f575c3d6d014439d8e63bd02); [passing provider CI 45954373659](https://github.com/Infiltrator-Projects/Filesystem-SysV/actions/runs/45954373659). Clean Release 2 and Release 4, both byte orders and 512/1024/2048-byte filesystem blocks. Direct and single/double/triple indirect addressing, explicit sparse files, nested directories, links and free-list validation. Byte-authored tests passed; third-party historical media and signed firmware boot remain unqualified. Formatting, writes, repair, defragmentation and release remain unqualified.

## 7. Technical references and provenance

- [Technical reference (PDF)](docs/SysV.pdf) — original-format technical reference as one integrated manual and [PDF stream source](docs/SysV.pdfstreams); not an operational manual.

- [Pinned central source baseline](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/sysv)
- [Native design](native/filesystems/sysv/DESIGN.md)
- [Migration and qualification plan](MIGRATION_STATUS.md)
- [Original-format reference source](docs/SysV.pdfstreams) and [PDF](docs/SysV.pdf)
- [Project philosophy](docs/PHILOSOPHY.md)
- [Common ABI](https://github.com/Infiltrator-Projects/Libraries)

Preserve inherited notices and licences on source extraction; new project-authored code uses GPL-3.0-or-later. The [technical reference](docs/SysV-TECHNICAL-REFERENCE.md) expands original-format research. installation remains pending. Central code or test results are not evidence of an independent provider.
