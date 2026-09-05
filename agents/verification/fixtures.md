# Verification method — fixtures

Register `VM-FIX`: writing a case that can fail — a bound, an oracle, a repeat, a custody claim,
a durable format, a key. One method per heading; cite by ID. Router:
[`../verification.md`](../verification.md).

## VM-FIX-01 — A claim about a bound needs the range

METHOD — A claim about a BOUND needs the range, not a point: sweep it as a property, and run the real screen at the supported minimum; one arranged case proves that arrangement.
BECAUSE — thirty cases and twenty-four caught mutations were green while the shipped terminal at
its minimum width of 78 columns showed the marker cut; the crowded case stopped three tabs short
of the boundary.
SEEN — `tests/test_workshop_panels.cpp` case `"HD-7: the sharing policy is monotonic, bounded
and never starves either list"`.

## VM-FIX-02 — A bound with slack is asserted against the shape's own constants

METHOD — A bound with SLACK cannot be falsified by a measurement: assert a derived bound against the shape's own constants, and keep the measured-instance case beside it as evidence about today.
BECAUSE — a maximal legal file fit under both the old and the new ceiling because the per-field
bound carries an order of magnitude of slack, so the mutation reverting the derivation survived.
SEEN — `tests/test_workshop_persistence.cpp` case `"WUX-10: a session may hold as much as it may
hold, and be read back"`.

## VM-FIX-03 — A surviving falsifier is a finding about the cases first

METHOD — A surviving falsifier is a finding about the CASES first: write the case, re-run, report both facts; eliminate the harness before reporting a line as not load-bearing.
BECAUSE — both survivors of one matrix were cases: a derivation nothing pinned, and a guard no
case pressed; true by construction and not yet pinned is a state, not a verdict about the line.
SEEN — `tests/test_workshop_document.cpp` case `"WUX-11: an action with no default gesture
answers to no key, and says so"`.

## VM-FIX-04 — A matrix cannot find what the code and the cases agree on

METHOD — A mutation matrix cannot find a defect the code and the cases agree about: it only asks whether the cases notice a CHANGE, so all-caught means the cases bind the code, never that the code is right.
BECAUSE — the case and the code were both wrong at the same point, so every mutation of that
point was caught and the matrix read twenty-four of twenty-four over a shipped defect.
SEEN — nowhere yet

## VM-FIX-05 — A uniform fixture collapses two laws into one sentence

METHOD — A UNIFORM fixture collapses two laws into one sentence; ask what pair of assertions the uniformity makes indistinguishable and inject a non-uniform instance (no value a multiple of another).
BECAUSE — under a uniform width "this line fits" and "this line is N codepoints" are one
sentence; a proportional metric caught two production defects that all nine uniform cases pass.
SEEN — `tests/test_maker.cpp` case `"FC-4: the answer lands in the named state field, and an
answer of another kind is refused with the state unchanged"` (`label` first, so a write by
position is caught).

## VM-FIX-06 — A fixed population is derived from a capacity the code owns

METHOD — A test that needs a fixed population derives it from a capacity the code owns and states its own oracle, never calling the function under test to compute its expectation.
BECAUSE — a census in a case costs every future author two red cases and a decision about
whether they broke something; a case that calls the function under test agrees with itself.
SEEN — `tests/test_workshop_panes_seam.cpp` case `"the combined catalog stops at thirty-two
entries, built-ins included"`.

## VM-FIX-07 — Capture the prior fact, then anchor it

METHOD — Capture the prior fact, then ANCHOR it to the constant, or the comparison is circular; a census (`kPanelKinds == 2`) is ceremony aimed at the next author — claim over the population instead.
BECAUSE — without the anchor, reversing the built-in order would have passed; the prefix is
captured from the catalog and then pinned field for field against the compile-time table.
SEEN — `tests/test_workshop_panes_seam.cpp` case `"the runtime catalog is beside the
compile-time one and never inside it"`.

## VM-FIX-08 — A repeated staging converges on the current source

METHOD — A repeated staging converges on the CURRENT source: remove, then copy; the witness changes the source between the two calls, since staging the same bytes twice proves only that the second call did not error.
BECAUSE — "calling it twice changes nothing" is the definition a repair drifts toward because
doing less work satisfies it; the artifacts a fixture stages are rebuilt between runs.
SEEN — `tests/test_workshop_load.cpp` case `"a repeat CONVERGES the destination onto the current
source, not the old one"`, `Stage::put`.

## VM-FIX-09 — One canary per kind of wrong

METHOD — One canary per KIND of wrong: a platform-faithful canary and a portable one, both run from a cleared stage in a single process so a tidy machine cannot satisfy them.
BECAUSE — the platform-faithful canary reddens only where its STL quirk lives; the portable one
reddens only the converging case everywhere, and that split is the argument for two cases.
SEEN — `tests/test_workshop_load.cpp` case `"staging over what a previous run left behind is an
ordinary repeat"`, case `"a repeat CONVERGES the destination onto the current source, not the old
one"`.

## VM-FIX-10 — Clearing a directory before a lane is a ritual

METHOD — Clearing a directory before a lane is a ritual, not a repair: the contract moves into an unwritten precondition and the stale-bytes hazard survives.
BECAUSE — deleting the stage hides the symptom while a refused copy still leaves the old
artifact in place, and the green starts proving something about the directory as much as the code.
SEEN — nowhere yet

## VM-FIX-11 — A case that stops at a gesture's base witnesses what it remembered

METHOD — A case that stops at a gesture's recorded base witnesses what it REMEMBERED, not what it wrote: add the motion and the release, assert the authored amount, and assert the unnamed axis unchanged.
BECAUSE — the mutation that made the base read the visible rectangle reddened four assertions,
two of them the authored results the earlier case had stopped short of.
SEEN — `tests/test_workshop_screen.cpp` case `"WIND-2a: a clipped default resize begins from the
full resolved size"`.

