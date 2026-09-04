# Verification method — probes

Register `VM-PROBE`: runtime probes and instruments — where a counter is read, what an
instrument costs, what a reproduction has to print, and how a moved build is compared. One
method per heading; cite by ID. Router: [`../verification.md`](../verification.md).

## VM-PROBE-01 — A wall-clock probe reads both counters at the same point

METHOD — A wall-clock probe reads both compared counters at the SAME point in the dispatch stream: stamp the second at the arrival of the answer carrying the first, and the tolerance becomes derivable.
BECAUSE — the two numbers were sampled at different moments, so the compared quantity was the
difference of two jittering tails: off by one in one run of eight, and worse under load.
SEEN — `tests/test_audit_probes.cpp`.

## VM-PROBE-02 — Remove the skew, never widen the number

METHOD — A fixed tolerance cannot be honest about the difference of two jittering tails: remove the skew, never widen the number; a probe green locally has been tested on the wrong machine.
BECAUSE — widening the number is the repair that makes CI green and the tolerance a hope; with
the skew removed the margin is derivable from single-threaded FIFO and one parked beat.
SEEN — `tests/test_audit_probes.cpp`.

## VM-PROBE-03 — Bracket a suspect translation edge with two probes

METHOD — Bracket a suspect translation edge with two probes — the reader's raw fields and the dispatch entry — so one run says whether the wire or the interpretation is false.
BECAUSE — the reader's raw scancode plus a synthetic modifier release proved the OS delivered a
different key than the hand pressed, in one look.
SEEN — nowhere yet

## VM-PROBE-04 — An instrument has a cost: measure it, name it, pin it

METHOD — An instrument has a cost: measure it, name it as a constant, and pin it by asserting the service's own counter rather than absorbing it into the arithmetic.
BECAUSE — a read of the service's clock costs queue turns and the beat parked behind the ask
runs inside one, so the read is one beat stale; absorbed, that is a wrong number called exact.
SEEN — `tests/test_timer.cpp` `kRulerTrailingBeats`.

## VM-PROBE-05 — The probe's own process is a variable

METHOD — The probe's own process is a variable: ask what the production caller's process already contains and start there (a C loader and a C++ host answer a retention question differently).
BECAUSE — the same artifact read retained under a C loader and released under a C++ one in the
same minute, because the first C++ library a C process opens drags the runtime in and it stays.
SEEN — nowhere yet

## VM-PROBE-06 — A reproduction with no diagnostic has not reproduced anything

METHOD — A reproduction that produces no diagnostic has not reproduced anything: if START does not print the sentence the prompt quoted, the environment is wrong, and the attempt is discarded.
BECAUSE — a driver reports a child's failure, and a child that cannot start has no message to
relay, so the driver exits one in silence, which looks exactly like a repository-specific failure.
SEEN — nowhere yet

## VM-PROBE-07 — A flake that needs the full lane's mix is usually state, not load

METHOD — When a flake needs the full lane's mix, look for state earlier work leaves behind (a log over a threshold), not for load: pad the state and the rate moves.
BECAUSE — the collision needed the build log over its recompaction threshold when the case ran,
and the cases before it each recompacted it back under; padding the log moved the rate to half.
SEEN — nowhere yet

## VM-PROBE-08 — Fidelity of a moved build is checkable

METHOD — Fidelity of a moved build is checkable, not arguable: `ninja -t commands` before and after with object paths normalised, token for token.
BECAUSE — all eight compile commands were identical token for token and both link lines differed
in exactly the object path; that is a stronger answer than any argument about flags.
SEEN — nowhere yet

## VM-PROBE-09 — A precedent transfers only as far as its reason

METHOD — A precedent transfers only as far as its reason: the neighbouring fixture tree is prepared at build time because a suite binary depends on it; copy the reason, not the shape.
BECAUSE — build-time preparation of the fixture tree left a real hole in a lane that has no
target to hang it from; configure time, once, has no build order that can defeat it.
SEEN — `tests/CMakeLists.txt` `zengine-build-fixture`.
