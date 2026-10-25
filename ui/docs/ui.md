# The UI package — authored and solved

**Reference.** One distinction, held apart by a compile-time fence: what a weaver *authored* is
not what a room *makes of it*. A part says where it stands and how big it is in words -- a share
of its frame, its own measure, what its frame leaves, a point of its frame -- and one solver turns
those words into whole-pixel rectangles for a given room. If you are storing authored layout, this
is the vocabulary; if you are painting or hit-testing it, this is what you solve it with.

Source: [`ui/vocabulary.hpp`](../vocabulary.hpp) ·
[`ui/layout.hpp`](../layout.hpp). Link `zengine::ui`.

Two headers keep the halves apart, so which one you hold is visible in what you include. The
authored side holds no solved number; the solved side is only ever an observation of it.

```text
ui/vocabulary.hpp   Size{mode, amount?}      a width or a height: its word, and its number
                                             where the word reads one
                    Place{mode, x?, y?}      where it stands: a point of its frame, and the
                                             offsets that point reads
                    Element{id, label,       one authored part -- identity, label,
                            context,         WHOSE FRAME it stands in...
                            place,           ...its place in that frame...
                            width, height,   ...two sizes...
                            flow, gap?}      ...and how it lays the parts standing in it
                    size_word / place_word   the one table that spells the words,
                    size_named / place_named and reads a word back to its number
                    ById / walk_context      finding and following a relationship, with
                                             no room and no number in sight
                    problem                  what is wrong with the intent alone
                    authored_only_v<T>       the compile-time fence, as a question about
                                             ANY type, so an application can ask it too

ui/layout.hpp       Room{w, h, step}             the root frame, and the window's step
                    Measure{w?, h?}              a part's own measure, said before the solve
                    Rect / Placed{id, rect}      the solved observation, in whole pixels
                    Scene{room, items}           authored order == paint order
                    arrange                      the one place intent becomes geometry
                    root_frame / frame_in        the frame a part stood in
                    hit / placed_for             what is under this pixel -> the AUTHORED id
```

No kernel, no weave, no bus: the package is vocabulary and arithmetic, header-only over
`loom::core`, so it exists in every configuration.

## What it is not

Not a widget set and not a drawing vocabulary. There are no widget kinds, no colour, no z and no
style; paint order is authored order. It knows nothing about painting: `zengine::graphics` is the
drawing vocabulary, and a solved scene is what you paint *from*. Its unit is the whole pixel; a
cell is a terminal window's step and nothing more. A frame lays its parts freely, each by its own
words, or down a column that begins again at its top. No frame divides its span among its parts,
and no part is placed beside another: a part is placed in its frame, never relative to a sibling.

