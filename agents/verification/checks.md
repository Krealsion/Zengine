# Verification method — checks

Register `VM-CHECK`: a check that reads the tree, and a pass that rewrites it — the self-test,
the grep, the four characters, the clock, the sheet and its applier. One method per heading;
cite by ID. Router: [`../verification.md`](../verification.md).

## VM-CHECK-01 — A tree-reading check self-tests before it answers

METHOD — A tree-reading check self-tests before it answers: its predicate says NO to a token that is absent and YES to one that is present, and an empty population is a red, never a quiet pass.
BECAUSE — a well-formed tree and a checker that finds nothing produce byte-identical output; an
expectation of nothing is satisfied by anything.
SEEN — `tests/check_doc_links.cmake`; `tests/check_package_vocabulary.cmake`;
`tests/check_law_register.cmake`.

## VM-CHECK-02 — Grep code, not comments

METHOD — Grep code, not comments: a check that asks whether a file declares or spends a name strips `//` and `/* */` first and matches a whole token — `Rect` is not inside `SurfaceRect`.
BECAUSE — twenty-four wrong attributions survived three register steps on a whole-file grep, and
fourteen remained once comments were stripped.
SEEN — `tests/check_law_register.cmake` `zen_law_token_in`.

## VM-CHECK-03 — A CMake script that splits text has four characters to fear

METHOD — A CMake script that splits text into a list has four characters to fear — `;`, `[`, `]`, `\` — and the swap is stated once; copy a `CMAKE_MATCH_n` into a variable before a nested loop rewrites it.
BECAUSE — one unbalanced bracket welds every later line into one element, measured on both CMake
versions this repository configures with; a nested match rewrites the enclosing scope's captures.
SEEN — `tests/check_law_register.cmake` `ZEN_SOH`.

## VM-CHECK-04 — A tree-reading check's wall clock is its filesystem's

METHOD — A tree-reading check's wall clock is its filesystem's: quote the tree's filesystem with the number.
BECAUSE — the same script measured 9.75 s on a 9p-mounted tree, 1.99 s on ext4 and 2.5 s on the
Windows host; a number quoted without its filesystem says nothing.
SEEN — nowhere yet

## VM-CHECK-05 — A mass edit goes sheet, applier, proof

METHOD — A mass edit goes sheet → applier → proof, regenerated from the start commit: the sheet is the review, the applier converges from a clean checkout, and the proof is mechanical and travels with the commit.
BECAUSE — the sheet is the review, one row per declaration; an applier that starts from the
working tree drifts on a rerun, and a proof that is not mechanical does not travel with the
commit.
SEEN — `tools/workshop-split/apply.py`; `tools/workshop-split/prove.py`.

## VM-CHECK-06 — A sheet carries the whole pointer group; an index is stale at the first rewrite

METHOD — A per-line sheet carries the whole pointer group above a declaration, and an index over a file is stale at its first rewrite: resolve fresh per lookup or re-index after every write.
BECAUSE — a flagged line's neighbour is a section banner or a content citation and the decision
is about the group; a stale index marks good lines for dropping, which is judgment-shaped, not
tool-shaped.
SEEN — `tools/workshop-split/apply.py`.

## VM-CHECK-07 — A verification grep is scoped to sources

METHOD — A verification grep is scoped to sources, or it greps its own conclusions; when a number surprises you, read the hits before believing the number or the claim.
BECAUSE — an unscoped grep for an absent primitive reported three hits, all prose in a report
naming the primitives in the sentence claiming none appear; the temptation is to fix a true claim.
SEEN — `tests/check_package_vocabulary.cmake`.

## VM-CHECK-08 — Never name a probe or build directory after the thing you prove absent

METHOD — Never name a probe or build directory after the thing you are proving ABSENT: a directory's name is embedded in nearly every generated file, and the decisive grep then lies.
BECAUSE — trees named after the thing being proven absent made the decisive grep report dozens
of hits, every one the directory path, in a phase whose whole claim was zero.
SEEN — nowhere yet

## VM-CHECK-09 — A generated line is regenerated, never hand-edited

METHOD — A generated line is regenerated, never hand-edited: a record's Laws supported is filled from the registers' WHY lines by the tool under `tools/`, and the check asserts the two agree.
BECAUSE — a hand-edited generated line is the drift the generator exists to end; the check
asserts each record lists exactly the laws whose WHY names it, and the tool writes that list from
the WHY lines.
SEEN — `tools/fill_laws.sh`; `tests/check_law_register.cmake`.

## VM-CHECK-10 — A retired spelling may appear in exactly one file

METHOD — A retired spelling may appear in exactly one file, the checker that declares it, and the check asserts both halves of that.
BECAUSE — a spelling that may appear anywhere is a spelling nobody retired; one that may appear
nowhere cannot be declared, so the checker is the one file that carries it, and the check asserts
both halves.
SEEN — `tests/check_package_vocabulary.cmake`.

## VM-CHECK-11 — Every current-facing text file is read whole for a path outside the repository

METHOD — Every current-facing file of a text kind is read whole for a path outside the repository; the spellings live in the check alone, and the remedy is words.
BECAUSE — a leak is a substring wherever it sits, comment or code, so files are read raw; a
standalone clone has no sibling, and a check that passes only in one workspace layout cannot be
run by a consumer.
SEEN — `tests/check_doc_links.cmake` `ZEN_DOC_OUTSIDE_SPELLINGS`.

## VM-CHECK-12 — A body move out of a header goes sheet, applier, proof

METHOD — A body move out of a header goes sheet → applier → proof: the sheet names each body and its rewrites, the applier regenerates from the start commit, and the proof diffs bodies, literals and registers.
BECAUSE — four hundred and fifteen bodies moved under a proof of four checks and exit zero; the
residues' rerun had to regenerate from a later commit, because the split's applier would rebuild
what the residues filed.
SEEN — `tools/workshop-split/apply.py`, `tools/workshop-split/prove.py`,
`tools/workshop-split/sheet.tsv`; `tools/workshop-residues/apply.py`,
`tools/workshop-residues/prove.py`.

## VM-CHECK-13 — A qualified spelling names a declaration only from inside its scope

METHOD — A qualified spelling `Scope::name` names a declaration only from inside `Scope`, read by a scope tracker over the braces; the self-test pins the twin refused above a free function of that name.
BECAUSE — a suffix match let a wrong `Struct::name` sit green for a phase; run with scope
tracking against the main branch's tip, the rule caught six: four a parser gap (a template
specialization's opener) and two wrong spellings.
SEEN — `tests/check_law_register.cmake` `zen_law_pointer_names`, `zen_law_scope_step`.
