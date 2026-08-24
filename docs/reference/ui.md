# The UI package — authored and resolved

**Reference.** One distinction, held apart by a compile-time fence: what a maker *authored* is
not what a viewport *makes of it*. If you are storing user-authored geometry, this is the
vocabulary; if you are painting or hit-testing, this is what you resolve against.

Source: [`ui/vocabulary.hpp`](../../ui/vocabulary.hpp) ·
[`ui/layout.hpp`](../../ui/layout.hpp).

This package owns exactly **one** distinction: what a maker *authored* is not what a
viewport *makes of it*. Two headers keep the halves apart, and the second is only ever an
observation of the first.

```text
ui/vocabulary.hpp   Extent{mode, amount}     an authored width or height, one property
                    kRootContext             the identity that is not one: "the root"
                    Element{id, label,       one authored element -- identity, label,
                            context,         WHOSE FRAME its values are read in...
                            x, y,            ...authored placement in that frame...
                            width, height}   ...and two authored extents
                    authored_only_v<T>       the compile-time fence, as a question about
                                             ANY type, so an application can ask it too
                    ById / walk_context      finding and following a relationship, with
                                             no viewport and no number in sight

ui/layout.hpp       Viewport{cells_w, cells_h}   what the ROOT frame is made of
                    Rect / Placed{id, rect}      the resolved observation
                    Scene{viewport, items}       authored order == paint order
                    root_frame / resolve_in      authored shape + context = resolved shape
                    resolve_extent / resolve     the ONE place intent becomes geometry
                    frame_in                     the frame an element was read in
                    hit / placed_for             what is under this cell -> the AUTHORED id
```

Four things are structural rather than promised:

- **Resolution needs a context.** `resolve()` takes a viewport, so "how big is this?" is not an
  answerable question about an element alone.
- **A context is a FRAME, and the root is one of them.** The origin an
  element's `x`/`y` are counted from and the span its shares are shares *of*, as one value
  (`Rect`). Before it, that frame was two hard-coded assumptions in one statement of `resolve` —
  origin `0,0`, span the whole viewport. An element now names an *identity* whose resolved
  rectangle supplies it, or says nothing and gets the root's. `resolve` orders its own work by
  dependency, iteratively and on the heap (no depth ceiling, no recursion), and emits its items
  in **document** order, because document order is paint/hit/list order and must not silently
  become dependency order. A chain that cannot reach the root — a cycle, or a source nothing
  carries — is **not placed at all**: an absence, never a guess at the root.
- **The result is a separate value, cached nowhere.** The authored side has no field able to
  hold a resolved rectangle, and the fence makes adding one a compile error — proven *firing*
  by `ui_authored_extent_required` and `ui_resolved_geometry_refused` (a bare `int64_t width`,
  and a resolved `w`/`h` cached beside honest extents), with `ui_authored_element_compiles` as
  the positive control.
- **The resolved side has no wire form.** `Rect`/`Placed`/`Scene`/`Viewport` are deliberately
  not `ZEN_SHAPE`s (asserted against `loom::Shape`), so an observation cannot be serialized,
  poked, or published as though it were content. The authored side *is* content and travels as
  ordinary shapes.

`resolve_extent` is **total for every value the type can hold**, not merely for validated ones:
authored content is a shape, so it arrives from the wire and from a poke as well as from a
checked edit. An out-of-range share is clamped and an absurd span divides before it multiplies
(`span * amount` on unvalidated `int64` is signed overflow — undefined behaviour produced by
data). *What* a legal extent is stays with whoever accepts one; see Workshop's `check_extent`.

What it is **not**: no widget kinds, no stacks, no relational arrangement, and **still no
parent/child**. An element says what
its values are *measured against*; it says nothing about containment, ownership, clipping,
painting or lifetime, and a dependent's rectangle may extend well past its source's with nothing
trimming it. "Put B inside A" is something an application could build on this; Workshop builds
exactly one policy over it (a source with dependents is not deletable) and keeps that policy in
its own document law. Also no colour, no z, and nothing about
painting. `SurfaceCanvas` is the drawing vocabulary; a resolved scene is what you paint *from*.
The Loom's `loom::Widget` + `px_layout` is the *other* model — intent plus **relationship**,
resolved by a renderer — and it is neither relocatable (the Loom console, TUI and bridge
consume it in-tree) nor able to express an authored placement or an absolute extent. The two are not competitors and neither replaces the other.

No kernel, no weave, no bus: this package is vocabulary and arithmetic, so it exists on every
configuration.
