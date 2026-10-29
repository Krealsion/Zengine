# The mutants

A mutant is a small wrong change to the code that a case claims to catch. The repository keeps
every mutant its cases' comments claim, here, as code, so a claim can be run again by anyone and
is run every week: a case that stops catching what its comment says is found. The method is the
verification register's, [the harness](../../agents/verification/mutation.md) and [reading its
verdicts](../../agents/verification/mutation-verdicts.md).

## The corpus

One file a test file: `tests/mutants/<suite>.md` holds the mutants that `tests/test_<suite>.cpp`
claims, one entry a mutant. The claiming comment names its entry by path and heading,
`// ⚔ MUTATION (tests/mutants/<suite>.md#<id>): ...`, and a comment claiming several entries of one
file names each, `(tests/mutants/<suite>.md#<id>, #<id>)`. An entry reads:

````markdown
## <id>

What the mutant does, in a sentence.

In `<file>`:

```diff
 a line that stands as it is
-a line the mutant replaces
+its replacement
```

Red in `<program>`:

- a case's whole name, as its `TEST_CASE` spells it, a long one wrapped with its next lines
  indented two spaces
````

- **The id** is lower-case words joined by dashes, and is the heading's anchor.
- **A hunk** names its file by an `In` line before it. Its space and `-` lines stand once in that
  file, as whole lines; several hunks, in one file or in several, make one mutant.
- **On code, never on a comment** ([VM-MUT-20](../../agents/verification/mutation.md#vm-mut-20--anchor-mutations-on-code-never-on-prose)):
  a hunk that changes only comments is stale. A changed line opening with `*` counts as a comment
  only inside a block comment the hunk itself opens, since a dereference opens with `*` too. Where
  the cases read a file as text, the entry says
  ``The cases read `<file>` as text.``, and that file is the artifact the mutant lands in
  ([VM-MUT-32](../../agents/verification/mutation.md#vm-mut-32--a-file-the-case-reads-at-runtime-is-the-artifact)).
- **Its context names the function.** A hunk's space lines hold a line of the function the
  entry's sentence names -- its signature, or a line no other function holds -- so the text found
  once is found there. The runner checks only that the text stands once in the file: a line two
  functions share can land the mutant in the wrong one, which no case of the right one catches.
- **Every case named goes red.** ``Red in `CTest`:`` names a CTest entry instead, by its exact
  name in backticks, run as `ctest` runs it.

## The runner

`tests/mutants/run.py`, Python 3.8 or newer on Linux, over a Ninja tree configured as the official
lane's:

```sh
python3 tests/mutants/run.py --list                                    # each mutant, and who claims it
python3 tests/mutants/run.py --build build --only launcher.md#nothing-closed    # one mutant
python3 tests/mutants/run.py --build build --corpus launcher.md        # one file's mutants
python3 tests/mutants/run.py --build build --shard 3/8                 # a piece of the whole
python3 tests/mutants/run.py --gather <out>... --summary <file.md>     # pieces' verdicts, together
```

It builds the tree, and runs every case its mutants name, which must be green before any mutant is
written. For each mutant, in turn: the text found once, its bytes written over the file's own and
read back; the programs its cases run built, with every library and every program no CTest entry
runs as a suite, each step under a timeout and its exit checked; an object compiled again from the
changed file, and changed by it; each case run as CTest runs its program; the file's first bytes
written back. Then the whole tree is built again, every case is green again, every object a mutant
compiled is back to its first bytes, and `git status` is what it was.

- **caught**: every case the mutant names went red -- a failure, a timeout or a crash -- or a
  `static_assert` refused it at compile time.
- **survived**: a case it names stayed green. The claim is not true of the code as it stands; that
  is a finding about the case, and no case is changed to make it go red.
- **stale**: it no longer applies -- its text is not in its file once, it does not build for
  another reason, it changes nothing compiled, a case it names selects nothing, or no comment
  claims it.

It exits 0 when every mutant was caught, 1 when one survived or is stale, and 2 when the run
cannot be believed: the corpus does not read, a case is red before any mutant, a restore did not
hold, or the run was stopped (a stopped run still writes back the mutant it was on). It writes
`results.json`, `table.md` and a log of every step under `--out` (the build tree's `mutants/` by
default). It edits the source tree it is run from, so it never runs beside another lane on that tree
([VM-MUT-22](../../agents/verification/mutation.md#vm-mut-22--never-run-a-restoring-harness-beside-another-lane)),
and it refuses a second runner on one build tree.

## Probing a survivor

A survivor says the case does not catch what its comment claims, and why is found with the mutant
in, before any case is changed: write the hunk's `+` lines over the file by hand, build the program
its cases run, run the one case alone with doctest's `--success` (or a print it does not keep), and
read what the case asserted against what the claim needs. Then `git checkout -- <file>`, and build
again. What the probe found decides the entry: the case made to assert the claim, so the mutant
goes red; the comment's claim moved to the case that does catch it; or the entry dropped, its reason
in the pull request -- the mutant changes nothing a case can see, or it removes a second guard the
case cannot tell from the first. Each kept entry is run again with `--only` before the push.

## A pull request's mutants

A case whose comment claims a mutant commits the mutant here, in its test file's corpus file, the
comment pointing at the entry. Each new entry is run alone with `--only`, red, before the push, as
a canary is ([VM-MUT-01](../../agents/verification/mutation.md#vm-mut-01--canary-first)), and the
pull request's body cites it by path.

## The weekly run

[`.github/workflows/mutants.yml`](../../.github/workflows/mutants.yml) runs the whole corpus on
`main` every week, and on request, in pieces that each build the tree and run their share. It holds
no pull request: when a mutant survives or no longer applies, the run is red and says which, each
with why, in one open issue, `The weekly mutants: what survived or no longer applies`, which it
opens or adds to.
