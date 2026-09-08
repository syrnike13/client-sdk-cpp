# Fresh native audio ingress

`FreshAudioStream` is a track-bound input for native playout. It returns owned
48 kHz stereo PCM in 10 ms frames. It does not subscribe a publication, select an
output device, mix participants, or change Room state. Those remain application
owners. Stop all streams before disconnecting their Room or shutting down the SDK.

```text
WebRTC decoded sink (timestamp before conversion)
  -> NativeAudioSink format conversion
  -> four preallocated PCM frames
  -> synchronous private native read
  -> FreshAudioFrame with steady-clock decoded_at
```

Construct, read, inspect stats, and destroy a stream on one owner thread. Reads
never wait for a frame. The queue admits at most four 960-sample values; overflow
evicts the oldest. Producer and consumer each make one atomic attempt to access
the queue. Contention drops the incoming frame or returns `empty` to the reader.
The queue does not allocate, log, notify a worker, serialize protobuf, or invoke
an application callback. It bypasses the legacy Rust and C++ `AudioStream` queues.

Keep `FreshAudioFrame::decoded_at` unchanged through subsequent application
queues. It is in `std::chrono::steady_clock`'s domain. Conversion time, native
queue residence, and native-to-C++ bridge time contribute to age. Translation
uses the start of the native read and may conservatively overestimate age by
the duration of that call; it never intentionally refreshes the timestamp at
delivery. `age_us` is a snapshot at return, not a replacement timestamp.

The default maximum age is 60 ms. A caller may request 1–1,000,000 microseconds;
larger buffers or age limits should not be used to hide a slow playout consumer.
Stale frames are discarded before delivery. `sequence`, `discontinuity`, and
the received/drop/stale/invalid/queued counters make gaps and capacity visible.
An empty read leaves its output value unchanged: callers must inspect the result
and must not replay their previous frame.

The existing native sink performs rate/channel conversion and may initialize
its resampler when the input format changes. That work precedes the fixed queue
and is included in frame age; this API does not claim that all WebRTC internals
are allocation-free. Invalid converted frame lengths are counted and rejected.
There is no silent fallback to a different queue or an unbounded path.

The stream retains its typed track. Destruction first retires ingress, then
removes the native sink before releasing that track. The application still
needs a publication/source generation fence: retaining a track does not grant
permission to play PCM after a participant's track is removed or replaced.

With manual subscriptions, seed the application's publication registry after
connect using `Room::remotePublications(maximum)`. Initial publications do not
emit `onTrackPublished`. This bounded snapshot copies owning handles under the
Room lock, avoiding concurrent iteration over a participant's publication map.
It reports truncation and caps capacity at 4096 entries. Take snapshots on a
control thread, and fence their application against intervening Room events;
an owning handle alone does not make a removed publication current again.

Verification lives in the Rust `fresh_audio_stream` tests and C++
`AudioSourceTest.FreshSink*` tests. Full remote Room, output, latency, and resource
proofs are the consuming application's Media Lab responsibility.

The deterministic Rust callback test injects 250 ms of native processing age
for 100 consecutive frames and verifies that none becomes fresh at the queue
boundary. Separate application probes delay the reader by 250 ms and exercise
renderer wake stalls. A 30-minute, two-participant application run measured
p95 scheduled age at most 50 ms and maximum 56.911 ms, with bounded queues and
no underruns; these are development measurements on one Windows machine.

## Publisher replacement regression

The remote-audio routing fixture uncovered a publisher regression with mixed
microphone, screen audio and screen video: after a track replacement worked,
removing a different track could also remove that replacement at the receiver.
Disabling transceiver reuse removed the symptom, but would discard the existing
resource bound. Waiting for removal offer/answer settlement did not fix it.

The correction keeps libwebrtc's sender MSID in `SetLocalDescription` and applies
the current track identity only to the offer emitted to the SFU. All codec,
MID, SSRC and transport fields remain the same. The fixture passes 17 phases,
including immediate screen replacement, microphone replacement, adjacent source
continuity, demand fencing, persistent volume and receiver Room replacement.
A native Rust regression test checks local/wire identities over replacement and
a subsequent unrelated negotiation while retaining one RTP transceiver.
