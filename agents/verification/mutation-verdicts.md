# Verification method — reading a mutation matrix

Register `VM-MUT`, its second file: what a verdict means — a green, a mask, a crash, a refusal
at compile time — and what to do with each. The harness mechanics are in
[`mutation.md`](mutation.md). One method per heading; cite by ID. Router:
[`../verification.md`](../verification.md).

## VM-MUT-02 — Predict the blast radius before running

METHOD — Predict how many cases a mutation should redden before running it; a mismatch is a broken mutation, never a finding about coverage.
BECAUSE — a canary meant to remove occlusion entirely disabled one branch of three and came back
a clean red on exactly one case; only reading which case failed said the mutation under-applied.
SEEN — nowhere yet

## VM-MUT-03 — A green canary is believed

METHOD — A green canary is believed: investigate the pin, not the harness, and make exactly one term able to decide.
BECAUSE — it may be saying the pin passes for a reason its author did not intend, a second check
masking the one under test; equalize the other variables so one term is the only barrier left.
SEEN — nowhere yet

## VM-MUT-11 — A CAUGHT names the assertion that moved

METHOD — A CAUGHT names the assertion that moved, never the suite; split the log on doctest's rules and keep the ERROR blocks, because `TEST CASE:` also heads any case that emitted a MESSAGE.
BECAUSE — four of fifteen verdicts once named suites the mutation had not touched, and one row's
evidence was entirely the previous row's; a wrong suite is visibly absurd, a wrong row is not.
SEEN — nowhere yet

## VM-MUT-12 — A static_assert that refuses a mutant is a catch of a different kind

METHOD — A `static_assert` that refuses a mutant is a catch of a different kind: relax it and mutate again, or the report cannot tell a doubled guard from a sole one.
BECAUSE — four of fourteen mutations were refused by three assertions written to pin a
composition; relaxing one and mutating again showed the type system and a case both stop it.
SEEN — nowhere yet

## VM-MUT-13 — Express the behaviour, not the deletion of a line

METHOD — Express the BEHAVIOUR, not the deletion of a line: an inserted branch must be armed, a deleted check often leaves a second refusal, and a real mutation undone downstream is restated where it is observable.
BECAUSE — an inserted branch whose memo was never assigned compiled, applied and changed
nothing; a wrong index survived because the next clamp put it back onto the right answer.
SEEN — nowhere yet

## VM-MUT-14 — MASKED is not a hole

METHOD — MASKED is not a hole: cut both halves in one mutation to show the pair is load-bearing; a paired cut that stays green means more than two layers — count and name every layer.
BECAUSE — one capture rule survived a single and a double cut because three mechanisms were
refusing; a report names every layer or it reports a load-bearing term as dead.
SEEN — nowhere yet

## VM-MUT-15 — UNWATCHED is not redundant

METHOD — UNWATCHED is not redundant: before calling a surviving term redundant, ask which INPUT no case produces and whether the machinery can even run in the state the case leaves it; the fix is a case.
BECAUSE — an aliveness check survived deletion because every case killed and revived before
pumping; a guard's deletion passed because its assertions sat behind a full socket.
SEEN — nowhere yet

## VM-MUT-16 — A mask is usually a hole in an older suite

METHOD — A mask is usually a hole in an OLDER suite: close it by arranging the missing condition, and expect that case to be the cheapest defect-finder you have.
BECAUSE — three of seventeen masks each closed an earlier phase's under-proven claim at one case
and one re-run apiece; one such case went red on the pristine tree and found a real defect.
SEEN — nowhere yet

## VM-MUT-27 — The third verdict is respelled at the property

METHOD — A mutation that removes redundancy rather than a property is a third verdict beside caught and hole, UNEXPRESSIBLE: no arrangement can distinguish it, so respell it at the property, never arrange a condition.
BECAUSE — deleting a belt whose door is another function left mutant and pristine
indistinguishable; respelled at the property the belt guards, the target was a measurement again.
SEEN — nowhere yet

## VM-MUT-17 — A case on a handler's deliberate false path proves nothing about the handler

METHOD — A case that lands on a handler's deliberate `false` path proves nothing about the handler: find the consuming path by pressing for it, then arrange the contest.
BECAUSE — the cell the case pressed was a row the handler declines by design, so the press fell
through and the hoisted arms were invisible; the declining paths are the commented ones.
SEEN — nowhere yet

## VM-MUT-18 — Check which suite the row aimed at before calling a green a hole

METHOD — Before calling a green mutation a hole, check which suite the row aimed it at and which fixture reaches the mutated line; the two are told apart by reading, never by re-running more suites.
BECAUSE — two greens of fourteen: one a genuine hole closed by a case, one a multiply pointed at
a suite whose fixture never reaches it; the diagnosis found a third mutation nobody had written.
SEEN — nowhere yet

## VM-MUT-23 — A crashing mutation needs a separate semantic pin

METHOD — A crashing mutation needs a separate semantic pin — the focused suite and the sanitizer, reported together — and a canary must be surgical and non-crashing.
BECAUSE — a mutation that truncated the whole-binary run proved nothing about the red path; the
focused suite failed without a crash and the sanitizer named the use-after-free.
SEEN — nowhere yet
