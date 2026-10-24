# Pointer spaces — reference

**Reference.** Where a reported pointer position lands, and which of Input, the window and the
consuming application owns each step. It is here rather than in one package's source because no
single package can state it.

Sources: `input/vocabulary.hpp` (`space::`), `graphics/pixels.hpp` (the grains,
`cell_of_pixel`, `px_span_contains`), `graphics/vocabulary.hpp` (`kCellPx`, the area moments),
`workshop/screen.hpp` (`canvas_point_of`).
Package overviews:
[Input](../../input/docs/input.md) ·
[Graphics](../../graphics/docs/graphics.md).

## Three spaces, not one

```text
MEDIUM      what the platform reported, in its own numbers
            terminal: terminal cells, 0-based
            window:   window pixels, 0-based at the window's origin

CANVAS      the window's pixels, twelve to a cell; the OS window shows each,
            the terminal window floors them to cells

AREA        an owner's own area, in canvas pixels from the area's corner
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

## The window owns MEDIUM → AREA

Each window reads its own platform's moments and lays its own canvas out, so the step from a
medium's numbers to a canvas pixel is the window's, and nobody else's: the terminal window draws
canvas cell (0,0) on its terminal's first cell, one cell every `kCellPx` pixels; the OS window draws
the canvas 1:1 from its origin. It hands the owner of the area under the hand the position in that
area's canvas pixels (`AreaButtonChanged`, `AreaPointerMoved`, `AreaWheelTurned`). Where a moment
is read in its medium's numbers, its **grain** travels beside the position: a window pixel is
`kPixelGrainPx` (<!-- value kPixelGrainPx -->1<!-- /value -->) canvas pixel, and a terminal cell is
`kCellGrainPx` (<!-- value kCellGrainPx -->12<!-- /value -->), its own corner, since a terminal
cannot say anything finer. A consumer that spends cells and one that spends pixels are reading one
measurement (the cell is the pixel's floor). The grain is what a hit test floors by
(`px_span_contains`), so the hand meets exactly the device units a rectangle paints.

Two rules the arithmetic depends on, both because the numbers arrive from the wire rather than
from a computation:

- `cell_of_pixel` **floors**; it does not truncate. C++ integer division truncates toward zero, so
  a plain `/` sends pixel −1 to cell 0 — a pointer one pixel to the *left* of the canvas would
  report as being inside its first column. Flooring is what makes the cell boundaries evenly
  spaced across zero.
- sums **saturate** (`add_cells`). A saturated end is far outside any canvas, which already means
  "nothing there".

A position may be outside the canvas. This is a projection, not a hit test, and "off the canvas"
is an answer.

## The consumer reads its own area

Workshop's own area is its desk: it adds the area's corner to the position the window gave
(`canvas_point_of`, in `workshop/screen.hpp`, takes a moment in cells or pixels and answers the
canvas cell and pixel and the grain). A space the consumer does not recognise is **ignored, never
guessed at**. That is the whole reason `space` exists.

## Tests

Zengine suite `graphics` (where a moment lands in an area, both windows) and suite `workshop`
(the same pointer reaching a weaver's gesture through the real message path). Their case floors
are in [`tests/test_population.txt`](../../tests/test_population.txt).
