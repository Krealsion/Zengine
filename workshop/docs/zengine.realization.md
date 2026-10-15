# zengine.realization -- the realization owner

**Manual.** The realization owner makes your project run: it performs the project's load plan, and
it decides what becomes of a build offered to the running project. It is Workshop's plan booter,
the weave that answers to the office `zengine.realization` (an office is the name a weave answers
to). The Builder offers it what it built, and the Builder pane asks it to promote or revert; it says
what came of each. Workshop mounts it; it is not a file you load.

## About

A *load plan* says which artifacts your project is made of and how each takes part
([the load plan](load-plan.md)). The owner performs its rows in the order you wrote them: it mounts
an artifact's operators, then asks the Weave Manager to load its weave into its office, one row at
a time. It stops at the first row it cannot perform. A row whose artifact is not built yet, and
which a recipe can build, is not a failure: realization waits there, and the rows behind it wait
too, because the order you wrote is the order things happen in.

Building a file and loading it are two answers. The Builder builds; this owner decides whether the
project loads what was built, by the plan alone: a build of the row the project waits on is loaded,
and a build of a row already running is **reloaded in place** from a copy the host stages beside the
file it loaded: same weave id, state kept, same shapes only. The file a restart loads is untouched
by a reload; a *promotion* writes the running image into it, and a *revert* runs the image before
the last reload again. An offer's announced path is never used: the owner finds the file from the
artifact's name by the host's rule, so a message cannot redirect a load.

It is the one weave in Workshop that may ask the Weave Manager to load or reload a weave, and it
may ask that of the Manager alone.

## Use

