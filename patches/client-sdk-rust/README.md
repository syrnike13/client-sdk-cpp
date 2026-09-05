# Native v2 Rust patch

The submodule pins the published upstream commit
`1a477bc422c6890537b3bcdb017f0ac094d49661` (livekit-ffi/v0.12.75).
`0001-native-v2-runtime.patch` contains the callback-cycle cleanup, publisher
transceiver reuse and regression tests, and pre-encoded H264 support from #121.
It replaces the former three-patch series plus the unpublished local Rust
commit `247ef11b8f4aa87dc877d6ad2a40b4b28cf78a4b`.

The resulting normalized Git tree is exactly
`cf1625a3bce834c10a0372b9269c5eed30cad2a7` for v1.10.0-syrnike.6. It additionally fixes current-track
MSID signalling for reused screen senders and safely returns an absent track
for inactive RTP senders. Both defects have focused regression tests; the
replacement path was also decoded by a neutral application observer through
six publication generations and recovery to 1080p60.
A single patch permits strict forward/reverse applicability checks
even where the former patches changed one another's context. CMake applies it
on a clean checkout and verifies its reverse on reconfiguration.

The next revision has normalized tree
`8f653241fab0416c9c0a2da37398d90bc25790a3` and also fences pre-encoded reference continuity across WebRTC
encoder-queue drops. Capture assigns a private ingress sequence before handing
an encoded buffer to WebRTC; every pass-through encoder tracks its own sequence
and source identity. A missing access unit or failed encoded-image callback
requires a fresh keyframe before forwarding dependent frames. Raw capture
sequence gaps, timestamp alignment and the public SDK API remain independent
of this private sequence. The state has constant size and adds no queue or wait.

The unsuccessful tag v1.10.0-syrnike.3 referenced the unpublished Rust commit
and produced no release assets. v1.10.0-syrnike.4 is the reproducible successor.
