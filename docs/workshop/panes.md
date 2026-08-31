# Panes

**How-to.** Opening, closing, moving, resizing and ordering Workshop's panes, and the honest
answer to "how do I get a bigger one".

A **pane** is one region of Workshop's screen. Two kinds exist and a maker does not need to
tell them apart to use them:

- **built-in panels** — compiled into Workshop: `Builder`, `Info`, `Editor` and `Files`.
- **external panes** — offered by a loaded weave through a bounded protocol.
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

The picker paints a whole pane's worth of rows even when it has fewer to show, so a short
picker over a taller pane cannot leave that pane's last rows showing beneath it — one box
saying two unrelated things.

## Every pane has an edge, and one of them is yours

Each pane draws a **visible boundary** inside its own rectangle: one cell on every side, in
both a terminal and a window. The pane does not grow to make room for it — the rectangle you
author is the rectangle the pane occupies, and its contents are laid out inside the edge. So
a pane you drag to 40×12 still occupies exactly 40×12, and what it can show you is 38×10.

The pane you **last pressed into is the selected one**, and its edge is drawn differently
from every other pane's. Nothing else changes: selecting a pane does not open it, close it,
move it, or hand it the keyboard unless it is a pane that takes keys anyway.

Selecting a pane also brings it **forward** for as long as it stays selected — see
[what "front" means](#what-front-means).

## Moving, resizing and ordering — Arrange

Arranging has two scopes, and both mean the same thing: manipulate panes directly.

**Arrange one pane**: right-click the pane and choose **`arrange`**. The interaction is
bound to exactly that pane — its eight edge and corner handles appear, dragging its body
moves it, dragging a handle resizes it, and the arrow keys do the same one cell at a time.
Clicking anywhere else changes nothing and reminds you which pane you are arranging.
`Esc` or a right-click leaves.

**Arrange the desk**: press **`w`** — the band's legend advertises it as `w arrange desk`,
and the full hotkey view lists it. Every arrangeable pane wears its handles; drag any
pane's body or edges directly. No pane is "selected" when the scope opens and none has to
be: a press is its own targeting. The keyboard steps between panes instead:

| key | does |
|---|---|
| `Tab` / `Shift`+`Tab` | step the keyboard to the next / previous pane |
| `← → ↑ ↓` | move that pane one cell |
| `Shift`+arrows | resize it one cell — wider / narrower / taller / shorter, its top-left corner staying put |
| `=` / `-` | **grow / shrink** it four cells on both axes at once — the coarse step, same anchor |
| `Enter` | narrow to arranging exactly that pane |
| `f` `b` `r` `l` | send to front / back, raise / lower one step |
| `d` | remove that pane — the picker brings it back |
| `0` | **reset** — then `p` place, `w` width, `h` height, `o` order; `Esc` back |
| `Esc` or right-click | leave |

The same movement and resize keys work in the one-pane scope; `Tab` and `Enter` belong to
the desk, which is the only place there is a next pane to step to. The pointer's handles
reach all eight edges and corners, each keeping its opposite edge anchored; the keyboard's
resize always grows from the top-left corner, and a move plus a resize composes any
rectangle the handles can make.

`=` and `-` are the same resize, four cells at a time on both axes — one press is the
difference between a pane that is technically open and a pane you can work in. They move no
other pane, avoid no collisions and fit nothing to your content; they are the shifted arrows
with a bigger step, so everything the arrows refuse they refuse the same way (an axis that
cannot legally shrink keeps what it had while the other axis still moves).

A right-click while arranging **leaves the interaction and does nothing else** — it never
also opens the context menu. The next right-click, in ordinary Workshop, does.

Which panes are on the desk at all is the **picker**'s job (`p`), before and after any of
this — arranging never adds or offers a pane.

Everything you author here goes into the **setup**, so it survives if you save it with `s`.
See [setups](setups.md).

## The context menu — what can I do with this?

**Right-click anything** and a small menu opens **beside the click**, listing what can be
done with the thing you pointed at — the same operations Workshop's keys already perform,
aimed at that thing:

- **a pane** — `arrange`, `Order >` (front / back / raise / lower), `Reset >`
  (place / width / height), `remove`;
- **a document object** — `delete`;
- **the empty room** — Workshop's own doors: a new object, the picker, arrange desk, the
  terminal, attention, the hotkey view, save / open, the setup gestures, reset order.

The menu is sized by what it has to say, and near a screen edge it shifts just enough to
stay whole. Where a row's action has a working shortcut in the place you are returning to,
the menu shows it after the label — those spellings are the live keymap's, so remapping a
binding moves them, and a key that would not work there is simply not shown.

While the menu is open: `↑` `↓` choose a row, `Enter` chooses it (a `… >` row opens its
group, staying beside the click), `Esc` backs out of a group or closes the menu, and a
click outside dismisses it — a click spent on closing the menu never also operates
whatever it landed on. **`a`** opens the same menu from the keyboard, on the selected
object or the empty room, so the capability does not depend on a mouse; with no pointer
position to open beside, it opens at the panel column's corner.

**Pointing is not selecting.** Opening the menu on a pane or an object changes no
selection and moves no keyboard focus — the menu holds the pointed thing only for the one
action you choose. `arrange` is the deliberate exception: choosing it begins arranging
**that pane**, because arranging is an ongoing state, and only after the pane passes the
same checks every arranging road applies.

The menu offers what is *meaningful* for that kind of thing, not a prediction of success —
choose `arrange` on the Info panel and the owner answers in its own words (`the screen owns
its place`). On a terminal, whether a right-click reaches Workshop at all is the terminal
emulator's decision first (the Windows console and Windows Terminal both hand it through);
the `a` key works everywhere.

### What "front" means

Depth is two levels and that is the whole model. Within one plane: rectangles, then labels
over them, then bounded text regions over those. Between planes: the complete earlier plane,
then the complete later one over it. There is no numeric z, no sorting, no layer identity —
front is *painted later*, and `f` `b` `r` `l` author a position in an order.

**Selecting a pane lifts it, and does not write anything down.** Pressing into a pane brings
it to the front of the desk for as long as it is the selected one; selecting another pane
hands the lift over and the first falls back to exactly where you put it. The order you
authored is untouched by any of it — save the setup after a session of clicking around and
the file holds the desk you arranged, not the last pane you happened to press. `f` `b` `r`
`l` are still how you say *and I mean this permanently*.

The pane in front is also the pane your pointer reaches: what you see on top is what a click
in the overlap lands on. Context menus and the hotkey view stay above the panes either way —
a selected pane is never lifted over a menu you just opened on it.

### What a refusal means here

A pane whose geometry cannot be projected in the current medium refuses the gesture rather
than silently doing something else. An authored size in **pixels** is legal to hold and cannot
be presented on *any* medium here — a setup legal on a window stays legal on a terminal, and
what changes with the medium is which authored intent can be *shown*, never which can be
*held*. That is a different thing from the pixel *readout* above: reading `417 px` is a face
saying your canvas geometry in its own unit, and it changes nothing about what is stored.

## Reading a pane's geometry — and whose number it is

While you are arranging, the notice line names the pane, its state, and its window — in the
units of the face you are actually looking at. On a graphical Workshop that is **pixels**; in
a terminal it is **cells**:

```text
arrange @zengine.composer/compose (open) -- @77,53 417x233 px f0
arrange @zengine.composer/compose (open) -- @~6,~4 ~34x~19 cells f0 (~ projected)
```

Those two lines are **the same pane**, unchanged, read on two faces. Your pane's geometry is
stored once, on a medium-independent grid, and each face says it in the unit that face can
actually distinguish. Where a face cannot say your number exactly, it shows its nearest
answer with a `~` in front and says `(~ projected)` once at the end of the line.

> **A geometry shown in another medium may be a projection. Workshop does not treat seeing
> that projection as authoring it.**

Nothing about looking changes what you made. Open your desk in a terminal, read it in cells,
open it again in a window, read it in pixels, save it — the file holds exactly the numbers
you authored, byte for byte. Only a real gesture — a drag, a resize key, `=`, `-`, a reset —
changes them.

If part of a pane's window is still Workshop's answer rather than yours, that part reads `-`
and the line adds where the pane currently **is**:

```text
arrange @zengine.workshop/info (open) -- -x- f1 -- now @1668,36 576x108 px
```

Once you have authored the place and both extents, the `now` clause goes away: what you
authored *is* the rectangle.

**Which unit a face uses is that face's own report, not an assumption.** A terminal has no
unit finer than its cell and says so; the graphical Workshop reports the size of one canvas
cell in its own pixels. Neither number is written into your setup file, and neither is Zen's
"real" unit — your geometry belongs to the canvas, not to anybody's monitor.

## Pane geometry

**A pane placed in the stack is 9 rows tall, and a bigger terminal does not grant more.**

That is the current default and it is a real constraint, not a rendering artefact. A wider
surface *does* widen a pane — the surplus over the 48-column minimum is split evenly between
the pane and the material under it, so at 200 columns a pane's granted width goes from 48 to
109 without any threshold. Height does not work that way: the stack slot is a fixed number of
rows.

Of any pane's rectangle, one cell on each side is its **visible edge**, so a 48×9 default
pane shows you 46×7. That is the cost of being able to see where a pane ends, and it is paid
by the pane rather than by the desk — the rectangle does not grow. If a pane's content is
tight, the answer is the same one below: make the pane bigger.

### How to actually get a bigger pane

Fastest: `w` → `Tab` to the pane → **`=`**. One press grows it four cells on both axes,
which is enough to turn every pane the shipped desk opens into one you can work in. `-`
takes it back. Then `s` `Enter` to persist the setup.

By pointer: right-click the pane → `arrange` → drag its bottom edge (or any handle) to the
size you want. In a window the drag is pixel-fine; on a terminal it moves cell by cell,
which is the honest grain a terminal has. By keyboard, `Shift`+arrows still move one cell
per press when you want to land exactly. An authored size is accepted up to the document's
cell maximum.

There is still no "fill the room", no auto-fit to a pane's contents, and no snapping — `=`
resizes the pane you addressed and touches nothing else.

Judged plainly, and repeated in [limitations](limitations.md):

| | |
|---|---|
| feature absent? | **no** — authored per-pane size exists, persists, is honoured, and a hand can drag it |
| feature undiscoverable? | **mostly not.** The band's legend advertises `w arrange desk`, the pane's context menu offers `arrange`, and entering either scope puts visible handles on the panes — the affordance is on the thing itself |
| feature tedious? | **no longer** — `=` and `-` are the coarse step; `Shift`+arrows remain the fine one |
| product-hostile? | **no**, but the default is: a 9-row pane over the material you are building is the arrangement a maker meets first |

The smallest thing left that would change the felt experience: a pane-height default that
reads the surface. That is not built, and it is not designed here.

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
