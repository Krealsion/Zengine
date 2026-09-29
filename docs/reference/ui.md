# The UI package — authored and resolved

**Reference.** One distinction, held apart by a compile-time fence: what a weaver *authored* is
not what a viewport *makes of it*. If you are storing user-authored geometry, this is the
vocabulary; if you are painting or hit-testing it, this is what you resolve it against.

Source: [`ui/vocabulary.hpp`](../../ui/vocabulary.hpp) ·
[`ui/layout.hpp`](../../ui/layout.hpp). Link `zengine::ui`.

Two headers keep the halves apart, so which one you hold is visible in what you include. The
authored side holds no resolved number; the resolved side is only ever an observation of it.

```text
ui/vocabulary.hpp   Extent{mode, amount}     an authored width or height, one property
                    kRootContext             the context that names no element: the root
                    Element{id, label,       one authored element -- identity, label,
                            context,         WHOSE FRAME its values are read in...
                            x, y,            ...authored placement in that frame...
                            width, height}   ...and two authored extents
                    ById / walk_context      finding and following a relationship, with
                                             no viewport and no number in sight
                    authored_only_v<T>       the compile-time fence, as a question about
                                             ANY type, so an application can ask it too

ui/layout.hpp       Viewport{cells_w, cells_h}   what the root frame is made of
                    Rect / Placed{id, rect}      the resolved observation
                    Scene{viewport, items}       authored order == paint order
                    root_frame / resolve_in      authored shape + frame = resolved shape
                    resolve_extent / resolve     the one place intent becomes geometry
                    frame_in                     the frame an element was read in
                    hit / placed_for             what is under this cell -> the AUTHORED id
```

No kernel, no weave, no bus: the package is vocabulary and arithmetic, header-only over
`loom::core`, so it exists in every configuration.

## What it is not

Not a widget set, not a layout engine, and not a drawing vocabulary. There are no widget kinds,
no stacks and no relational arrangement: an element says where it is and how big it is, never
"beside" or "inside". There is no colour, no z and no style; paint order is authored order. It
knows nothing about painting: `zengine::surface::SurfaceCanvas` is the drawing vocabulary, and a
resolved scene is what you paint *from*. It shares the canvas's unit, the cell, and nothing else.

