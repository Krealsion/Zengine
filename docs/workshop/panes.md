# Panes

**How-to.** Opening, closing, moving, resizing and ordering Workshop's panes, and the honest
answer to "how do I get a bigger one".

A **pane** is one region of Workshop's screen. Two kinds exist and a maker does not need to
tell them apart to use them:

- **built-in panels** — compiled into Workshop: `Builder` and `Info`.
- **external panes** — offered by a loaded weave through a bounded read-only protocol.
  `Loaded`, `Project` and `Powers` arrive this way, from `zengine-introspection`.

## Opening and closing — the picker

Press **`p`**.

| key | does |
|---|---|
| `↑` `↓` | choose a row |
| `Enter` | open it, or remove it if it is already open |
| `Esc` or `p` | cancel |

The picker is the **one** owner of whether a pane is present. No pane advertises its own close
key, because with two kinds of pane a per-pane binding stops being tenable.

Its rows are the union of what this build can present and what your setup names. A row for a
pane this build has never heard of still appears, with a state word saying so — that is what
lets you tell a typo from a pane you have not installed. Without that row, a setup naming an
unknown pane could be seen and not removed without editing the file by hand.

The picker paints a whole pane's worth of rows even when it has fewer to show. In a character
medium there is no edge between an overlay and what is under it, so a short picker over a
9-row pane left the pane's last rows showing beneath — one box saying two unrelated things.

## Moving, resizing and ordering — management mode

Press **`w`**. This is a mode, so its keys need no modifier.

| key | does |
|---|---|
| `Tab` `↑` `↓` | choose a pane |
| `m` | **move** — arrows nudge one cell; `Esc` back |
| `s` | **size** — `Tab` picks which edge, arrows grow one cell; `Esc` back |
| `f` | send to front |
| `b` | send to back |
| `r` | raise one step |
| `l` | lower one step |
| `0` | **reset** — then `p` place, `w` width, `h` height, `o` order; `Esc` back |
| `Esc` | close management |

Everything you author here goes into the **setup**, so it survives if you save it with `s`.
See [setups](setups.md).

### What "front" means

Depth is two levels and that is the whole model. Within one plane: rectangles, then labels
over them, then bounded text regions over those. Between planes: the complete earlier plane,
then the complete later one over it. There is no numeric z, no sorting, no layer identity —
front is *painted later*, and `f` `b` `r` `l` author a position in an order.

### What a refusal means here

A pane whose geometry cannot be projected in the current medium refuses the gesture rather
than silently doing something else. An authored size in **pixels** is legal to hold and cannot
be presented on a terminal — a setup legal on a window stays legal on a terminal, and what
changes with the medium is which authored intent can be *shown*, never which can be *held*.

## Pane geometry

**A pane placed in the stack is 9 rows tall, and a bigger terminal does not grant more.**

That is the current default and it is a real constraint, not a rendering artefact. A wider
surface *does* widen a pane — the surplus over the 48-column minimum is split evenly between
the pane and the material under it, so at 200 columns a pane's granted width goes from 48 to
109 without any threshold. Height does not work that way: the stack slot is a fixed number of
rows.

### How to actually get a bigger pane

`w` → choose the pane → `s` → `Tab` to pick the edge → arrows.

Each arrow press grows the pane by **one cell**. Going from the 9-row default to something
comfortable is therefore twenty-odd keypresses, and then `Esc` `Esc` `s` `Enter` to persist it.
An authored size in cells is accepted up to the document's cell maximum.

Judged plainly, and repeated in [limitations](limitations.md):

| | |
|---|---|
| feature absent? | **no** — authored per-pane size exists, persists, and is honoured |
| feature undiscoverable? | **partly** — nothing on screen says a pane can be resized until you are already in management mode |
| feature tedious? | **yes** — one cell per keypress, with no larger step, no drag-to-size, and no "fill the room" |
| product-hostile? | **no**, but the default is: a 9-row pane over the material you are building is the arrangement a maker meets first |

The smallest thing that would change the felt experience is a coarse step (a modifier that
moves several cells) and a pane-height default that reads the surface. Neither is built, and
neither is designed here.

## Panes as an author

If you want to *add* a pane, that is [Making a Workshop
tool](../guides/make-a-workshop-tool.md), which sorts the work into the two paths it can take:
a compiled-in panel (source-contributor work) or an office-authored external pane (the bounded
read-only provider protocol — four shapes, a prose budget, no input, and no installation
story yet).

The exact wire shapes are
[`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp). The smallest complete
external-pane witness is
[`tests/weavelib/workshop_hello.cpp`](../../tests/weavelib/workshop_hello.cpp) — a test
fixture, not a product plugin. The shipped one is
[`introspection/`](../../introspection/loaded.hpp), documented at
[introspection](../reference/introspection.md).

## What a pane may not do

An external pane is **read-only prose within a bounded budget**. It publishes rows; it does
not receive keyboard input, cannot draw arbitrary geometry, and cannot exceed the room it was
granted. A pane that has more to say than it can hold must say *how much it left out* — a
count a reader can trust is a count that names what it read.

There is no plugin discovery and no installation story: a pane arrives because an artifact was
in the [load plan](load-plans.md) and the weave offered one.
