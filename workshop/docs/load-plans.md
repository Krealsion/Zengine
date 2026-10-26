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

That is why there is no `--skin` flag and no `--input` flag: a plan is repeatable, diffable and
durable, so which window the desk opens in is a row of it rather than a second code path. The one
flag about the window, `--tui`, chooses among the windows the plan offers
([below](#the-window-the-os-window-else-the-terminal-window)); it adds none.

Workshop prints the whole executed arrangement on the way up when its standard output is not a
terminal that a terminal skin or the terminal window holds, and on a terminal desk the
[`Project` pane](../../introspection/docs/zengine.introspection.md#arrangement) shows the live
arrangement, so what a run is actually made of is on your screen rather than inferred from which
flags you passed.

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

## The shipped plan

[`workshop/default-load-plan.json`](../default-load-plan.json) is the one plan Workshop ships:
the desk drawn through the [graphics directory](../../graphics/docs/zengine.graphics.md), in the OS
window where one can open, else in the terminal window. Its third, fourth and fifth rows are the
medium and the input beside it:

| row | loads |
|---|---|
| third | `zengine-graphics`, as `zengine.graphics` |
| fourth | `zengine-window-sdl`, as `zengine.window.sdl`; **otherwise** `zengine-window-terminal`, as `zengine.window.terminal` |
| fifth | `zengine-input`, as `zengine.input` |

The directory introduces each pane's area and Workshop's own to the window that loaded: the
[OS window](../../graphics/docs/zengine.window.sdl.md), which reads SDL's one event queue itself,
or the [terminal window](../../graphics/docs/zengine.window.terminal.md), which takes the process's
terminal and sends its keys to their owners. No skin is loaded.
[`zengine-input`](../../input/docs/zengine.input.md) follows either and serves input sessions, a
guest's or a session tool's keys.

The plan also carries `zengine-operators-basic` (a provider only), `zengine-timer` (a provider
**and** a weave), `zengine-introspection` and `zengine-composer`. And it authors two **choices**
for the Editor's office: `standard`, the row that loads, and `neovim`, loaded only when you switch
to it ([Neovim in Workshop](../../editor/docs/neovim.md)).

## The window: the OS window, else the terminal window

The window row names, under `otherwise`, what loads where its own artifact cannot (plan format
version 4):

```json
{
  "artifact": "zengine-window-sdl",
  "provider": [],
  "weave": [ { "role": "zengine.window.sdl" } ],
  "optional": false,
  "otherwise": [
    { "artifact": "zengine-window-terminal", "weave": [ { "role": "zengine.window.terminal" } ] }
  ]
}
```

Read it as: the OS window, otherwise the terminal window. At each launch:

- **The OS window opens** where it can, and the desk is there.
- **Where it cannot open** -- no display, no video driver SDL can use, its library missing -- its
  load is refused, nothing of it stays loaded, and the terminal window loads in its place. The
  desk is in the terminal Workshop was started from; Workshop says why, once, in the Notices pane,
  and Attention keeps it for the run:

  ```text
  `zengine-window-sdl` did not load, so `zengine-window-terminal` stands in its row -- it would not start here: zengine-window-sdl: SDL_Init(SDL_INIT_VIDEO) failed: No available video device
  ```

  The words after `start here:` are SDL's own, which the OS window refused to be made in. It says
  them on standard error too, which the terminal window covers while it runs and gives back when
  Workshop ends.
- **A build without the OS window** (configured with `ZENGINE_TUI_ONLY=ON`) holds no
  `zengine-window-sdl`, and the terminal window loads with nothing said.
- **`--tui`** opens the terminal window, of the windows the plan offers, and tries no other: the
  row naming it in its place loads it alone. A plan offering no terminal window -- one naming the
  OS window alone -- is refused before anything opens, with its fix:

  ```text
  zengine-workshop - load plan refused: --tui opens the terminal window, and <plan> offers none (no row of this plan loads anything into `zengine.window.terminal`): add zengine-window-terminal under its window row's `otherwise`, or start without --tui (workshop/docs/load-plans.md)
  ```

The choice is the launch's: nothing switches a running Workshop to the other window, and no
start remembers which one opened. A project's own plan, which the Builder's `o` writes from the
plan in force, carries the row as it was read, `otherwise` and all, never the window that opened.
A plan naming one window reads, loads and writes as it always has.

**The retired graphical plan.** No build ships `graphical-load-plan.json`. A launch naming it where
it is gone is refused with the fix, and a file of yours by that name is a plan like any other:

```text
zengine-workshop - load plan refused: graphical-load-plan.json is retired: the shipped default-load-plan.json opens the OS window, else the terminal window -- start without --load-plan, or with --tui for the terminal window (workshop/docs/load-plans.md)
```

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
- **What a row loads in its place is `otherwise`** (plan format version 4): weaves, each in an
  office of its own, tried in order when the row's own artifact cannot load. An office with
  choices takes no row's alternatives.
- **Alternatives for an office are `choices`** (plan format version 2): each names an artifact that
  may hold the office and the word a weaver switches to it by, and exactly one of them is also the
  row that loads it. Keep both of a shipped plan's Editor choices if you want to switch editors;
  the format is [the reference's](load-plan.md#the-file-format), the law
  [editor-switch.md](../../editor/docs/editor-switch.md). The choice holding an office at quit is
  remembered in your session, not in the plan, and the next start loads it while the plan still
  authors it ([Neovim in Workshop](../../editor/docs/neovim.md#switch-while-you-work)).

## A project plan that names a Skin

A project's plan, `workshop-plan.json`, starts as a copy of the plan in force when the Builder's
`o` first wrote it. One begun while the shipped default loaded the terminal Skin names
`zengine-skin-tui-classic` (or `zengine-skin-tui-block`), as `zengine.skin`, in its third row; one
begun under the SDL Skin names `zengine-skin-sdl` there, and `zengine-input-sdl` after it. The
Skins and SDL's reader are retired, and Workshop refuses such a plan by the row's name before it
opens anything, saying what to put there:

```text
artifact 'zengine-skin-tui-classic' is retired: put the default plan's zengine-graphics row and its window row, zengine-window-sdl otherwise zengine-window-terminal, where it stands (workshop/docs/load-plans.md)
artifact 'zengine-skin-sdl' is retired: put the default plan's zengine-graphics row and its window row, zengine-window-sdl otherwise zengine-window-terminal, where it stands (workshop/docs/load-plans.md)
artifact 'zengine-input-sdl' is retired: put zengine-input where it stands (workshop/docs/load-plans.md)
```

A choice, or a row's alternative, naming one is refused the same way. Put the default plan's
`zengine-graphics` row and its window row where the skin's row stands, and leave every other row
as it is, the rows `o` added included; `zengine-input`, as `zengine.input`, stands where
`zengine-input-sdl` stood. The two rows are copied as they stand in
[`default-load-plan.json`](../default-load-plan.json), the window row with its `otherwise`, and the
plan becomes version 4 (`"version": 4` and `"format_version": "4"`), every row saying
`"otherwise"`, empty but the window's. For the terminal window alone, `zengine-window-terminal`, as
`zengine.window.terminal`, may stand in the window row with no `otherwise`, and the plan keeps its
version.

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
or `f`) is decided by the realization owner, [`zengine.realization`](zengine.realization.md). A row
that is **waiting** is realized: the product is copied into place and loaded. A row that is
**already live** is **reloaded in place**:

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

Every offer, promotion and revert the owner hears is numbered, and its answer names that number;
[follow one](zengine.realization.md#follow) by it, since a status that reads `promoted:` may be an
older promotion's.

## What a plan cannot do

No directory scan, no artifact enumeration, no dependency resolution, no version consultation,
no network, no resolution cache, and no rewriting itself: the one thing that adds a row is your
own `o` in the [Builder](../../builder/docs/zengine.builder-pane.md#builderload), which appends to the
project's `workshop-plan.json` after the running project accepted the row, and nothing edits,
reorders or removes one. And **no unload and no replacement**:
a plan is initial and restart intent, and a reload in place changes no row of it. See
[limitations](limitations.md#lifecycle).
