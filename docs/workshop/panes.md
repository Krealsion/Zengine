# Panes

**How-to.** Opening, closing, moving, resizing and ordering Workshop's panes, and the honest
answer to "how do I get a bigger one".

A **pane** is one region of Workshop's screen. Two kinds exist and a maker does not need to
tell them apart to use them:

- **built-in panels** — compiled into Workshop: `Builder` and `Info`.
- **external panes** — offered by a loaded weave through a bounded read-only protocol.
  `Loaded`, `Project` and `Powers` arrive this way, from `zengine-introspection`.

## Opening and closing — the picker

Press **`p`** — the band's legend advertises it as `p + panel`, and the full hotkey view
(`Ctrl`+`k`) lists it, so this one is on screen from the first frame.

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

Press **`w`** — the band's legend advertises it as `w window`, and the full hotkey view
lists it. This is a mode, so its keys need no modifier.

**Its sub-keys are not announced.** On entry the status line names the selected pane and its
current window; it does not list `m`, `s`, `f`, `b`, `r`, `l` or `0`. The sub-modes do announce
their own keys once you are in them (`+ window size <edge>`, `+ window reset -- p place, w
width, h height, o order, esc back`), so the table below is the missing first step.

| key | does |
|---|---|
| `Tab` `↑` `↓` | choose a pane |
| `m` | **move** — arrows nudge one cell; `Esc` back |
| `s` | **size** — `Tab` picks which edge, arrows grow one cell; `Esc` back |
| `f` | send to front |
| `b` | send to back |
| `r` | raise one step |
| `l` | lower one step |
| `d` | remove the selected pane — the picker brings it back |
| `0` | **reset** — then `p` place, `w` width, `h` height, `o` order; `Esc` back |
| `Esc` | close management |

Everything you author here goes into the **setup**, so it survives if you save it with `s`.
See [setups](setups.md).

## The context menu — what can I do with this?

**Right-click anything** and Workshop lists what can be done with the thing you pointed at —
the same operations its keys already perform, aimed at that thing:

- **a pane** — `move`, `size`, `Arrange >` (front / back / raise / lower), `Reset >`
  (place / width / height), `remove`;
- **a document object** — `delete`;
- **the empty room** — Workshop's own doors: a new object, the picker, management, the
  terminal, attention, the hotkey view, save / open, the setup gestures, reset order.

While the menu is open: `↑` `↓` choose a row, `Enter` chooses it (a `… >` row opens its
group), `Esc` backs out of a group or closes the menu, and a click outside dismisses it —
a click spent on closing the menu never also operates whatever it landed on. **`a`** opens
the same menu from the keyboard, on the selected object or the empty room, so the
capability does not depend on a mouse.

**Pointing is not selecting.** Opening the menu on a pane or an object changes no
selection and moves no keyboard focus — the menu holds the pointed thing only for the one
action you choose. `move` and `size` are the deliberate exception: choosing one enters
management **on that pane**, because arranging is an ongoing state, and only after the
pane passes the same checks the `w` road applies.

The menu offers what is *meaningful* for that kind of thing, not a prediction of success —
choose `move` on the Info panel and the owner answers in its own words (`the screen owns
its place`). On a terminal, whether a right-click reaches Workshop at all is the terminal
emulator's decision first (the Windows console and Windows Terminal both hand it through);
the `a` key works everywhere.

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
| feature undiscoverable? | **partly.** The band's legend and the hotkey view advertise `w window`, so the *mode* is discoverable. Once inside it, the select-mode status line names the selected pane's window but not the keys that change it — so `s` is the step nothing on screen points at |
| feature tedious? | **yes** — one cell per keypress, with no larger step, no drag-to-size, and no "fill the room" |
| product-hostile? | **no**, but the default is: a 9-row pane over the material you are building is the arrangement a maker meets first |

The smallest things that would change the felt experience: one help line in select mode naming
its own keys (the pattern the reset sub-mode already uses), a coarse step on a modifier, and a
pane-height default that reads the surface. None is built, and none is designed here.

## Pane titles

Every loaded pane carries a one-row title — `name @provider` — reserved out of the room its
provider is granted. Press **`t`** (`workshop.pane-titles`, remappable like every binding) to
hide the titles and return that row to each provider's content; press it again to bring them
back. The choice is presentation only — no pane's identity, geometry or saved setup changes —
and it lasts for the current run.

**One exception, and it is the law rather than a leftover:** the pane currently holding the
keyboard keeps its title, mark and all. The `> ` mark is one of the two on-screen statements
of where typing goes, and hiding chrome may never hide that — so with titles off, focusing a
pane shows its title for exactly as long as the focus holds.

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