Loom's `loom::Widget` with `px_layout` is a different model, not a competitor: intent plus
*relationship*, resolved by a renderer, with no authored placement and no absolute extent. That
model refuses geometry members by name (`x`, `width` and the like), because in it any width would
be a resolved one; here a width is authored, so the fence is shaped differently
([below](#the-fence)).

## Authored intent

**A size is one property.** A weaver authors a width, not a type and then a value, so the word and
the number travel together as `Size` and are never two separately editable fields:

| word | number | the part is |
|---|---|---|
| `default` | none | the owner's answer: the measure the owner says, else the frame's span |
| `pixels` | a count | exactly that many pixels |
| `share` | 1 to 10,000 | that many ten-thousandths of the frame's span (`kShareWhole` is the whole) |
| `fit` | none | its own measure, rounded up to the step; with none said, the reach of its own parts |
| `fill` | none | from its anchored edge to the frame's far one; centred, the whole span |

**A place is a point of the frame.** `Place` says which point a part's offsets count from, and
each offset is whole pixels inward from the edge the word names -- or, for `share`, ten-thousandths
of the frame's width and height from its top-left:

| word | `x` | `y` |
|---|---|---|
| `default` | none: the frame's left | none: the frame's top |
| `pixels` | from the left | from the top |
| `top`, `bottom` | none: centred across | from that edge |
| `left`, `right` | from that edge | none: centred down |
| `top-right`, `bottom-left`, `bottom-right` | from that corner's edge | from that corner's edge |
| `center` | none | none |
| `share` | ten-thousandths of the width, from the left | ten-thousandths of the height, from the top |

A part standing at `right` has its own right edge `x` in from the frame's right edge. A centred axis
carries no offset. A part at `share` with `y` 3333 stands a third of the way down at every size of
the frame: its edge is the exact position floored to the step, as a share size's is, and each
offset is clamped to the frame as a share's amount is (`place_reads_share`).

**A number is present exactly where its word reads it.** Each number is a Loom optional field
(`std::optional`): `amount` on `pixels` and `share`, `x` and `y` where the place reads them, `gap`
on a column (`size_reads_amount`, `place_reads_x`, `place_reads_y` say where). A number on a word
that reads none is not read. The words are numbers in the shape and spelled by one table,
`size_word` and `place_word`, so every owner spells them alike, and read back by the same table
(`size_named`, `place_named`); a number no word is has no spelling, and solving reads it as
`default`.

**Nothing on the authored side validates.** What a legal size, place or relationship is belongs to
the application that accepts one, because that is the only place that can also refuse. The
package gives that application two things: `problem`, which asks the intent alone for what no room
could solve ([below](#a-size-that-depends-on-itself)), and a solve that is total over every value
([below](#solving-is-total)).

**Identity is the id.** A context names a part by `id`, and `hit` and `placed_for` answer with
one; `label` is text for a person and is never looked up, so two parts may share a label. A name
used as an identifier is the mistake the separate fields exist to prevent. The package mints no
ids and does not require them distinct: identity policy belongs to whatever holds the parts, and
an id means nothing outside that holder's lifetime. It still answers about a repeated id rather
than assuming there is none (`ById` answers with the first position).

### Context is a frame, not a parent

Every number a part carries is measured against something, and `context` says what. A *frame*
is the origin the offsets count from and the span the shares are shares *of*, as one `Rect`: the
two are one measurement. A part whose context is `kRootContext`
(<!-- value kRootContext -->0<!-- /value -->, the default) stands in the root frame, the whole
room at the origin. A part whose context names another part stands in that part's solved
rectangle.

A context is an **identity, never a position**: not an index into the sequence, not a pointer,
not a place in a scene. Those change under every insertion, reallocation, save and load; an
identity is what a selection, a hit test and a file already agree on. `#4` means the part carrying
identity 4, not the fourth part.

A context says where a part's numbers are measured from, and nothing more. It does not say the
frame contains, owns, clips or paints the part, must outlive it, or sits behind or in front of
it; a part's rectangle may extend well past its frame's, and nothing trims it. Containment,
ownership, z-order and a delete policy are an application's to build on top, as its own policy,
not as properties of this field.

### How a frame lays its parts

`flow` says how a part lays the parts standing in it. **`kFlowFree`**, the default: each part by
its own place and sizes; parts may cover each other. **`kFlowStack`**: down a column, in authored
order, a slot each. A part's slot is its measured height, or its own height where none is said;
the next slot starts `gap` pixels below it; and where a slot would pass the column's floor, the
column begins again at its top. Across, a part in a column stands as its place says; down, at its
slot's top, drawn at its own height, so a part drawn taller or shorter than its slot leaves its
neighbours where they are.

## Following a relationship

`ById` indexes a sequence by identity, because every use of a relationship has to find the part
carrying one: a linear search per step would make solving or checking a sequence quadratic in a
length that a file chooses. It records positions, so it describes the sequence it was built from.

`walk_context` follows one chain from an identity toward the root and ends in one of three ways:
`Root`, `Missing` (it names an identity nothing carries) or `Cycle` (it comes back to itself).
Missing and cyclic are separate outcomes because they are different mistakes with different
repairs. The walk is iterative and its only bound is exact: a walk that has visited more parts
than the sequence holds has visited one twice, which is what a cycle is. So how deep a legal
composition may be is not decided by how much stack a host has, and there is no authored ceiling.
A cycle is reported as one lap (`#7 -> #9 -> #7`) rather than the road into it, because "invalid
graph" tells a weaver nothing they can act on.

An application checks a proposed edit with it: before giving part `e` a new frame `s`, walk from
`s` without a memo and ask `passes_through(e)`; the edit closes a loop exactly when it does. A
whole sequence it checks with `problem`, which walks every part sharing one `settled` memo, so a
sequence whose chains all reach the root costs one visit per part.

### A size that depends on itself

`problem(parts)` answers the intent's first problem in authored order, with no room and no
measure, in a sentence that names the part and what would repair it; empty when there is none:

- a context no part carries, and a context that loops;
- a part that reads its frame's span -- a `share`, a `fill`, or a place counted from the frame's
  far edge or centre -- along an axis where the frame's size is `fit`: the frame is measured from
  the part and the part from the frame;
- a column that begins again at its floor and fits its own height to its parts.

These are refused from the intent alone, by whoever accepts the sequence, before anything is
solved. What depends on the room -- parts that overrun a small one, a share that rounds below a
step -- is not a problem: the solve answers it.

## Solved geometry

**Solving needs a room.** `arrange(parts, room, measures)` takes one, so "how big is this?" is
not a question a part can answer alone, which is why an inspector can show `50%` and `336 px` as
two readings with neither wrong. The room carries the window's **step**: the smallest span its
window shows, 1 pixel in a window of pixels and a cell in a terminal. Each window solves the same
intent at its own step; neither floors the other's answer.

**Measures are values, said before the solve.** `measures[i]` is `parts[i]`'s own measure, per
axis, from its owner: what `default` and `fit` read. The solve never asks a part anything, so
nothing it answers can depend on the room it is solving.

**Edges, not widths, are rounded.** A pixel offset and a pixel length are exact at every step: a
terminal floors them when it shows them. An edge that a share or a centring computes is the exact
position floored to the step, counted from the frame's start. Two parts not solved together --
one a share from the frame's start, one the rest from its end -- name one edge, and so meet on one
pixel, at every span and every step; rounding each width alone would leave a gap or an overlap.

**The result is a separate value, cached nowhere.** A `Scene` is not stored on the parts it
observes; the authored side has no field able to hold one, and the fence makes adding one a
compile error. A solved number that outlived its room would be a stale answer, so the scene is
recomputed wherever it is wanted.

**The solved side has no wire form.** `Rect`, `Placed`, `Scene`, `Room` and `Measure` are
deliberately not `ZEN_SHAPE`s, asserted against `loom::Shape`, so an observation cannot be
serialized, poked or published as though it were content and later mistaken for it. The authored
side *is* content and travels as ordinary shapes.

**One place solves.** Painting, reading and hit testing that all read one scene cannot fall out
of step, because there is no second copy of the geometry. `frame_in` exists for the same reason: a
gesture that needs a frame's position asks the scene instead of reconstructing it. `Placed`
carries the authored id rather than a pointer to the part, unlike Loom's `PxTarget`, which carries
a `const Widget*` into the caller's tree: authored parts live in a vector that reallocates when one
is added, and the id is what survives that.

**Dependency order inside, authored order out.** A part cannot be placed before the frame it
stands in, and a frame that fits its parts cannot be sized before them. The obvious way to satisfy
that is to sort the sequence into dependency order, which would make document order mean
dependency order and silently change which rectangle paints over which and which one a click
finds. So `arrange` orders its *work* by dependency, iteratively on the heap -- the reach of a
frame that fits its parts after those parts, every frame's rectangle before the parts in it -- and
emits its *answers* in authored order. `hit` answers the topmost part, the last in authored order,
which is the one a person sees.

### A broken chain is absent

A part whose chain cannot reach the root, through a cycle or a context no part carries, has no
frame to read its numbers in, so it is **not placed**: absent from the scene, unpainted,
unhittable, and `placed_for` answers null, which is a normal answer. It is an absence and never a
guess. Falling back to the root would solve the part against a *different* relationship than the
one it names and show a confident rectangle in the wrong place; there is no nearest legal reading
of a broken reference the way there is of a share past the whole. `arrange` cannot refuse either,
because it is total; refusing is the job of whatever accepts the sequence, with `problem`. A
broken chain reaches `arrange` only through content no such check saw: a poke, the wire, or an
application without the check.

With a repeated identity, `frame_in` finds a frame as `placed_for` does, by the first *placed*
part carrying it, while `arrange` stood the part in the first part carrying it. If that first one
is not placed and a later one is, `arrange` leaves the dependent unplaced and `frame_in` still
names the later one's rectangle. Distinct identities, which an application holding the parts
ensures, rule this out.

### Solving is total

`arrange`, `add_px`, `sub_px` and `Rect::contains` are total over every value their types can
hold, not merely over validated ones. That is the bar for a shared vocabulary: authored content is
a shape, so it arrives from the wire and from a poke as well as from a checked edit, and a
package's arithmetic is exposed to every consumer's values. An out-of-range share is clamped to the
whole. A span too large to multiply divides first (`span * amount` on unvalidated `int64` is
signed overflow: undefined behaviour produced by data). A solved position is a frame's origin plus
an authored offset and saturates at the ends of `int64`, far outside any room. A step of none, or
less, is one pixel. A word no word is reads as `default`. `Rect::contains` compares the far edge
in unsigned arithmetic, so a rectangle whose right edge is not representable still answers.

A part that reads its frame's span, in a frame that fits that axis to its parts -- the intent
`problem` refuses -- is solved as `fit`: its measure where one is said, else the reach of its own
parts, its place read from the frame's start. The solve never recurses and never refuses.

## The fence

The fence is two claims, exposed as traits over any type so an application can hold its own
authored type to them, and so the fence can be shown firing, which an assertion nobody violates
cannot show:

1. **Type-aware, and airtight for what it names** (`extents_are_authored_v`). A width and a
   height are `Size`s, never numbers, and an integer does not convert to one: writing the solved
   `336` into the authored `width` does not compile.
2. **Name-based, and not airtight** (`carries_no_resolved_geometry_v`). No member is spelled like
   a solved rectangle: `w`, `h`, `right`, `bottom`, `rect`, `resolved`, `cells`, `pixels`. A
   solved number under another name, `extent_now` say, passes; this half backs review and does
   not replace it.

`authored_only_v` is both. `ui/vocabulary.hpp` asserts it of `Element`. The compile entries
`ui_authored_extent_required` (a bare `int64_t width`) and `ui_resolved_geometry_refused` (a
solved `w`/`h` cached beside honest sizes) show each half refusing an application's type, and
`ui_authored_element_compiles` is their positive control, so a refusal cannot be a fixture that
failed to build for another reason.
