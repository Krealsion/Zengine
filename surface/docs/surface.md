# The Surface package

**Reference.** The drawing vocabulary: what a weave publishes to be drawn, the rules for choosing
between its shapes, and the arithmetic every publisher and Skin shares. If you are giving a weave a
face, this is what you publish; what the Skin holding `zengine.skin` does with each message is
[the Skin's manual](zengine.skin.md).

Source: [`surface/vocabulary.hpp`](../vocabulary.hpp) ·
[`surface/region.hpp`](../region.hpp) ·
[`surface/skin_tui.hpp`](../skin_tui.hpp) ·
[`workshop/pane_text_fit.hpp`](../../workshop/pane_text_fit.hpp) ·
[`graphics/pixels.hpp`](../../graphics/pixels.hpp).

Visual intent in, output out. No game, world, or pane weave talks to the terminal, a window,
or a renderer: they **publish** intent, and a **Skin** — a replaceable loadable weave holding
the singleton `zengine.skin` office — claims the actual surface and paints it
([the Skin](zengine.skin.md)).

The vocabulary is deliberately tiny: `SurfaceText{slot, text}` (a line of **plain** text for a
named slot — "status", "score"; styling is the skin's business) and `SurfaceReady` (the active
skin's hello, published once per incarnation on its first message; text publishers re-publish
their current line on hearing it, so a fresh painter starts complete — the tally line survives
the painter being replaced mid-game).

`SurfaceCanvas{width, height, layers}` is the **general** canvas: an extent in canvas pixels and an
ordered list of `SurfaceLayer{rects, labels, texts}` — each one a complete plane of filled
`SurfaceRect`s in painter's (list) order, `SurfaceLabel` text runs over them, and bounded text
regions over those. Each
element carries a semantic **role** — `kFill`/`kAccent`/`kMuted`/`kAlert`/`kGround`/`kMediumGround`/`kGuide` — never a colour, so
the terminal media pick an SGR *and a glyph* per role (colour alone would be a lie on a
monochrome terminal) while the SDL medium picks RGB, from one unchanged publisher. Every
coordinate and extent is a whole **canvas pixel**, `kCellPx` (<!-- value kCellPx -->12<!-- /value -->) to a cell: the window
draws the picture 1:1, and a terminal — whose coarsest address is a cell — floors it to the cells
it covers, so one picture lands somewhere real in every medium. It is a *drawing* vocabulary and pointedly not a layout one — no parent/child, no
anchors, no percentages; whoever publishes has already decided where things go.

`kGround` means opaque empty material. A rectangle bearing it covers earlier material
with black in the shipped SDL skin, or spaces on a black background in the terminal skin.
Publish it before the content it sits beneath; the ordinary clipping and painter order
still apply. It is also available as text ink or a row background, so publishers must
choose contrasting roles. It does not change the surface's default background.

`kMediumGround` is that default background itself: opaque, empty material in what the medium
shows where nothing is published — the window's own dark ground in the SDL skin, and in the
terminal skin whatever the terminal wears, light or dark, since it writes no ground colour there.
A rectangle or a row's background may name it; it is not an ink, and text named in it is drawn
as `kFill`. Plain text -- `kFill`'s ink -- is the terminal's own text colour in the terminal skin
where the ground is the terminal's own, so it reads on whatever the terminal wears, and reverse
video, a caret's or a selection's, swaps the two; on a ground the canvas painted, such as a
pane's black, it is the palette's white, as that ground is the palette's. The window keeps its
own colours.

![Inventory and Info in a dark terminal and in a light one, each drawing its rows on the terminal's own ground: plain text in that terminal's own text colour; Inventory's name line, Input sample, with sample selected and the caret on its s; Info's Width draft, 480, with 80 selected](images/terminal-plain-text-light-dark.png)

**Which of the two kinds of text a publisher chooses is one question, and it is not about
importance**: *is the rectangle mine?* A `SurfaceTextRegion` is the
semantic shape — it is the only one a graphical medium sets in real type — and it answers that
question with its `ground`:

| the rectangle is… | say | what the medium does |
|---|---|---|
| **mine** | `kGroundOwn` (the default) | clears the whole of it before a row is drawn: spaces in a character medium, its own fill in a graphical one |
| **somebody else's, and I am writing ON it** | `kGroundBeneath` | draws the rows and nothing else — no padding, no fill, so material published beneath shows wherever a glyph does not |
| **not a rectangle at all — this CELL is the meaning** | a `SurfaceLabel` | one cell per byte, in every medium |

So ordinary tool prose, headings, lists and controls are ordinary regions; a weaver's name written
across an authored object is a `kGroundBeneath` region; and `SurfaceLabel` stays exactly right
where the cell itself is the unit — one affordance glyph over the ring that fills it, chrome
sharing a row with another publisher's sentence. None of the three is deprecated and none is a
fallback for the others. One arithmetic decides the rest for you: a canvas cell is
`kCellPx` and this repository's face has an 18-pixel line, so `fit_region` answers *zero*
rows for a region one cell tall and hands it back to the cell projection — a single-row label
published as a single-cell region is the same picture it always was.

**`ground` is not a row's `background`, and they must not be read as one field**. A row
that names no background defers to its region; a region has nothing to defer to, so its two
answers are about *ownership* rather than about ink — which is why the default here is the
opposite of the default there, and why `kGroundBeneath` is not spelled `role::kNone`. It is also
not transparency: there is no blend, no opacity and no order of its own. The region is in exactly
the plane its publisher put it in, and `kGroundBeneath` removes one fill.

**Painter's order is two levels, and that is the whole of the depth model**. Inside a
plane: rects in list order, then labels over them, then text regions over those. Between planes:
the complete earlier plane, then the complete later one over it. `layers[0]` is back-most and
`layers[n-1]` front-most. There is deliberately no coordinate transform, no opacity, no clipping
tree, no layer identity or key, no numeric z, no sorting and no hit testing — a layer is a
position in a vector, the publisher supplies an already-ordered list, and each Skin executes it.
An unknown role paints as `kFill` rather than vanishing, and elements outside the extent are the
Skin's to clip. Every medium draws labels, one cell a byte, and draws something visible for a byte
it has no glyph for rather than nothing; what each Skin draws them in is
[the Skin's](zengine.skin.md#surfacecanvas).

`SurfaceTextRegion{x, y, w, h, rows, caret_row, caret_col, ground}` is the **one place a canvas admits a medium may set
its own type**. It is placed in canvas pixels like everything else — so where it sits is
the same kind of fact as where a rect sits, and every medium can honour it — and what happens
*inside* is the medium's: a terminal draws one `SurfaceTextRow{text, role}` per cell row of the
cells the region covers, cut at its width and dropped past its height; a window that has a real
face open draws the rows at its own advance and line height, inside the region's own pixels,
clipped to its own viewport. Neither is pretending: the terminal is never asked to invent a pixel, and the window
is never asked to round its type onto a twelve-pixel lattice. The cell projection is
`surface/region.hpp`'s `project_text_regions`, one function shared by the terminal skins *and*
by the SDL medium whenever it has no font — so the lower-fidelity answer is not a stub, it is
literally the arithmetic the Terminal pane performed for itself before regions existed. Regions
are the topmost thing **in their own plane** (rects, then labels, then regions), because a region
is a grant of bounds and owns what is inside them — and a *later* plane covers an earlier one
whole, kind for kind, which is how a publisher says which of two presentations is in front
(see the painter's-order note above). **How much fits is not on the shape** and that
absence is load-bearing: `fit_region` resolves the region's bounds against the medium's text
metric, and the publisher and the medium both call it, so "how many rows and columns" has one
answer in the process. Workshop's Terminal pane publishes two, both its own.

**A row may sit on something**. `SurfaceTextRow` gained one field: `background`, a
semantic role like `role` itself, defaulting to `role::kNone` — the *absence* of a ground,
which is what every row said before this existed and is not an ink role. It is the smallest
honest answer to the one question a list has to answer: which row am I on. A terminal paints
it as an SGR background, the SDL medium fills the row's strip inside the region's own viewport,
and the bitmap face paints it as the cell's own quad — the same clear a label cell already
got, in a different ink. `role::kNone` is **negative** on purpose: the unknown-role fallback
is `kFill`, so a positive sentinel would be indistinguishable from a role a later vocabulary
added, and the failure would be silent and in the widening direction. And because colour
alone would be a lie on a monochrome terminal — the argument `glyph_for_role` already makes
one shape over — a publisher marking a row as chosen is expected to say so in the row's *text*
as well; Workshop's completion list writes `> `. Adding the field made `SurfaceTextRow`
version 2, `SurfaceTextRegion` version 2 and `SurfaceCanvas` version 3: a region's wire
identity is computed from its row type and a canvas's from its region type, so both changed
without either gaining a field of its own.

**A region may have a caret**, and it is said in the region's *own prose lattice*:
`caret_row`/`caret_col` are a row index and a column index into the rows the region carries,
never a pixel and never a canvas cell. That is what lets each medium answer it with the metric
it already resolved — a window fills a bar `kCaretWidthPx` wide at
`origin_x + caret_col * advance_px`, between two characters, and a medium whose character is a
cell shows the **cell the caret stands on, inverted**: the cell of the character it sits before,
the blank after a row's last character, and past a full row's end the row's last cell, which
still shows it. Nothing is inserted, so no character moves for a caret and a press on a cell
names the character it shows. A terminal reverses that cell and underlines it; inside a
selection, which is itself reversed, it is the one cell shown plain, still underlined. The
bitmap face fills it with the row's ink and draws the glyph in the ground, or in the selection
band where it is selected. `kNoCaret` is **negative** on purpose, the same argument
`role::kNone` makes: a prose row index is non-negative by construction, so the absence of a
caret cannot collide with a row anybody might mean. It is not a focus fact (a canvas has no
focus, and two regions may each carry one), and not blinking — there is no clock on this shape.

![A terminal's caret three ways: in the Editor, on c in the middle of abcdefgh, the cell inverted and underlined; inside the selection cde, the caret's c is the one cell shown plain and still underlined; and in the View Builder's six-cell width box typed past its end, on the box's last cell, 7, which still shows it](images/terminal-caret.png)

**A region may have a selected range**, said in the same lattice: `sel_begin_row`/`sel_begin_col`
and `sel_end_row`/`sel_end_col` are two caret-like positions — begin inclusive, end exclusive,
in **reading order** — and the text between them is what a weaver's next gesture acts on. On the
begin row the range covers from `sel_begin_col` to that row's own end, every row between whole,
and the end row up to `sel_end_col`; `workshop/pane_text_fit.hpp` owns that per-row arithmetic
in one function (`selection_span_of_row`) both media consume, so the highlight cannot cover
different characters in different media. Each medium then answers in its own voice: the character
medium sets exactly the selected cells in **reverse video** (composing with whatever ink and ground
they already wear — and a `\x1b[0m` reset restores it, so the goldens of a selection-free canvas
are byte-identical), and the graphical medium fills a **band** under the glyphs
(`kSelectionBand`, resolved per row from the same `RegionFit` that placed the text; the bitmap
face paints a selected cell's clear in the band's ink). `kNoSelection` is negative for
`kNoCaret`'s reason, and a range that is absent, empty, or not in reading order shows nothing —
a value no publisher could mean is the absence, never a guess. Which end the caret is at is not
restated: the caret fields already say it. It is *not* per-span styling — one range, meaning
selection — and not multiple selections.

**A coordinate is a whole pixel, and each medium floors at its own grain.** `SurfaceRect`,
`SurfaceLabel` and `SurfaceTextRegion` say their place and extent in canvas pixels and carry no
remainder. A publisher that thinks in whole cells multiplies once and says exactly what it
always said. Each medium resolves a value at its own grain by **one quantization law** — a
device unit of `g` canvas pixels shows the span `[L, R)` on units `[floor(L/g), floor(R/g))` —
so a terminal shows a rectangle on the cells its floored edges cover and a label from the cell
its anchor floors to, the SDL medium draws it on exactly its pixels, and whole-cell geometry
lands where it always did in both. The prose lattice (rows, columns, carets, selections) stays
the metric's business, not the canvas's.

**A region too small for a medium's own type is a CELL region in that medium**, and
that is the same sentence a zero metric already means rather than a new rule. A face's line is
not a cell — this repository's is 18 device pixels against a 12-pixel cell — so a region **one
cell tall** holds `(12 - 2*inset) / 18` = zero rows of it. Without the fallback such a region
resolves to a graphical fit with no capacity and both media then draw **nothing**:
`plan_text_regions` skips a fit with no rows, and `plan_canvas` has already decided the
regions were the other list's. A bounded region that silently vanishes is the one answer
`fit_region` exists to make impossible, so it falls back — and because the publisher
asks the same function for its capacity, the publisher, the window and the terminal all get
one answer. The split between the two draw lists is the predicate
`fit_region(r, metric).graphical()` rather than a test on the metric alone, so they remain
exactly disjoint and exactly complete.

## What the Skin says back

`SurfaceExtent{width, height, text_advance_px, text_line_px, cell_px}` is the one fact that
travels the *other* way — a medium answering how much room it has, in canvas pixels, how big
one character of its own type is, in its own device pixels, and how many of those a canvas cell
is. Every other shape here is intent flowing publisher → skin; this flows skin → publisher, and
it exists because "how many cells is there room for" is a fact **only the medium holds**. The
active skin publishes it when the answer CHANGES and at no other time
([when each says it](zengine.skin.md#follow)), and a medium with no answer — a window skin before
its window exists, a terminal skin with no terminal — publishes **nothing** rather than
publishing zeroes: "I have no opinion" and "there is no room" are different sentences. It is an
*offer*: a publisher that ignores it keeps publishing whatever extent it likes and the skin clips,
which is the contract `SurfaceCanvas` already states.

How big a terminal is, the one question here put to an operating system, is the terminal window's
(`graphics/window/terminal_io.hpp`):
`ioctl(TIOCGWINSZ)` on POSIX, `GetConsoleScreenBufferInfo`'s
**visible window** on Windows (never `dwSize`, which is the 9,001-row scrollback buffer). A
redirected, piped or captured run has no terminal at the far end, and measures nothing.

**The metric exists because exactly one party may measure in a sizing conversation**, and for
text that party has to be the application: the Terminal pane chooses which transcript entries
it can show *whole* and then says how many it left out, and a medium that wrapped on its own
behalf would make that sentence false. So the medium measures its face once and publishes the
*result*; the application does the arithmetic. **Zero means "text is a cell"** — the honest
answer for every terminal skin, for a window before its font opens, and for a window whose font
*failed* to open, because in all three the thing actually being painted is a cell-sized glyph.
What the metric deliberately does not carry is a family, a filename, a point size, an ascent, a
hinting mode or a DPI: an application needs the result of measurement, not the mechanism, and
every one of those fields would be a fact about one backend that a second backend would have to
fake.

**The skin owns the desktop placement too, in both directions.**
`SurfacePlacement{x, y, maximized}` is the second skin → publisher fact: where the skin's
window sits on its desktop, in the *skin's own desktop units* — the **normal** window's
top-left (sampled only while unmaximized and remembered across a maximized stretch), with the
current maximized state beside it. Published when it changes, and **before** the extent when both
changed in one gesture, so a consumer keeping the normal window's room hears the state before the
size. A publisher may remember these numbers and may hand them back; it may never interpret them
— a desktop coordinate is a fact about one machine's monitors, and authored canvas geometry stays
medium-independent. Absence is **silence**, not zeroes: `(0,0)` is a real place on every desktop,
so a medium with no window — every terminal skin, whose window belongs to the emulator — simply
never says one. `SurfacePlacementRemembered{x, y, maximized}` is the road back: sent to the skin's
role once, by a publisher restoring a session, and it is a *want*, not an instruction. The Skin
judges it against the displays that exist now
([how](zengine.skin.md#surfaceplacementremembered)), and what the publisher then hears is the truth
through the ordinary report, never an echo.

## The clipboard

**The skin owns the platform clipboard, in both directions — and the read follows paste
intent.** `ClipboardCopy{text}` is intent: *a weaver copied this* — published by whichever
application or pane provider hosted the copy, handed to the platform by the active skin as far as
its medium honestly can, and mirrored by every other text-holding participant, which is what keeps
copy-here-paste-there true inside the process even on a medium whose platform cannot answer. A
mirror means *the freshest copy said in this process* — never the platform's state, because
nothing here watches the system clipboard: it is ambient host state that may have nothing to do
with this application, and permission to use its text when a weaver asks to paste is not
permission to observe it continuously. When a paste is requested, the asker sends
`ClipboardTextRequested{}` to the skin's role and the skin answers `ClipboardText{readable, text}`
from its medium at that moment; when `readable` is false the asker pastes its in-process mirror
instead. The two fields are separate because an empty platform clipboard and an unreadable one are
different sentences, and collapsing them would paste stale mirror text the platform no longer
holds. What each Skin can read and set is [the Skin's](zengine.skin.md#clipboardtextrequested).

## A capture

`SurfaceCaptureRequested{after_frame}` asks the Skin for a picture of what its medium presented,
answered `SurfaceCaptured`; `SurfaceCaptureChunkRequested{capture, offset}` fetches the picture's
bytes, `kCaptureChunkBytes` a chunk, answered `SurfaceCaptureChunk`. The medium reads back what it
drew, so a capture cannot disagree with the screen and is not a second renderer. A picture proves
presentation at one frame and nothing about work still pending. What each Skin's picture is,
how an ask waits for a frame, and every refusal, are
[the Skin's](zengine.skin.md#surfacecapturerequested).

## Current wire versions

A shape's version is part of its identity at the admission gate. These compose upward: a
region's identity is computed from its row type and a canvas's from its layer type, so adding
one field to a row moves three numbers.

| shape | version |
|---|---|
| `SurfaceRect` | 3 |
| `SurfaceLabel` | 3 |
| `SurfaceTextRow` | 2 |
| `SurfaceTextRegion` | 7 |
| `SurfaceLayer` | 5 |
| `SurfaceCanvas` | 9 |
| `SurfaceText` | 1 |
| `SurfaceExtent` | 4 |
| `SurfacePlacement` | 1 |
| `SurfacePlacementRemembered` | 1 |

