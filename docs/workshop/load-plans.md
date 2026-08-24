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

## What a plan cannot do

No directory scan, no artifact enumeration, no dependency resolution, no version consultation,
no network, no resolution cache, and no rewriting itself. And **no unload and no reload**: a
plan is initial and restart intent. See [limitations](limitations.md#lifecycle).
