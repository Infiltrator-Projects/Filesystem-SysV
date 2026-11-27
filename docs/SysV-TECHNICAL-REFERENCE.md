# SysV: technical reference

## Historical family and variant boundaries

The System V filesystem descends from early Unix inode-based file storage. The related Xenix, System V Release 2, System V Release 4, Coherent and Seventh Edition layouts share important ideas but differ in superblock placement, field alignment, byte order and allocator details. Treat each as a specific profile, not a single interchangeable encoding. Root inode is 2 and inode 1 is traditionally associated with bad blocks. Inode zero means no object. Filesystem zones are allocation units and must be distinguished from host sectors and device I/O buffers.

## Volume identification and placement

The typical layout is a reserved boot region, superblock, contiguous inode table and data area. Xenix commonly places its 1024-byte superblock at volume byte 1024, while System V and Coherent commonly place the 512-byte superblock at byte 512. Some System V Release 2 images reserve a whole leading track; zone addresses then need the corresponding profile-specific offset. System V supports profiles with 512, 1024 and 2048-byte zones. Coherent normally uses 512. Never identify a variant solely by a guessed block size; confirm superblock structure, root inode and plausible filesystem bounds.

## Byte order and profile selection

System V and Xenix on-disk numeric encoding depends on their platform lineage; the commonly encountered x86 profile is little-endian. Coherent uses little-endian 16-bit words but a 32-bit PDP-style ordering: the higher 16-bit word precedes the lower 16-bit word. Packed 24-bit inode zone addresses require separate decoding rules and must not be read as host integers. Coherent lacks a reliable magic constant; use cross-validated geometry, expected field layout and root directory evidence. Reject ambiguous profiles for writes. V7 also needs independent identification and does not become System V merely because its inode record looks similar.

The imported sysv.h defines Xenix, SysV4, SysV2 and Coherent as separate host interpretations. SYSV_MAGIC_BASE-derived VFS identifiers are not on-media signatures; use the variant detection and byte-order evidence in super.c. Unambiguous profile admission is mandatory before writes.

## System V Release 4 superblock

Relative offsets in the four-byte-aligned 512-byte layout: first data zone at 0 (16 bits), total zones at 4, free-stack count at 8 (16 bits), fifty 32-bit free-zone entries beginning at 12. Free-inode cache count at 212, one hundred 16-bit inode entries from 216, state bytes from 416, update time at 420, device information from 424, free-zone total at 432, free-inode total at 436, volume and pack names at 440 and 446. State at 500, magic at 504, zone-size type at 508. The clean-state relation uses 0x7C269D38 minus the update time. Apply the relation only to the verified Release 4 profile.

## Release 2, Xenix and Coherent differences

Release 2 uses two-byte alignment: first data zone at 0, total zones at 2, free-zone count at 6, fifty free-zone entries from 8, inode cache count at 208, cache at 210, status bytes from 410 and update time at 414. Free totals at 426/430; volume and pack names at 432/438; state, magic and type occupy the same final offsets 500/504/508. Its documented clean state is 0xCB096F43. Xenix uses a 100-entry free stack and 100-inode cache within a larger superblock; the clean byte is 0x46. Coherent uses 64 free-stack entries and 100 inode-cache entries plus interleave parameters. Do not apply Release 4 offsets to these layouts.

## Inode table and record inventory

The inode table is addressed according to the selected profile's inode-table start. Each 64-byte inode contains 16-bit mode at 0, links at 2, UID at 4, GID at 6, 32-bit file length at 8, thirteen three-byte zone addresses at 12 through 50, generation byte at 51, and 32-bit access, modification and inode-change timestamps at 52, 56 and 60. To locate inode n, add 64 times (n-1) to the table's starting byte. Validate against the first data zone and maximum available inode slots. Device-special inodes can interpret the mapping field differently from ordinary files. A nonzero link count alone does not validate a mode or data mapping.

## Direct and indirect block mapping

