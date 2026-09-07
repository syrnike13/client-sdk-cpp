# Fixed-frame microphone processing

`RealtimeAudioProcessing` is a narrowly scoped native microphone DSP seam for
the application fork's microphone stage (#127 in syrnike13/syrnike13-app).
The existing `AudioProcessingModule` API remains unchanged.

Create, use and destroy one instance on its sole owner thread. Construction
allocates two independent WebRTC processors and prewarms them with 200 fixed
frames. `process` accepts exactly 480 PCM16 mono samples at 48 kHz, an optional
rendered reference of the same shape, and a bounded 0–500 ms delay. Processing
applies AEC followed by NS; either processor can be bypassed independently.
Absent reference bypasses AEC without disabling NS. The application owns
reference freshness, renderer epochs, gate/mute, gain and device/publication
lifetime; this class has no Room, device, callback or global FFI handle.

The private native ABI has only create/process/destroy operations. Fixed buffers
and integer errors avoid protobuf requests and error-string allocations in
ordinary processing. The owner contract forbids concurrent calls. A new echo
timeline requires a newly prepared instance; there is intentionally no reset
API because WebRTC `Initialize` allocates state.

Local Windows x64 Release experiments against the pinned `.12` base:

| Measurement | Result |
| --- | --- |
| Existing protobuf APM, 1,000 warmed forward/reverse/delay frames | 17,000 heap allocations / 489,000 bytes |
| Fixed-frame seam, 1,000 measured echo frames | 0 observed HeapAlloc/HeapReAlloc calls |
| Synthetic 30 ms delayed/convolved echo, NS disabled | 40.6 dB ERLE |
| Independent stationary-noise fixture | 13.8 dB attenuation, 0 measured heap calls |
| Application warm DSP worker, 200 mute/unmute cycles and config updates | 0 measured heap calls |
| Initial experiment with WebRTC Initialize at epoch change | 1,369 allocations; rejected and removed from the seam |

Heap evidence uses disposable-process import interposition in the loaded SDK and
UCRT on the calling thread. It is not a complete ETW trace or proof about every
allocator, platform, physical echo path or double-talk condition. The native
application fixture records timing, epochs and whole-worker heap samples.

SDK unit tests cover invalid delay, exact unavailable-reference bypass, owner
thread rejection, independent NS over 20 create/process/destroy cycles, and
synthetic AEC with an ERLE threshold of 10 dB.
