# Choosing what a run is made of

**How-to.** Using and writing a load plan, from a maker's side. The exact file format, the
execution law and the rollback rules are [the load-plan reference](../reference/load-plan.md).

## The idea

Workshop's host program names **no artifact**. Which providers are mounted and which weaves are
loaded into which roles is a file, read at startup:

```sh
zengine-workshop --load-plan <path>
```

Default: `default-load-plan.json`, beside the binary.

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
| [`workshop/default-load-plan.json`](../../workshop/default-load-plan.json) | the terminal Workshop |
| [`workshop/graphical-load-plan.json`](../../workshop/graphical-load-plan.json) | the windowed Workshop |

They differ in exactly two rows:

| | default | graphical |
|---|---|---|
| skin | `zengine-skin-tui-classic` | `zengine-skin-sdl` |
| input | `zengine-input` | `zengine-input-sdl` |

Everything else is identical, and Workshop's own code is identical under both. Both plans also
carry `zengine-operators-basic` (a provider only), `zengine-timer` (a provider **and** a
weave), `zengine-introspection` and `zengine-composer`.

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

A row whose artifact is **not on this disk** and which some [build recipe](builder.md) in this
project **can produce** is not a failure. Workshop stops at that row, says so, and keeps
running:

```text
zengine-workshop - waiting to be built: zengine-oven (build it, and its authored
                   participation is performed then -- every authored row after it is
                   waiting on this one)
```

That is what a project looks like on its first run: the plan says how the artifact
participates, the artifact has not been built yet, and Workshop still starts. Build it with
`Shift+b` in the Builder pane and its authored participation is performed **in the same run** —
the role, the mount mode and the order all come from this file, and the Builder supplies
nothing but the file. The moment it settles, the rows after it are performed too.

**The rows after it wait as well, and that is deliberate.** The order you wrote is the order
things happen in — it is this file's whole way of saying that one artifact needs another
(see [Reading a record](#reading-a-record)). Running the later rows first because an earlier
artifact happens to be missing would give you an arrangement your plan does not describe, and
one that stops matching it the day you build that artifact before starting.

You can still **build** a later artifact while an earlier one is waiting; building and
participating are different things. Asking to *realize* it early is answered with the name of
the artifact the project is waiting on, and nothing changes.

An artifact that is missing and that **nothing here can build** still refuses the plan by name.
And an artifact that is already loaded is refused rather than reloaded: a rebuilt file has not
changed the image that is running.

The `Project` pane calls the waiting row `pending`, and every row behind it `authored`.

## What a plan cannot do

No directory scan, no artifact enumeration, no dependency resolution, no version consultation,
no network, no resolution cache, and no rewriting itself. And **no unload and no reload**: a
plan is initial and restart intent. See [limitations](limitations.md#lifecycle).
