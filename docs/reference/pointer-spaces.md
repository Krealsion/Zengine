# Pointer spaces — reference

**Reference.** Where a reported pointer position lands, and which of Input, Surface and the
consuming application owns each step. It is here rather than in one package's source because no
single package can state it.

Where a reported pointer position lands, and who is allowed to say. This is the
one contract that spans three packages — Input stamps a coordinate's space,
Surface owns each medium's projection onto the canvas, and the consuming
application owns the pairing between them — so it lives here rather than in any
one of their sources.

Sources: `input/vocabulary.hpp` (`space::`), `surface/pointing.hpp`,
`surface/vocabulary.hpp` (`kCanvasCellPx`, `kCellSubs`). Package overviews:
[Input](input.md) ·
[Surface](surface.md).

## Three spaces, not one

```text
MEDIUM      what the backend reported, in its own numbers
            terminal: terminal cells, 0-based
            window:   window pixels, 0-based at the window's origin

CANVAS      cells of the SurfaceCanvas the active Skin is painting

APPLICATION whatever composition the publisher chose -- e.g. Workshop's
            workspace, which sits at an offset ON the canvas
```

A pointer crosses all three. Each boundary has exactly one owner.

## Input stamps the space; it does not project

`PointerMoved`, `PointerButton` and `PointerWheel` each carry a `space` field
alongside their coordinates: `space::kCells`, `space::kPixels`, or
`space::kUnknown`. It exists because a terminal cell and a window pixel are both
small non-negative integers and nothing else tells them apart — a position
without a unit is the same defect as a button without a position.

Input never converts. It reports the moment the backend actually observed
(README: *Input reports coherent MOMENTS; applications interpret GESTURES*), and
the numbers in that moment are the medium's own.

## Surface owns MEDIUM → CANVAS, one function per medium

The offset from a medium's numbers to a canvas cell is a fact about how that
Skin lays a canvas out, which is precisely what a Skin decides and what nobody
outside the package can see. Both live in `surface/pointing.hpp`:

| function | rule | why that number |
|---|---|---|
| `canvas_of_terminal_cells(x, y)` | `y - kTuiCanvasTopRow`, saturating | the terminal Skins write `\x1b[3;1H`, because terminal rows 1–2 carry the `SurfaceText` slots |
| `canvas_of_window_pixels(x, y)` | `cell_of_pixel` on each axis | the canvas starts at the window's ORIGIN — `plan_canvas` draws cell (0,0) at pixel (0,0), no margin and no scaling, one cell every `kCanvasCellPx` pixels |

Since WUX-2 each projection has a **fine twin one lattice down**
(`canvas_subs_of_window_pixels`, `canvas_subs_of_terminal_cells`), answering in
*sub-units* — 1/`kCellSubs` (48) of a cell — with the reporting medium's **grain**
travelling beside the position: a window pixel is `kPixelGrainSubs` (4) sub-units,
a terminal cell is `kCellGrainSubs` (48), and a consumer that spends cells and one
that spends subs are reading one measurement (the cell is the sub's floor). The
grain is what a hit test floors by (`sub_span_contains`), so the hand meets
exactly the device units a fine rectangle paints — the pane-arrangement consumer
this was built for is Workshop's, whose `PointedAt` carries all three.

Since G-2 the window is **not** exactly the canvas: it is user-resizable, so it
can be a few pixels wider than a whole number of cells, and a publisher that
ignores `SurfaceExtent` can leave it much wider than that. The transform is
unchanged by that, and the reason is worth stating because the old wording
rested on the coincidence: what this arithmetic depends on is the canvas's
ORIGIN, not its extent. The medium only ever grows down and to the right, so
the origin does not move. A pointer landing on a cell no canvas has is already
this function's documented answer — see below.

Two rules the arithmetic depends on, both because the numbers arrive from the
wire rather than from a computation:

- `cell_of_pixel` **floors**; it does not truncate. C++ integer division
  truncates toward zero, so a plain `/` sends pixel −1 to cell 0 — a pointer one
  pixel to the *left* of the canvas would report as being inside its first
  column. Flooring is what makes the cell boundaries evenly spaced across zero.
- the terminal subtraction **saturates** (`add_cells`). Whichever weave holds
  the input role is a weave like any other, and `INT64_MIN - 2` is undefined
  behaviour produced by data. A saturated end is far outside any canvas, which
  already means "nothing there".

A `CanvasPoint` may be outside the canvas. This is a projection, not a hit test,
and "off the canvas" is an answer.

## The consumer owns the PAIRING, and that is the honest cost

Which medium's transform applies to a given event is decided from the `space`
the backend stamped — and that decision belongs to the consumer, because
**nothing in the process can ask the active Skin what its presentation context
is**. A consumer states the pairing in one place (Workshop's `canvas_point_of`
is one switch) rather than pretending it is derived.

A space the consumer does not recognise is **ignored, never guessed at**. That
is the whole reason `space` exists.

Surface deliberately declines to make this decision for anyone: there is no
registry, no transform graph, no presentation-context query and no dispatch on a
backend identity. Pushing the pairing into `pointing.hpp` would be the package
quietly deciding that `kPixels` can only ever mean this window.

## The open seam, stated so it is recognisable

The pairing is true today because there is exactly one terminal layout and one
graphical one. A **second graphical Skin with a different layout** would report
`kPixels` too, and no consumer switch could tell the two apart. That is the day
a presentation-context capability — a way to ask the active Skin how it lays a
canvas out — has to be answered rather than deferred. It is not built, and
nothing here pretends otherwise.

## Tests

Zengine suite `surface` (where a reported pointer lands on the canvas, both
media, as pure arithmetic on every lane) and suite `workshop` (the same pointer
reaching a maker's gesture through the real message path). Their case floors are
in [`tests/test_population.txt`](../../tests/test_population.txt).
