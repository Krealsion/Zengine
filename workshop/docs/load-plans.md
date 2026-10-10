# Choosing what a run is made of

**How-to.** Using and writing a load plan, from a weaver's side. The exact file format, the
execution law and the rollback rules are [the load-plan reference](load-plan.md).

## The idea

Workshop's host program names **no artifact**. Which providers are mounted and which weaves are
loaded into which roles is a file, read at startup:

```sh
zengine-workshop --load-plan <path>
```

Default: `workshop-plan.json` in the directory you launched from, when there is one — the file
`o` in the [Builder](../../builder/docs/zengine.builder-pane.md#builderload) writes — else
`default-load-plan.json`, beside the binary. The build-recipe catalog is found by the same
rule: `build-recipes.json` there, else the shipped one
([the recipe file](../../builder/docs/recipes.md#adding-a-recipe-from-files)).

That is why there is no `--skin` flag and no `--input` flag. Those two were the flags a plan
replaced, and the replacement is not cosmetic: a plan is repeatable, diffable and durable, so
"the graphical Workshop" is a second **shipped file** rather than a second code path.

Workshop prints the whole executed arrangement on the way up, so what a run is actually made of
is on your screen rather than inferred from which flags you passed.

**Workshop *begins* your project and then runs normally while it comes up.** Mounting a provider
finishes where it stands; loading a weave is a request whose answer comes back a few moments
later, so a row is done when its own answer arrives rather than when the file has been opened.
Your rows still happen strictly in the order you wrote them — one at a time, no reordering and no
retry — but the rest of the program is not blocked while any of them is in flight. The `Project`
pane says so: it shows one row `(loading)`, the rows above it resolved, and the rows below it
`(not reached)`.

If a row is refused, Workshop names the artifact, says how many participated before it, and
exits — the same behaviour as before, and still deliberate, because the row that fails may be the
one that draws your screen.

## The two shipped plans

| file | is |
|---|---|
| [`workshop/default-load-plan.json`](../default-load-plan.json) | the terminal Workshop |
| [`workshop/graphical-load-plan.json`](../graphical-load-plan.json) | the windowed Workshop |

They differ in exactly two rows:

| | default | graphical |
|---|---|---|
| skin | `zengine-skin-tui-classic` | `zengine-skin-sdl` |
| input | `zengine-input` | `zengine-input-sdl` |

Everything else is identical, and Workshop's own code is identical under both. Both plans also
carry `zengine-operators-basic` (a provider only), `zengine-timer` (a provider **and** a
weave), `zengine-introspection` and `zengine-composer`. And both author two **choices** for the
Editor's office: `standard`, the row that loads, and `neovim`, loaded only when you switch to it
([Neovim in Workshop](../../editor/docs/neovim.md)).

## Reading a record

```json
{
  "artifact": "zengine-timer",
  "provider": [ { "mode": "normal" } ],
  "weave":    [ { "role": "zengine.timer" } ]
}
```

One record per artifact, with two optional surfaces:

| record | means |
|---|---|
| `provider` set, `weave` empty | mount this artifact's operator contributions and nothing else. It is not a participant |
| `provider` empty, `weave` set | load it as a weave into that role. Its provider surface, if it has one, is not mounted |
| both set | mount the contribution, **then** load the weave |
| `"mode": "overlay"` | contribute over powers already in the catalog, reversibly |

An artifact that exports both surfaces and is asked for one **gets one**. Nothing infers a
provider mount from a weave declaration, or the reverse.

**Order between records is yours; order within a record is not.** Between artifacts the order
is authored policy — there is no solver, and a person wrote the rows in the order they must
happen. Within one record, provider-before-weave is law: a provider+consumer artifact validates
the rule it is about to spend inside its own construction, which happens several deliveries
after the load command, so the contribution must be in the catalog first.

## Making your own

Copy a shipped plan and change rows. Two things to know:

- **Artifact stems are resolved to files by the host**, next to the executable. The plan names a
  stem; exactly one rule in Workshop's host turns a stem into a path, and a test reads that
  source and refuses a plan that tries to spell one itself.
- **Adding a native artifact to a plan is an execution-authority decision, not configuration.**
  A plan row causes code to be loaded into this process. Treat editing one the way you would
  treat editing a list of shared libraries a program will `dlopen` — because that is what it is.
- **Alternatives for an office are `choices`** (plan format version 2): each names an artifact that
  may hold the office and the word a weaver switches to it by, and exactly one of them is also the
  row that loads it. Keep both of the shipped plans' Editor choices if you want to switch editors;
  the format is [the reference's](load-plan.md#the-file-format), the law
  [editor-switch.md](../../editor/docs/editor-switch.md).

## When a record fails

One artifact is the atomic unit. A record that mounts a provider and then fails to load its
weave **rolls back its own mount** — by the provider identity the artifact declared — and stops
the plan.

Earlier artifacts are **not** rolled back. A transaction across the whole plan is a bigger
promise than has been measured a need for, so instead you are told which artifact stopped it
and what still stands.

Three ways an operator handoff can end, and they are three rather than two:

| outcome | means |
|---|---|
| *not a consumer* | an ordinary weave. Not a fault and not a diagnostic — most weaves are this |
| *offered* | the artifact took this host's resolution for this one load |
| *a failed handoff* | the image **does** export a consumer surface and the handoff did not complete. This **refuses the artifact** |

The third refuses rather than continuing, because an artifact that falls back to its own local
copy of a rule when nothing was offered would silently swap the process's semantic authority
for that copy — a downgrade invisible in every answer until the two disagree.

## When an artifact has not been built yet

A row whose artifact is **not on this disk** and which some [build recipe](../../builder/docs/recipes.md) in this
project **can produce** is not a failure. Workshop stops at that row, says so, and keeps
running:

```text
zengine-workshop - waiting to be built: zengine-oven (build it, and its authored
                   participation is performed then -- every authored row after it is
                   waiting on this one)
```

That is what a project looks like on its first run: the plan says how the artifact
participates, the artifact has not been built yet, and Workshop still starts. The Builder pane
shows the same fact on its `project` row — the waiting artifact, the recipe that produces it,
and how many authored rows are stopped behind it — and **`f`** builds and realizes it in one
gesture (or `b` with *load after build* armed, having chosen the recipe with `c`; see
[the frontier](../../builder/docs/zengine.builder-pane.md#builderfrontier)). Its authored participation is performed **in
the same run** — the role, the mount mode and the order all come from this file, and the
Builder supplies nothing but the file. The moment it settles, the rows after it are performed
too.

**The rows after it wait as well, and that is deliberate.** The order you wrote is the order
things happen in — it is this file's whole way of saying that one artifact needs another
(see [Reading a record](#reading-a-record)). Running the later rows first because an earlier
artifact happens to be missing would give you an arrangement your plan does not describe, and
one that stops matching it the day you build that artifact before starting.

You can still **build** a later artifact while an earlier one is waiting; building and
participating are different things. Asking to *realize* it early is answered with the name of
the artifact the project is waiting on, and nothing changes.

An artifact that is missing and that **nothing here can build** still refuses the plan by name.
And an artifact that is already loaded is **reloaded in place** when you load its rebuilt
product — same `WeaveId`, state kept, same shapes only; the plan's row is untouched, and which
image a restart loads is a separate, explicit act ([below](#when-a-built-artifact-is-loaded-again)).

The `Project` pane calls the waiting row `pending`, and every row behind it `authored`.

## When a built artifact is loaded again

A build offered to the running project ([load after build](../../builder/docs/zengine.builder-pane.md#builderarm),
or `f`) is decided by the realization owner, `zengine.realization`. A row that is **waiting** is
realized: the product is copied into place and loaded. A row that is **already live** is
**reloaded in place**:

- the host copies the rebuilt product to a path of its own beside Workshop,
  `<stem>.reloads/<stem>-<n>`, off the file the process has loaded, under a name no file has yet;
- the realization owner asks the Weave Manager to reload the weave from that copy;
- the Loom swaps the code behind the **same weave id** and carries the weave's **state** across.
  The role, the routing and every other weave are untouched.

The Builder's `realize` row then says `reloaded in place -- weave #N keeps its id and its state`,
naming the operation it is about. Any other offer is refused in the owner's words: an artifact the
plan does not name (`this project does not name artifact 'oven': a build can produce a file, and
only the project's own plan can say how it participates`), or one behind the row the project waits
on, which may be built now and takes part when the rows in front of it have.

**Two things a reload leaves you.** The file a restart loads is exactly what it was, so the row says
`NOT DEFAULT` until you choose. **Promote** ([`builder.promote`](../../builder/docs/zengine.builder-pane.md#builderpromote))
writes the running image into that file, beside itself and then renamed, so a refused write leaves
nothing half written. **Revert** ([`builder.revert`](../../builder/docs/zengine.builder-pane.md#builderrevert))
runs the image before the last reload again, by the same reload: same id, state kept. Before a
promotion writes over the file, it keeps those bytes beside the reloads, so a revert after a promote
still runs the code you had. With no reload in this run there is nothing to revert to, and revert
says so.

**Same shapes only.** A reload carries state and keeps routing, so it is refused, before the running
weave is touched, when the rebuilt weave keeps a different **state** or answers different
**messages**; the refusal says what changed:

> the rebuilt 'zengine-oven' keeps a different STATE than the running one; a same-shape reload
> cannot carry the state across, so the running weave was left as it is. Replacing it is a
> prepared replacement with an authored migration (Loom: state schema version mismatch; reload
> refused)

An artifact that also supplies operators to the catalog is not reloaded, and a row that is only a
provider has no weave to reload; both are refused in words.

Every offer, promotion and revert the owner hears is numbered: it says `RealizationAsked` v1 first,
taken as ask number N or refused in its own words, and its answer names N (`ArtifactRealized` v3,
`ArtifactPromoted` v2; 0 for an ask it did not take). A promotion is answered at once, a revert when
its reload settles. Follow a promotion or a revert by that number: a status that reads `promoted:`
may be an older promotion's.

## What a plan cannot do

No directory scan, no artifact enumeration, no dependency resolution, no version consultation,
no network, no resolution cache, and no rewriting itself: the one thing that adds a row is your
own `o` in the [Builder](../../builder/docs/zengine.builder-pane.md#builderload), which appends to the
project's `workshop-plan.json` after the running project accepted the row, and nothing edits,
reorders or removes one. And **no unload and no replacement**:
a plan is initial and restart intent, and a reload in place changes no row of it. See
[limitations](limitations.md#lifecycle).