There are ten direct addresses followed by single, double and triple indirect roots. Indirect blocks store 32-bit entries, so a 1024-byte zone contains 256 pointers. Logical zones 0 through 9 are direct, 10 through 265 are singly indirect, 266 through 65801 are doubly indirect, and the following range is triply indirect. For a doubly indirect logical zone 500, subtract 266 to obtain 234; select entry 0 in the outer block and entry 234 in the inner block. Convert the final physical zone to bytes using the selected profile. Treat zero addresses as absent mappings where the format permits holes; bound traversal against file length, address width and volume geometry.

## Directories and link semantics

The ordinary directory record is 16 bytes: a 16-bit inode number followed by fourteen filename bytes, with shorter names padded by zeros. Do not require a terminator when a full-width name uses all fourteen bytes. Unused slots carry inode zero. Traverse directory data through the same mapping machinery as other files, enforcing record alignment and inode bounds. Dot and dot-dot establish local and parent references. Hard links share inode identity; removing a directory name decrements its link count and reclaims storage only after the final valid reference and any host open-handle semantics are resolved. Historical symbolic-link conventions vary by profile.

## Free-zone stack algorithm

The superblock contains a cache of free-zone addresses. This is not a bitmap and not a linked list with a pointer in every free zone. When allocation pops the last address in a chunk, that zone carries the next chunk of free addresses. Decode its count and entries according to the selected layout, load the next cache, then allocate the popped zone. Freeing into a full cache writes the existing chunk into the newly freed zone and resets the cache to point to it. Some profiles use a zero sentinel; Coherent uses a zeroed terminating chunk. Validate every count and address, detect repeated zones and cycles, and update total counters without underflow. Free inodes are cached separately and replenished by scanning the inode table.

## Writes and crash consistency

The classic layouts do not provide a general metadata journal. Creating a file requires an allocated inode and directory slot; extension may allocate indirect metadata and payload zones. Never publish uninitialised pointer blocks or return a referenced zone to the free stack. Truncate must release indirect subtrees safely after removing references. Dirty/clean state and superblock update ordering differ among profiles. Preserve reserved and interleave metadata. A relocation changes the owning direct or indirect pointer after copying and verifying data; reinsert the old zone into the free-list structure only after publication is safe. Validate failure ordering and native repair compatibility before writes are allowed.

## Independent consistency checking

Reconstruct every live inode's zone ownership, including indirect nodes and metadata, and compare it with the complete free-stack traversal. Detect invalid addresses, duplicate claims, zones absent from both sets, cycles in free chunks, inode-cache duplication and mismatched link counts. Distinguish bad-block reservations from leaks. Verify root and parent relationships and count all reachable inode references. Test each profile independently with zero-filled sparse data, full fourteen-byte names, direct-to-indirect boundaries, empty and full free stacks, mixed-endian fields and deliberately damaged superblocks. Do not repair ambiguities by assuming a different System V profile.

## Implementation and qualification boundary

The independent provider uses Common V2: https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/FILESYSTEM_COMMON_ABI_V2.md . Negotiate actual operation masks and media profiles. Distinguish unsupported, corrupt, I/O and stale-handle results. Require bounded host I/O, exclusive unmounted ownership and durable flush before mutation. Test Media and Defragmenter consume the same engine, not separate format parsers. Hosted and freestanding binaries, relocation, recovery, BOOT, kernel integration and packages require separate exact-source evidence. Current support belongs to the repository README and qualification records. The intended package paths are /System/Filesystems/ and /System/Resources/Documentation/; the reference is not an installation or release-status claim.

## Sources and qualification boundary

Independent technical documentation: https://docs.kernel.org/6.8/filesystems/sysv-fs.html . Field-layout and algorithm references: https://github.com/torvalds/linux/tree/v6.12/fs/sysv and include/linux/sysv_fs.h. Seek original Xenix, AT&T and Coherent formatter/checker media for fixture validation. The historic Linux implementation is a useful interoperability oracle but is not a complete specification of every Unix-derived variant. Exact per-variant identification, address packing, free-list chunk alignment and clean-state handling require tests against independent images. Original implementation evidence does not establish a qualified provider.

Test free-list loops, ambiguous superblocks and indirect arithmetic for every admitted profile.

Retained source: https://github.com/Infiltrator-Projects/Filesystems/blob/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/sysv/linux/sysv.h (blob 0a48b2e7ed).
