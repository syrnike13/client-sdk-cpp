# Publisher negotiation shutdown

Rapid publish/unpublish followed by Room disconnect can leave the fast publisher
negotiation task waiting for an answer after signaling and the peer connection
have closed. The detached task owns an `Arc<SessionInner>` throughout its
10-second answer timeout and possible retry. That retains the peer connection
factory and its native threads beyond Room teardown. The debounced path also
owns the session and previously could not cancel an already executing future.

Session shutdown now takes the single owned negotiation task under the same
state lock used to create it, sends cancellation and joins outside the lock.
The cancellation covers offer creation and answer waiting. Shutdown also takes
and cancels/joins the debouncer. A closed-session check under each creation lock
prevents a task from being installed after its shutdown drain. The public SDK
API, sender ownership, ICE interfaces, resource limits and negotiation behavior
of a live session remain unchanged.

## Focused evidence

The application's hosted run `34016936287` failed its 50-cycle
`lifecycle-churn` scenario with 99 additional handles and 12 threads after the
unchanged five-second cleanup deadline. This includes cancellation of in-flight
connects and audio/video publish/unpublish cycles through the production Engine.

The same scenario was run alone against a real local project SFU on Windows 11,
using the published `.8` DLLs: exit 1, final handle/thread deltas 4/2, zero pending
callbacks, cleanup wait 5065 ms. Intermediate samples retained groups of roughly
six native threads before later releasing them. No full media matrix was rerun.

With the candidate DLLs and the identical publisher executable, all 50 cycles
passed: exit 0, final handle/thread deltas 0/0, pending callbacks 0 and cleanup
wait 0 ms. The allowed growth remains two handles and zero threads. Existing
`local_track_published for unknown sid` warnings remain recorded; this change
does not claim to resolve that separate event-order observation.

The two Rust tests in `utils/debouncer.rs` verify release of a pending future and
cancellation/join of an already running future that never completes by itself.
Both passed in a small Cargo test harness importing that exact source module,
without building or running unrelated SDK tests.

Local evidence paths under `E:/syrnike13-build-cache`:

- `issue126-ci-34016936287-failed.log`
- `issue126-sdk8-churn-focused/{publisher.log,publisher.err,exit.json}`
- `issue126-sdk9-churn-focused/{publisher.log,publisher.err,exit.json}`
- `issue126-debouncer-tests.log`
- `issue126-build-sdk9.log` (built through `build.cmd`)
