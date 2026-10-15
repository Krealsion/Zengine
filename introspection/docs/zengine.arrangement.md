# zengine.arrangement -- the arrangement door

**Manual.** The office that says what your project asked for and what came of it, answering to
`zengine.arrangement` (an office is the name a weave answers to). Workshop mounts it in its own
process before it performs your project's plan; it is not a file you load. The `Project` pane of
[`zengine.introspection`](zengine.introspection.md#arrangement) shows its answer, and any office may
ask it the same question.

## About

Your project's *load plan* names the artifacts it runs, each as a provider of powers, a weave, or
both, and *realization* performs it row by row, in order, stopping at a row that refuses unless
the row is optional, which it steps over. The door reads two things the host owns, the plan and
what realization made of each row, at the moment you ask, so it answers mid-realization too. It
keeps no copy and no answer between asks, publishes nothing and runs nothing: it cannot mount,
load, unload, reload or replace anything, and its grant is its answers.

The answer pairs what was authored with what resolved, and never merges them. `authored_role` is
the office the plan asks a weave to hold, and `provider` is the identity the artifact declared for
itself when it was mounted: two facts that may read alike. There is no resolved office: the office
the kernel bound is `zen.ListLoaded`'s answer.

A host that mounts no such door holds no `zengine.arrangement`, and an ask reaches nobody.

## Use

Ask [`ArrangementRequested`](#arrangementrequested) as an office you hold, and read the answer that
comes to you alone: Loom marks it as the answer to your ask, so no other sender can supply it.

## Commands

### `ArrangementRequested`

`ArrangementRequested` v1 asks what this project asked for, and where realization has got with each
row.

- **Takes:** nothing.
- **Answers:** to the asker alone, `ResolvedArrangement` v2 when the asking office's holder accepts
  it, else v1: `plan`, the file the host read, and `artifacts`, one a plan row in plan order, then
  each authored choice a switch loaded that is not a row. Each has `artifact` (the stem),
  `authored_provider` (`normal` or `overlay`), `authored_role` and `state`: `authored` (not
  reached), `pending`, `loading`, `resolved`, `refused`, `reloading`, `switched` or `unavailable`,
  which v1 says `refused`. A `resolved` or `reloading` row adds `provider`, `powers`, `weave` (the
  weave id this kernel minted, 0 for none) and `offer` (`offered`, `not-a-consumer`,
  `version-mismatch` or `not-opened`; empty where no weave was loaded). v2's rows add `optional`,
  `reason`, the refusing layer's sentence, and `next`, as `make 'tally' available (build it), then
  relaunch Workshop -- this run stepped over it and will not return to it`, `fix what the refusal
  names, then relaunch -- the project stopped at this row` or `build it; its authored participation
  is performed then`.
- **Refuses:** an ask authored as no office, personal speech or a root's send, is not answered; the
  door counts it (`refused`).
- **Example:** from a weave holding `zengine.introspection`,
  `mail.as_role("zengine.introspection").send_to_role("zengine.arrangement", ArrangementRequested{}, 7);`,
  then match the answer's correlation, 7.

## See also

- [`zengine.introspection`](zengine.introspection.md), whose `Project` pane shows this answer.
- The shapes: [`workshop/arrangement_vocabulary.hpp`](../../workshop/arrangement_vocabulary.hpp).
- [`zengine.powers`](zengine.powers.md), which answers which powers resolve.
