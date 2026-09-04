# Verification method — the mutation harness

Register `VM-MUT`, its first file: the harness mechanics — the canary, the snapshot, the build,
the restore, the hashes. How a matrix's verdicts are read is in
[`mutation-verdicts.md`](mutation-verdicts.md). One method per heading; cite by ID. Router:
[`../verification.md`](../verification.md).

## VM-MUT-01 — Canary first

METHOD — Canary first: run one mutation hand-proven red before trusting any matrix; a matrix whose every row reads CAUGHT against a stale binary is the dangerous kind of wrong.
BECAUSE — a matrix whose every row reads CAUGHT against a stale binary is indistinguishable from
one that worked; a first run's canary once read BUILD-FAIL and every red after it meant nothing.
SEEN — nowhere yet

## VM-MUT-04 — Run the whole binary

METHOD — Run the WHOLE test binary, never a chosen list of suites: the pin that catches a mutation often lives somewhere else.
BECAUSE — choosing suites is choosing not to look; the pin that catches a mutation often lives
in a suite the mutation's author did not name.
SEEN — nowhere yet

## VM-MUT-05 — A timeout on every step, and the build's return code checked

METHOD — A timeout on every step and the build's return code checked; a timeout is RED; a verdict read through a pipe is no verdict (VM-LANE-16).
BECAUSE — a build that never ran reports the previous binary's counts as a clean result; a
wedged pump is a finding, not an unknown.
SEEN — nowhere yet

## VM-MUT-06 — A case count below baseline is TRUNCATED

METHOD — A case count below baseline is TRUNCATED whatever the summary says; identical counts across every mutation is the tell that nothing rebuilt.
BECAUSE — doctest prints its summary after the process dies partway, so a crash wears the
costume of a clean red: one row read as one case failed having run twenty of four hundred.
SEEN — nowhere yet

## VM-MUT-07 — A bad mutation has four costumes

METHOD — A bad mutation has four costumes — BUILD-FAILED, TRUNCATED, NOT-APPLIED, GREEN-unexpressible — and none is evidence: repair and re-run all four; after a harness repair re-run every non-red row.
BECAUSE — each costume is a mutation that never answered: an orphaned variable under `-Werror`,
a loop past a shorter vector, a pattern matching nothing, a scenario that cannot stage the attack.
SEEN — nowhere yet

## VM-MUT-08 — Snapshot the bytes, restore by rewriting them

METHOD — Snapshot the bytes at the start and restore by rewriting them: never a metadata-preserving copy (rebuilds nothing) and never `git checkout` (restores the predecessor's file); key backups by full path.
BECAUSE — a preserved mtime rebuilds nothing, so mutation N ran against mutation N-1's objects
under an all-CAUGHT matrix; a checkout over uncommitted work threw a phase's two headers away.
SEEN — nowhere yet

## VM-MUT-09 — Hash the artifact the mutation lands in

METHOD — Hash the artifact the mutation lands in, not the binary that runs; refuse a verdict when it did not change; hash the source before and after the edit and abort when the pattern matched nothing.
BECAUSE — a weave library is loaded, so the test binary is byte-identical across its mutations;
an editor that could not write on a 9p mount printed a verdict for every row in nine seconds.
SEEN — nowhere yet

## VM-MUT-10 — Rebuild after the restore, inside the harness

METHOD — Rebuild after the restore inside the harness and re-run the owning case, printing the restored green: a restored source is not a restored tree, and the next lane inherits the mutant otherwise.
BECAUSE — the next lane inherited the last mutation's binary and came back red with its exact
counts; a restore that is proved by a green is a restore, one that is assumed is a hope.
SEEN — nowhere yet

## VM-MUT-19 — A red for an incidental reason is not evidence

METHOD — A red for an incidental reason is not evidence: `(void)` every parameter the mutation orphans under `-Werror`, and rewrite the cut so the PIN is what reddens.
BECAUSE — a mutation that does not compile runs the previous binary and reads as a clean red; an
unused-parameter error after a deleted check says nothing about the property.
SEEN — nowhere yet

## VM-MUT-20 — Anchor mutations on code, never on prose

METHOD — Anchor mutations on code, never on prose: an apostrophe in a comment closes a shell string several mutations later; `bash -n` the script and check its exit code before trusting a matrix.
BECAUSE — an apostrophe in a comment inside a single-quoted shell string closed the string, and
bash died several mutations later after earlier rows had printed results that looked normal.
SEEN — nowhere yet

## VM-MUT-21 — Never kill a harness mid-run, never edit a running script

METHOD — Never kill a harness mid-run (its restore never runs) and never edit a running script (bash reads by byte offset): run from a frozen copy, and let it time out.
BECAUSE — a killed harness leaves a mutation applied in the working tree; an edit shifted every
later byte offset and bash executed fragments with a mutation still applied.
SEEN — nowhere yet

## VM-MUT-22 — Never run a restoring harness beside another lane

METHOD — Never run a restoring harness beside another lane on the same tree: a source-tree check reddens for a reason that is not your change; check for a background lane BEFORE touching a file.
BECAUSE — a background sanitizer lane read `doc_links` red while a one-off canary rewrote a
header in place; the run was discarded and the intact tree was green.
SEEN — nowhere yet

## VM-MUT-24 — A residue marker must be distinctive

METHOD — A residue marker must be distinctive (a field default cries wolf); restore after every mutation and verify the tree clean at the end with `git status` and a grep for the marker.
BECAUSE — a marker equal to a field default matched two honest sources and reported two
remaining on a clean tree; a marker that cries wolf trains the eye to skip the last check.
SEEN — nowhere yet

## VM-MUT-25 — A compiler cache does not weaken any of this

METHOD — A compiler cache does not weaken any of this: a hit replays the mutated object, never the baseline, and the artifact hash is the guard either way — cache freely, keep hashing.
BECAUSE — measured both ways with artifact hashes: identical verdicts with and without the
cache, and the no-rebuild probe was green with an unchanged hash in both; the hash is the guard.
SEEN — nowhere yet

## VM-MUT-26 — An in-place editor cannot write on a 9p mount

METHOD — An in-place editor that writes a work file and renames it cannot write on a 9p mount and the harness prints a verdict for an edit that never landed: write and truncate the same inode, fail loud on no match.
BECAUSE — twelve mutations, twelve unedited files, twelve BUILD-FAILED lines, indistinguishable
from twelve caught falsifiers to a skimmed log.
SEEN — nowhere yet