Loom's `loom::Widget` with `px_layout` is a different model, not a competitor: intent plus
*relationship*, resolved by a renderer, with no authored placement and no absolute extent. That
model refuses geometry members by name (`x`, `width` and the like), because in it any width would
be a resolved one; here a width is authored, so the fence is shaped differently
([below](#the-fence)).

## Authored intent

**An extent is one property.** A weaver authors a width, not a type and then a value, so the mode
and the amount travel together as `Extent` and are never two separately editable fields. A mode
is `kExtentCells` (the amount is cells) or `kExtentPercent` (the amount is a share, 0..100, of
the frame's span); resolution reads any other mode as cells.

**Nothing on the authored side validates.** What a legal extent, placement or relationship is
belongs to the application that accepts one, because that is the only place that can also refuse.
The package's own obligation is the other half: resolution is total over every value
([below](#resolution-is-total)).

**Identity is the id.** A context names an element by `id`, and `hit` and `placed_for` answer
with one; `label` is text for a person and is never looked up, so two elements may share a label.
A name used as an identifier is the mistake the separate fields exist to prevent. The package
mints no ids and does not require them distinct: identity policy belongs to whatever holds the
elements, and an id means nothing outside that holder's lifetime. It still answers about a
repeated id rather than assuming there is none (`ById` answers with the first position).

**Placement is authored; extent is resolvable.** `x` and `y` are cells from the frame's top-left
and resolution never reinterprets them, so there is no "50% across". `width` and `height` carry
intent a frame must interpret, so they are `Extent`s, and the fence makes a bare number there
unspellable.

### Context is a frame, not a parent

Every one of those four numbers is measured against something, and `context` says what. A
*frame* is the origin the offsets count from and the span the shares are shares *of*, as one
`Rect`: the two are one measurement, the smallest value that reads `x = 2, width = 50%` as a
rectangle. An element whose context is `kRootContext` (0, the default) is read in the root frame,
the whole viewport at the origin. An element whose context names another element is read in that
element's resolved rectangle. An element that wanted its position from one element and its size
from another would need a second context field, not a different kind of frame.

A context is an **identity, never a position**: not an index into the sequence, not a pointer,
not a place in a scene. Those change under every insertion, reallocation, save and load; an
identity is what a selection, a hit test and a file already agree on. `#4` means the element
carrying identity 4, not the fourth element.

A context says where an element's numbers are measured from, and nothing more. It does not say
the source contains, owns, clips or paints the element, must outlive it, or sits behind or in
front of it; a dependent's rectangle may extend well past its source's, and nothing trims it.
Containment, ownership, z-order and a delete policy are an application's to build on top, as
its own policy, not as properties of this field.

## Following a relationship

`ById` indexes a sequence by identity, because every use of a relationship has to find the
element carrying one: a linear search per step would make resolving or checking a sequence
quadratic in a length that a file chooses. It records positions, so it describes the sequence it
was built from.

`walk_context` follows one chain from an identity toward the root and ends in one of three ways:
`Root`, `Missing` (it names an identity nothing carries) or `Cycle` (it comes back to itself).
Missing and cyclic are separate outcomes because they are different mistakes with different
repairs. The walk is iterative and its only bound is exact: a walk that has visited more elements
than the sequence holds has visited one twice, which is what a cycle is. So how deep a legal
composition may be is not decided by how much stack a host has, and there is no authored ceiling.
A cycle is reported as one lap (`#7 -> #9 -> #7`) rather than the road into it, because "invalid
graph" tells a weaver nothing they can act on.

An application checks two things with it. A whole sequence: walk every element, sharing one
`settled` memo, which marks what reached the root, so a sequence whose chains all do costs one
visit per element. A proposed edit: before giving element `e` a new source `s`, walk from `s`
without a memo and ask `passes_through(e)`; the edit closes a loop exactly when it does. With
distinct identities, `resolve` places an element exactly when its walk reaches the root; that
holds by construction, since both find a source through `ById`, and no case compares the two.

## Resolved geometry

**Resolution needs a viewport.** `resolve` takes one, so "how big is this?" is not a question an
element can answer alone, which is why an inspector can show `60%` and `28 x 6 cells` as two
readings with neither wrong.

**The result is a separate value, cached nowhere.** A `Scene` is not stored on the elements it
observes; the authored side has no field able to hold one, and the fence makes adding one a
compile error. A resolved number that outlived its viewport would be a stale answer, so the scene
is recomputed wherever it is wanted.

**The resolved side has no wire form.** `Rect`, `Placed`, `Scene` and `Viewport` are
deliberately not `ZEN_SHAPE`s, asserted against `loom::Shape`, so an observation cannot be
serialized, poked or published as though it were content and later mistaken for it. The authored
side *is* content and travels as ordinary shapes.

**One place resolves.** Painting, reading and hit testing that all read one scene cannot fall
out of step, because there is no second copy of the geometry. `frame_in` exists for the same
reason: a gesture that needs a source's position asks the scene instead of reconstructing it.
`Placed` carries the authored id rather than a pointer to the element, unlike Loom's `PxTarget`,
which carries a `const Widget*` into the caller's tree: authored elements live in a vector that
reallocates when one is added, and the id is what survives that.

**Authored order survives composition.** An element cannot be resolved before the element it
measures against, and the obvious way to satisfy that is to sort the sequence into dependency
order. That would make document order mean dependency order and silently change which rectangle
paints over which and which one a click finds. So `resolve` orders its *work* by dependency,
iteratively on the heap, and emits its *answers* in authored order: dependency order is internal
to one function, and presentation order stays what the weaver arranged. `hit` answers the topmost
element, the last in authored order, which is the one a person sees.

### A broken chain is absent

An element whose chain cannot reach the root, through a cycle or a context no element carries,
has no frame to read its numbers in, so it is **not placed**: absent from the scene, unpainted,
unhittable, and `placed_for` answers null, which is a normal answer. It is an absence and never a
guess. Falling back to the root would resolve the element against a *different* relationship
than the one it names and show a confident rectangle in the wrong place; there is no nearest
legal reading of a broken reference the way there is of a 500% share. `resolve` cannot refuse
either, because it is total; refusing is the job of whatever accepts the sequence, with
`walk_context`. A broken chain reaches `resolve` only through content no such check saw: a poke,
the wire, or an application without the check.

With a repeated identity, `frame_in` finds a source as `placed_for` does, by the first *placed*
element carrying it, while `resolve` measured against the first element carrying it. If that
first one is not placed and a later one is, `resolve` leaves the dependent unplaced and
`frame_in` still names the later one's rectangle. Distinct identities, which an application
holding the elements ensures, rule this out.

### Resolution is total

`resolve_extent`, `add_cells`, `resolve_in` and `Rect::contains` are total over every value
their types can hold, not merely over validated ones. That is the bar for a shared vocabulary:
authored content is a shape, so it arrives from the wire and from a poke as well as from a
checked edit, and a package's arithmetic is exposed to every consumer's values. An out-of-range
share is clamped. A span too large to multiply divides first (`span * amount` on unvalidated
`int64` is signed overflow: undefined behaviour produced by data). A resolved position is a
frame's origin plus an authored offset and saturates at the ends of `int64`, far outside any
viewport. `Rect::contains` compares the far edge in unsigned arithmetic, so a rectangle whose
right edge is not representable still answers.

A share never rounds an element out of existence: it resolves to at least `kMinCells`, one cell,
because an element a weaver authored is one they meant to see. A cells extent is taken as written.

## The fence

The fence is two claims, exposed as traits over any type so an application can hold its own
authored type to them, and so the fence can be shown firing, which an assertion nobody violates
cannot show:

1. **Type-aware, and airtight for what it names** (`extents_are_authored_v`). A resolvable
   dimension is an `Extent`, never a number, and an integer does not convert to one: writing the
   resolved `28` into the authored `width` does not compile.
2. **Name-based, and not airtight** (`carries_no_resolved_geometry_v`). No member is spelled like
   a resolved rectangle: `w`, `h`, `right`, `bottom`, `rect`, `resolved`, `cells`, `pixels`. A
   resolved number under another name, `extent_now` say, passes; this half backs review and does
   not replace it.

`authored_only_v` is both. `ui/vocabulary.hpp` asserts it of `Element`. The compile entries
`ui_authored_extent_required` (a bare `int64_t width`) and `ui_resolved_geometry_refused` (a
resolved `w`/`h` cached beside honest extents) show each half refusing an application's type, and
`ui_authored_element_compiles` is their positive control, so a refusal cannot be a fixture that
failed to build for another reason.