Offer a build with [`OfferArtifact`](#offerartifact), promote with
[`PromoteArtifact`](#promoteartifact), revert with [`RevertArtifact`](#revertartifact): each is
published, and the owner hears every one. Then follow its account ([Follow](#follow)):
`RealizationAsked` says what became of your ask and gives it a number, and the answer names that
number. The Builder offers a build when you asked it to realize one
([`BuildRequested`](../../builder/docs/zengine.builder.md#buildrequested)); the Builder pane
publishes a promotion ([`builder.promote`](../../builder/docs/zengine.builder-pane.md#builderpromote))
or a revert ([`builder.revert`](../../builder/docs/zengine.builder-pane.md#builderrevert)). Every
refusal below says the artifact's name as `'oven'`.

## Commands

### `OfferArtifact`

`OfferArtifact` v1 offers a build to the running project: realize the row waiting on it, or reload
the row already running it.

- **Takes:** `artifact`, the artifact's name (its stem, as the plan names it); `recipe`, the recipe
  that built it, which the host copies from; `op`, the build's operation; `path`, which is ignored.
- **Answers:** `RealizationAsked` v1, act `offer`, taken as ask N or refused; then
  `ArtifactRealized` v3 naming N, when the load or reload settles: `realized` with what took part,
  `weave #21 as example.oven` (and `provider 'oven' supplied 3 | ` before it), or
  `reloaded in place -- weave #21 keeps its id and its state`, which a reload not yet promoted
  follows with `; not the default yet -- the next launch runs the old code until this image is
  promoted`; or not realized, in the refusing layer's words: `artifact 'oven': weave load refused:
  <the Manager's words>`, or a reload's refusal ([`RevertArtifact`](#revertartifact)).
- **Refuses:** with `ArtifactRealized` naming ask 0: `this project does not name artifact 'oven':
  a build can produce a file, and only the project's own plan can say how it participates`;
  `artifact 'oven' is not waiting to be realized in this run`; `this project's realization is
  waiting on artifact 'tally', which its plan authors before 'oven'. Authored order is realization
  order, ...`; `realization has not begun`; `realization is not between rows: it is still performing
  the authored plan`; `this project's realization stopped at a refusal and is not performing any
  further rows`; the host's staging refusal, `the rebuilt 'oven' could not be staged off the loaded path: <the host's words>;
  the running image is unchanged`; and a live row that cannot be reloaded, as a revert says.
- **Example:** the Builder, after a build of `oven` asked to be realized succeeded,
  `mail.publish(zengine::builder::OfferArtifact{3, "oven", "oven", path});`.

### `PromoteArtifact`

`PromoteArtifact` v1 asks for the running image of an artifact to become the file a restart loads.

- **Takes:** `artifact`, the artifact's name.
- **Answers:** `RealizationAsked` v1, act `promote`, always taken as ask N; then at once
  `ArtifactPromoted` v2 naming N: `promoted` with `promoted: the next launch runs the image weave #21
  is running now`, or not, with why. The host writes the file beside itself and renames it, and
  keeps the bytes it wrote over beside the reloads, so a revert still runs the code you had.
- **Refuses:** in `ArtifactPromoted.detail`: `artifact 'oven' already runs from the file a restart
  loads; there is nothing to promote`; `artifact 'oven' loaded no weave in this run; there is no
  running image to promote`; `artifact 'oven' is not part of this running project`; `the running
  'oven' could not be promoted: <the operating system's words>; the file a restart loads is
  unchanged`; and, as an offer is, a realization that is not between rows.
- **Example:** the Builder pane's `builder.promote` publishes
  `zengine::builder::PromoteArtifact{"oven"}`.

### `RevertArtifact`

`RevertArtifact` v1 asks for the image before an artifact's last reload to run again, by the same
reload: same weave id, state kept.

- **Takes:** `artifact`, the artifact's name.
- **Answers:** `RealizationAsked` v1, act `revert`, taken as ask N or refused; when the reload
  settles, `ArtifactRealized` v3 naming N: `reverted: weave #21 runs the image before the last reload
  again, and keeps its id and its state`, or the reload's refusal, which names what changed and ends
  with Loom's own words: `the rebuilt 'oven' keeps a different STATE than the running one; ...`,
  `... answers to different MESSAGES than the running one; ...`, `... changed a shape but kept its
  name and version, ...`, `(Loom: <its words>)`.
- **Refuses:** with `ArtifactRealized` naming ask 0: `artifact 'oven' has no previous image to
  revert to: it has not been reloaded in this run, or a promotion wrote over the image before its
  last reload`; `artifact 'oven' is not part of this running project`; `artifact 'oven' also
  supplies operators to the catalog; ...`; `artifact 'oven' loaded no weave in this run: ...`;
  `artifact 'oven' is not running: its office example.oven is held by the authored choice 'tally';
  switch back to it before reloading it`; and, as an offer is, a realization not between rows.
- **Example:** the Builder pane's `builder.revert` publishes
  `zengine::builder::RevertArtifact{"oven"}`.

## Hears

The Weave Manager's answer to the load or reload the owner asked for, Loom's `zen.Result` (the
loaded weave's id), `zen.Ack` or `zen.Refused` (its words). An answer counts only when its
correlation and its sender are those of the one ask outstanding; any other is left alone, neither
refused nor recorded. One answer settles the row in flight, or the reload.

## Follow

It publishes its own account, as `zengine.realization`, for anyone to follow:
`RealizationAsked` v1 once for every offer, promotion and revert it hears, before anything else
about it, taken as ask N or refused in its words; then `ArtifactRealized` v3 or `ArtifactPromoted`
v2, naming N (0 for an ask it did not take). A promotion is answered at once, an offer and a revert
when their load or reload settles, so other answers may come between: follow an ask by its number,
never by order. The plan's own rows, performed at launch, are said by Workshop and shown by the
Project pane, not here.

## Files

- **The load plan** it performs: [the load plan](load-plan.md), and
  [choosing what a run is made of](load-plans.md).
- **A reload's copy.** It writes no file itself: the host copies a rebuilt product beside Workshop,
  to `<stem>.reloads/<stem>-<n>`, under a name no file has yet, and the owner reloads from there.
- **A promotion** is the host's write into the file the plan loads, beside it and then renamed; the
  bytes it wrote over are kept in `<stem>.reloads/` first.

## See also

- [When a built artifact is loaded again](load-plans.md#when-a-built-artifact-is-loaded-again).
- [`zengine.builder`](../../builder/docs/zengine.builder.md), which offers builds, and
  [`zengine.builder-pane`](../../builder/docs/zengine.builder-pane.md).
- [`builder/vocabulary.hpp`](../../builder/vocabulary.hpp), where these shapes are declared, and
  [`workshop/load_execute.hpp`](../load_execute.hpp), the owner.
