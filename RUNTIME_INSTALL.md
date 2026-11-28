# SysV native installation

The runtime release builds the read-only Common V2 provider at the pinned source revision and installs `/System/Filesystems/SYSV.so` and `/System/Resources/Documentation/Filesystem-SysV.pdf`. Unsupported operations are not advertised. Native compilation uses CPU-specific optimisation; no PGO or physical firmware qualification is claimed.
