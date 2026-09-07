# Native v2 Rust patch

The keyframe pacing correction enables WebRTC's existing
`WebRTC-Pacer-KeyframeFlushing` behavior in the zero-playout-delay factory
environment; the default factory remains unchanged. On the first
packet of a new keyframe, the pacer removes superseded queued video and RTX
packets for that stream. A keyframe already being paced is preserved; audio and
other stream queues are unaffected. It does not impose a frame quota or change
encoder, source, publication identity, or the selected screen preset.

The pinned WebRTC implementation and its `FlushesPacketsOnKeyFrames` test are
in [pacing_controller.cc](https://webrtc.googlesource.com/src/+/refs/branch-heads/7559/modules/pacing/pacing_controller.cc)
and [pacing_controller_unittest.cc](https://webrtc.googlesource.com/src/+/refs/branch-heads/7559/modules/pacing/pacing_controller_unittest.cc).
Two app #139 preview-stall candidate runs passed the unchanged full-interval
receiver threshold: p95 132/133 ms, maximum gap 532/467 ms, zero invalid CRC
markers, continuous screen audio/remote voice, and unchanged publication and
encoder identity. These candidate runs do not replace qualification of the
published SDK pin in the application repository.

The v1.10.0-syrnike.9 candidate has normalized tree
`8f5c3220de048f23a8f17b4a65653be3c5b11179`. Session shutdown now cancels
and joins publisher negotiation tasks before closing signaling and peer
connections. Fast negotiation includes its answer wait; debounced negotiation
can be cancelled both before and during execution. Closed-session checks share
the task-creation locks with shutdown so no task can escape the drain. See
[`docs/negotiation-shutdown.md`](../../docs/negotiation-shutdown.md) for the
focused reproduction and validation.

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

v1.10.0-syrnike.7 has normalized tree
`8f653241fab0416c9c0a2da37398d90bc25790a3` and also fences pre-encoded reference continuity across WebRTC
encoder-queue drops. Capture assigns a private ingress sequence before handing
an encoded buffer to WebRTC; every pass-through encoder tracks its own sequence
and source identity. A missing access unit or failed encoded-image callback
requires a fresh keyframe before forwarding dependent frames. Raw capture
sequence gaps, timestamp alignment and the public SDK API remain independent
of this private sequence. The state has constant size and adds no queue or wait.

v1.10.0-syrnike.8 has normalized tree
`4f24fb875460568d7cf2d1f38f491ce104123281`. It gives ICE UDP/STUN/TURN
sockets to individual ports instead of retained allocation sequences. In the
pinned WebRTC build, failed-network regathering prunes ports while retaining
the sequence's shared UDP socket until session teardown. The injected allocator
clears only the session's shared-socket flag after the peer connection applies
its defaults. Continual gathering, all network types, ICE transports and the
public SDK API remain unchanged. Network manager and packet socket factory
retain the same factory/connection-context lifetime as the default allocator.

See [the deterministic socket-retirement reproduction](../../docs/ice-socket-retirement.md).

The unsuccessful tag v1.10.0-syrnike.3 referenced the unpublished Rust commit
and produced no release assets. v1.10.0-syrnike.4 is the reproducible successor.
