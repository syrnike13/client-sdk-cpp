# Native v2 Rust patch

The submodule pins the published upstream commit
`1a477bc422c6890537b3bcdb017f0ac094d49661` (livekit-ffi/v0.12.75).
`0001-native-v2-runtime.patch` contains the callback-cycle cleanup, publisher
transceiver reuse and regression tests, and pre-encoded H264 support from #121.
It replaces the former three-patch series plus the unpublished local Rust
commit `247ef11b8f4aa87dc877d6ad2a40b4b28cf78a4b`.

The resulting normalized Git tree is exactly
`f042986ae7f0d68e04f41f086b19375c012c4982`, matching the previously tested local
worktree. A single patch permits strict forward/reverse applicability checks
even where the former patches changed one another's context. CMake applies it
on a clean checkout and verifies its reverse on reconfiguration.

The unsuccessful tag v1.10.0-syrnike.3 referenced the unpublished Rust commit
and produced no release assets. v1.10.0-syrnike.4 is the reproducible successor.
