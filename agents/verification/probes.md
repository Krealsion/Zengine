# Verification method — probes

Register `VM-PROBE`: runtime probes and instruments — where a counter is read, what an
instrument costs, what a reproduction has to print, and how a moved build is compared. One
method per heading; cite by ID. Router: [`../verification.md`](../verification.md).

## VM-PROBE-01 — A wall-clock probe reads both counters at the same point

METHOD — A wall-clock probe reads both compared counters at the SAME point in the dispatch stream: stamp the second at the arrival of the answer carrying the first, and the tolerance becomes derivable.
BECAUSE — the two numbers were sampled at different moments, so the compared quantity was the
difference of two jittering tails: off by one in one run of eight, past tolerance in one of sixty
under oversubscription, and red on a contended runner.
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
runs inside one, so the read is one beat stale; absorbed into the arithmetic, that is a suite
proving the wrong number and calling it exact.
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
BECAUSE — the collision needed the build log over its recompaction threshold at the moment the
case ran, and the forty cases before it each recompacted it back under; padding the log moved the
rate from one in two hundred to almost half.
SEEN — nowhere yet

## VM-PROBE-08 — Fidelity of a moved build is checkable

METHOD — Fidelity of a moved build is checkable, not arguable: `ninja -t commands` before and after with object paths normalised, token for token.
BECAUSE — all eight compile commands were identical token for token and both link lines differed
in exactly the object path; that is a stronger answer than any argument about flags.
SEEN — nowhere yet

## VM-PROBE-10 — Read the declared surface, not only the snapshot

METHOD — A weave's snapshot and its declared read surface are two questions; ask both of the running image, and where a field is materialized, COUNT the materializations and read that count too.
BECAUSE — a snapshot built on demand left the shape reads are answered from untouched, so a
document rode a reload intact while `path` and `text` read empty; the repair then rebuilt the
whole document on an arrow key with every returned byte identical. Output saw neither.
SEEN — `tests/test_workshop_panes_editor.cpp` case `"EDIT-W52: every field the pane advertises
reports what it is holding now"`, case `"EDIT-W64: the mirror is rebuilt when the bytes move and
at no other time"`; `tests/test_workshop_load.cpp` case `"RELOAD-3/VD-26: the reloaded pane reads
live, not out of the snapshot it revived from"`.

## VM-PROBE-11 — Do not move the simulated hand between two events of one poll

METHOD — Resolve a batch's geometry ONCE, before the first gesture is queued, and let the enqueue helpers spend those numbers: an observation that advances the schedule is not an observation.
BECAUSE — re-aiming per gesture hid the reflow the batch existed to catch, and it did the
re-aiming by READING the pane -- a poke, which drains -- so the batch delivered its own press
before the motion was queued, and proved two polls. Measured, on the committed witness.
SEEN — `tests/test_workshop_panes_editor.cpp` `EditorRig::aim`, `EditorRig::enqueue_press_doc`,
`EditorRig::admitted_caret`.

## VM-PROBE-12 — A cost is measured at the bound, on both shapes, and split from the baseline

METHOD — A cost claimed per operation is measured at the admitted bound on both document shapes -- one long line and many short lines -- and split into the instrument's own work and the work the owner did anyway.
BECAUSE — one increment on a four-thousand-line sample counted the materializations and said
nothing about what one costs beside the history's own snapshot, which is the same order of
work and was there all along; a count is not a cost.
SEEN — `tests/test_workshop_panes_editor.cpp` case `"EDIT-W64: the mirror is rebuilt when the
bytes move and at no other time"` (the count); the measurement rides with the phase record
(VM-WIT-24).

## VM-PROBE-09 — A precedent transfers only as far as its reason

METHOD — A precedent transfers only as far as its reason: the neighbouring fixture tree is prepared at build time because a suite binary depends on it; copy the reason, not the shape.
BECAUSE — build-time preparation of the fixture tree left a real hole in a lane that has no
target to hang it from; configure time, once, has no build order that can defeat it.
SEEN — `tests/CMakeLists.txt` `zengine-build-fixture`.
