# Taking an issue

**Contributing.** From an issue labelled `ready` to a pull request ready to merge, for a
contributor whose only source is a clone of this repository. Each step says what to do and routes
to the page that owns the detail. [Best practices](best-practices.md) says what good work looks
like along the way. The commands use GitHub's command-line client, `gh`, signed in and run inside
the clone; the GitHub web pages do the same.

## 1. Choose a `ready` issue

Bounded work is an issue written as a small brief. Only the `ready` label means the work is
verified, bounded and open to take; `gh label list` says what every other label means.

```sh
gh issue list --label ready
gh issue view <n> --comments
gh issue view <n> --json closedByPullRequestsReferences     # a pull request that already fixes it
```

A `ready` issue has four parts, and each one binds:

- **What is wrong**: the defect, and how it was reproduced.
- **Done when**: the outcome and the evidence that shows it. The pull request answers each line.
- **Fences**: what the change must not do or reach. They bind as firmly as the outcome. Work
  beyond them belongs to another issue, raised in a comment, never to a wider change.
- **Checked against**: the commit the issue was reproduced on.

One issue is one branch and one pull request. Terms for contributions from outside the project
are in [CONTRIBUTING.md](../../CONTRIBUTING.md#code-contributions).

**The issue's comments are its record.** Read them before taking it: the work is claimed and
followed there, and reconstructed from there later. A taker says each step as a comment:

| when | the comment |
|---|---|
| on taking the issue | `Research for fix in progress` |
| before building the change | `Proposed idea`, with the approach |
| on pushing it | `Fix pushed, awaiting CI`, with the pull request |

```sh
gh issue comment <n> --body "Research for fix in progress"
```

A taker that stops says so on the issue, and why. An issue whose last such comment is a day old,
with no pull request, may be taken over by a comment that says so.

## 2. Start from current `main`

```sh
git fetch origin
git switch -c <area>/<what> origin/main
```

Reproduce the issue on that commit before changing anything: *checked against* may name an older
one. If it no longer reproduces, nothing needs repairing; say so on the issue, naming the commit
you checked, and stop. If it rests on work that has not merged, say which, and build on it only
as the issue says. Name the branch for its area and its outcome, as `desk/pane-identity` or
`workshop/desk-read-bytes`.

Without push access to this repository, fork it with `gh repo fork --remote`, which makes the
fork `origin` and this repository `upstream`. Then read `origin/main` below as `upstream/main`,
fetched with `git fetch upstream`, give the change-kind check and the documentation lane
`-DZEN_BASE=$(git merge-base HEAD upstream/main)`, and open the pull request with
`--repo Krealsion/Zengine`. A first pull request from a fork runs its checks once a maintainer
approves them.

## 3. Read what owns the surface

[AGENTS.md](../../AGENTS.md) is the one mandatory read. Its table routes each surface to the
document that owns its law: read that document, and the pages the issue links, before designing
the change. Where the issue and the law disagree, trace both: correct the code, test or prose that
is wrong inside the fences, and take a change to the law itself to its owner, in a comment on the
issue, before building it
([intent, evidence, and architectural fit](../../AGENTS.md#intent-evidence-and-architectural-fit)).

## 4. The environment

[Supported toolchains](supported-toolchains.md#the-matrix) says which compilers and platforms
build this repository; Linux with GCC is the canonical lane. Beyond a compiler and CMake, a build
needs:

- **An installed Loom.** Zengine consumes Loom as a package: clone
  [Loom](https://github.com/Krealsion/Loom) beside this checkout, then build and install it as
  [the default configuration](build-and-test.md#the-default-an-installed-loom-package-zen_loom_devoff)
  shows. The tests need that Loom's kernel, which on Windows is an opt-in
  ([the population contract](../../AGENTS.md#the-population-contract)).
- **The network at configure**, unless SDL3 and SDL3_ttf are installed, for the SDL skin's pinned
  fetch; or `-DZENGINE_SDL_SKIN=OFF` ([SDL](supported-toolchains.md#sdl)).

Two optional tools each open a gate of the population: Python, beside a Loom installed with its
session tools, opens `session`, and a Neovim named by `-DZENGINE_NEOVIM_PROGRAM=<path>` opens
`neovim` ([the versions it needs](../../editor/docs/neovim.md#before-you-start)). The lane prints the
gates it ran under, and an entry whose gate is shut is declared absent, never passed
([the population file](../../tests/test_population.txt)).

A change that `tests/check_change_kind.cmake` calls documentation-only needs none of this: the
documentation lane needs CMake and Git.

## 5. Build

The commands are in
[AGENTS.md](../../AGENTS.md#build--test-canonical-wsl-consumes-an-installed-loom), and every
configuration is in [build and test](build-and-test.md). Configure a tree once; after each edit,
`cmake --build build` rebuilds what changed. Build the whole tree before believing a focused
run: not every suite has a build edge to the libraries it loads
([a loaded weave can spend the host's operators](../../agents/operators.md#a-loaded-weave-can-spend-the-hosts-operators)).

## 6. A test that fails before the change

The case comes first, and it fails on the unchanged code for the reason the issue gives.

1. **Put it with its subject's cases.** Workshop's suites are split by subject, and
   [where a case goes](../../agents/verification/population.md#where-a-case-goes) says how to
   find the one that pins a register's law; each package has its own suite; the external host's
   tool packages are checked by the scripts under `tests/session/`.
   [Testing a loaded pane](testing-workshop-panes.md) covers a case that needs a real pane. A case
   added to a suite raises that suite's floor in `tests/test_population.txt` to the measured
   count, in the same commit
   ([VM-POP-05](../../agents/verification/population.md#vm-pop-05--floors-are-minimums-anchored-to-a-measured-baseline)),
   and a new suite or target lists `zengine-sanitize` beside `zengine-warnings`
   ([VM-LANE-08](../../agents/verification/lanes.md#vm-lane-08--a-new-target-lists-zengine-sanitize-beside-zengine-warnings)).
2. **Run it alone.** A suite's binary is the target `tests/CMakeLists.txt` declares for its
   entry; the Workshop entry `workshop_<subject>` is `zengine-workshop-<subject>-tests`.

   ```sh
   build/tests/<binary> --list-test-cases --test-case='<pattern>'
   build/tests/<binary> --test-case='<pattern>'
   python3 tests/session/<checks>.py --tools external-host/tools/<package> --runtime <Loom checkout>/python
   ```

   These are the Linux spellings; on Windows a binary ends in `.exe`, and
   [the matrix](supported-toolchains.md#the-matrix) says what its runtime needs on `PATH`. A
   filter splits at every comma
   ([select the cases you think you selected](testing-workshop-panes.md#select-the-cases-you-think-you-selected)),
   so list what a pattern selects before trusting a run, and a run that selects nothing exits 70
   ([VM-POP-04](../../agents/verification/population.md#vm-pop-04--one-main-exits-70-on-an-empty-selection)).
3. **Keep what the red run printed, and the commit it ran on**: the pull request quotes both.
4. **Make the change**, and run the same case again.

A case asserts what *done when* requires, not what the code prints today, and it goes red on the
defect rather than on an incidental difference ([best practices](best-practices.md#the-witness)).
For an issue about a page, the red is the page as it reads before the change, quoted, or a text
check that fails on it.

## 7. The change

Before building it, the issue gets its `Proposed idea` comment, with the approach (step 1).
Then the smallest change that meets *done when* inside the fences. Everything it writes meets the
repository's standards for documents, comments, names, values and tests;
[best practices](best-practices.md) routes each to its owner.

## 8. The checks whose green counts

Commit first, as [attribution](repository-conventions.md#attribution) says, each commit one
coherent step. Then:

```sh
cmake -P tests/check_change_kind.cmake
```

says each changed file's kind against `origin/main`, and whether the change is
documentation-only. It reads the working tree and its untracked files too, so a pull request's
body file, an install prefix or a work directory belongs outside the clone, or under `build-*/`,
which Git ignores. If the change is documentation-only, the documentation lane is its
verification:

```sh
cmake -P tests/documentation_lane.cmake
```

Otherwise the official lane runs over the whole built tree, and
[the lanes in order](../../agents/verification.md#the-lanes-in-order) say when the sanitizer lane
and the installed-package witness are owed beside it; their commands are in
[AGENTS.md](../../AGENTS.md#build--test-canonical-wsl-consumes-an-installed-loom). Then, before
the push:

```sh
cmake -P tests/check_commit_attribution.cmake
git diff origin/main...HEAD
```

Read the whole diff, every file, as review will read it. What that read finds is fixed and
verified again before the push. A green you report names the repository, the configuration and
the compiler ([what a green means](build-and-test.md#what-a-green-means)), and quotes the lane,
never a bare `ctest` ([run the lane](build-and-test.md#run-testsverifycmake-not-a-bare-ctest)).

## 9. The pull request

```sh
git push -u origin <area>/<what>
gh pr create --base main --title "<what is true now>" --body-file <file outside the clone>
```

- **The title** is a sentence in the present tense saying what is true after the change, such as
  "The desk reader keeps each pane's provider and pane as a pair, so two panes whose names join
  alike stay two". The merge makes it the subject of a commit on `main`.
- **The body** says what was wrong and what changed, in a few sentences. Then the evidence: the
  case, the commit it was red on and what it printed; the lanes that passed, each with its
  repository, configuration and compiler; what did not run, and why. Then screenshots of a
  visible result, captured from the tested state, or the sentence that screenshots do not apply.
  A picture is attached on the pull request's web page, or committed under `docs/` and linked at
  its commit; [witnesses](../../agents/verification/witnesses.md) says how a live one is taken.
  Last, on a line of its own, `Fixes #<n>`, which closes the issue when the pull request merges.
- **Each commit is one coherent step**, its subject a sentence saying what is true after it, as
  the title does. The merge keeps every commit on `main`. A commit that changes a case a law cites
  lists the laws it re-verified ([ongoing rules](../../agents/workshop.md#ongoing-rules)).
- **Each commit is authored by the person who makes it**, never by the AI agent they use, and no
  commit or body carries a co-author line or an AI credit
  ([attribution](repository-conventions.md#attribution)). The body becomes the merge commit's
  message, which [the attribution guard](../../tests/check_commit_attribution.cmake) reads on
  `main`.
- **The issue gets its `Fix pushed, awaiting CI` comment**, with the pull request (step 1).
- **You do not merge.** Leave the pull request open once it is ready.

## 10. Reading the hosted run

CI runs on every pull request, never on a branch alone, and again on `main` after the merge.
Read the run job by job until every job has finished:

```sh
gh pr checks <n> --watch            # until none is pending; fails if any check failed, advisory too
gh pr checks <n> --required         # the checks the merge waits for; exits 8 while one is pending
gh run list --branch <area>/<what> --limit 1
gh run view <run-id>                # the jobs, each with its conclusion
gh run view <run-id> --log-failed   # a failed step's log, once the whole run has finished
gh api --allow-escape-sequences repos/Krealsion/Zengine/actions/jobs/<job-id>/logs   # one job's log before then
```

A check's link ends in its job's id.

- **The change's kind decides what runs.** A documentation-only change runs the attribution,
  change-kind and documentation jobs, and its build jobs show as skipped. A compiled change runs
  every job, and the sanitizer and MinGW-w64 jobs take longest.
- **The merge waits only for the checks `gh pr checks <n> --required` lists**, and one skipped
  for a documentation-only change counts as passed.
- **CI builds against Loom's current `main`**, resolved once when the run starts, so a red can
  come from Loom moving; the run names the Loom commit it built, and a red from Loom is reported
  as Loom's ([ownership and dependency direction](../../AGENTS.md#ownership-and-dependency-direction)).
- **An advisory job is red only in the job list.** The MSVC job says in its name that it is
  advisory: its red does not fail the run, and it is still reported
  ([VM-LANE-05](../../agents/verification/lanes.md#vm-lane-05--a-job-under-continue-on-error-is-red-only-in-the-jobs-list)).
- **A new push cancels the run in progress.** Push once, and read that run to its end before the
  next push.
- **A red is diagnosed before the next push**: name the job, the step and the case, reproduce it
  where the platform allows, and fix it on the branch.

The pull request is ready to merge when every required check has passed on its last commit and
the body's evidence names that commit.

## 11. Review, and `main` moving

A finding from review is fixed on the branch, verified as in step 8, and pushed once. The merge
does not wait for a branch to be current with `main`; when `main` moves and the pull request
conflicts with it, or review asks, rebase onto it, run the lane again, and update the body's
evidence to the new tip:

```sh
git fetch origin
git rebase origin/main
git push --force-with-lease
```