## VM-FIX-12 — A refusal being said is not evidence the write was atomic

METHOD — A refusal being SAID is not evidence the write was atomic: assert the whole row through a defaulted `operator==`, the cheapest complete no-write witness there is.
BECAUSE — the mutation that committed the legal axis before judging the other still printed the
correct refusal; the pane was simply already wider, and only the whole row saw it.
SEEN — `workshop/setup.hpp` `SetupPane`; `tests/test_workshop_screen.cpp` case `"WUX-2: a
refused anchored resize writes neither the place nor the size"`.

## VM-FIX-13 — An assertion that searches a composed row asks about every fact on it

METHOD — An assertion that SEARCHES a composed row asks about every fact composed onto it, platform spellings included: aim it at the value's own span, never at the line.
BECAUSE — a search for a backslash on the band was true about the claim and false about the row
on Windows, where the setup file's path shares the row and spells its separators that way.
SEEN — `tests/test_workshop_persistence.cpp` case `"QR-15: a name that could impersonate the
setup line is one SPAN on it"`.

## VM-FIX-14 — A content-sized surface's anchor needs a band that holds the content

METHOD — A fixture for a content-sized surface's anchor needs a screen whose band holds the whole content; derive every fixture and witness expectation from the content, the room and the face's line height.
BECAUSE — under a room-under-the-anchor law a fixture proved the anchor by taking fewer rows;
under content sizing every anchor reads the same wherever the band is shorter than the content.
SEEN — `tests/test_workshop_screen.cpp` case `"ARR-0: entering a group stays at the anchor, and
the popup resizes to it"`.

## VM-FIX-15 — Custody is falsified by changing the one value under live consumers

METHOD — Custody is falsified by a fixture that changes the ONE value underneath live consumers plus an address check — the same object, not equal contents — because a copy and a read answer alike until then.
BECAUSE — a copy and a read give the same answers forever, so an ordinary green proves nothing;
only replacing the value underneath, and asking for the same object, tells them apart.
SEEN — `tests/test_builder.cpp` case `"PROJ-0: neither build participant keeps a catalog of its
own"`.

## VM-FIX-16 — A mutant in main is reachable only structurally

METHOD — A mutant in `main()` is reachable only structurally, because no case can run a `main` that claims a terminal: report the source tripwire as a tripwire, never as a behavioural catch.
BECAUSE — the source tripwire is the honest ceiling for a value declared in `main`; reporting it
as a behavioural catch overstates the suite.
SEEN — `tests/test_population.txt`.

## VM-FIX-17 — Assert that the work happened before asserting what it said

METHOD — Assert that the work HAPPENED (`status == 0`) before asserting anything about what it said: a child that died before building still ends, ends once, and attributes correctly.
BECAUSE — a case asserted operation identity and output text and never the status, so a child
that died before building reported only a missing first line, for five weeks.
SEEN — `tests/test_builder.cpp`; `tests/test_maker.cpp` case `"FC-8: the definition and the
state are two native files written by one process, and a fresh process reads them back with
high == 7"`.

## VM-FIX-18 — Guard an index in the same keystroke

METHOD — Guard `[0]` and `.back()` with `REQUIRE` in the same keystroke: a mutation that empties the container otherwise detonates the run and hides every later result.
BECAUSE — two such guards turned forty-one and twenty-eight reported red cases into a hundred
and forty and a hundred and thirty-two; the crash arrives exactly when a mutation empties the
container, and hides every result after it.
SEEN — nowhere yet

## VM-FIX-19 — Before widening a key's fallthrough, read the no-op cases as law

METHOD — Before widening a key's fallthrough, grep the suites for cases that pin the key as a NO-OP and read them as law; a place a maker types into keeps the key while it holds the keys.
BECAUSE — two shipped cases went red the moment Escape's fallthrough widened to every context:
an editor case pinning Escape as nothing, and a seam case that types after it.
SEEN — `tests/test_workshop_editor.cpp` case `"EDIT-0: Escape means nothing in the editor -- no
mode closes, no text moves"`; `tests/test_workshop_document.cpp` case `"TEXT-0: the real
Composer's fields speak the vocabulary across the seam"`.

## VM-FIX-20 — A historical content id is pinned with its provenance

METHOD — A historical content id comes from a file the predecessor wrote, else from the accepted head compiled in a worktree; pin every historical id as a case with its provenance beside it.
BECAUSE — a retired struct copied one field out of order changes the content id, and admission
then refuses every file the extraction existed to keep readable, with no compile error.
SEEN — `tests/test_workshop_persistence.cpp` case `"WUX-10/SC-3: a retired shape's wire identity
is the identity it was written at"`.

## VM-FIX-21 — Aim a version test at the envelope, never at the bare field

METHOD — Once a parent's version outruns a nested child's, `format_version` is not a unique token in the file: aim a test at the envelope's own version, never at the bare field.
BECAUSE — a session at version four nests desks that still say three, so a test searching for
the bare field finds the nested one; two cases were doing exactly that.
SEEN — `tests/test_workshop_persistence.cpp` case `"WUX-0 D: a malformed session costs the desk
and nothing else"`, case `"WUX-0 D/MIG-0: an unreadable session names its version by NUMBER"`.

## VM-FIX-22 — A test can lean on a false-positive diagnostic

METHOD — A test can lean on a false-positive diagnostic without saying so: when a diagnostic stops firing, grep for what was reading it before believing the count.
BECAUSE — an audit's only evidence that one shape had been spoken was the refusal of a
publication nobody accepted; remove the false positive and it counts one fewer and still passes.
SEEN — nowhere yet
