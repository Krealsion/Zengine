# The Surface package

**Reference.** The drawing vocabulary, the rules for choosing between its shapes, and what a
skin does with each. If you are giving a weave a face, this is what you publish.

Source: [`surface/vocabulary.hpp`](../../surface/vocabulary.hpp) ·
[`surface/region.hpp`](../../surface/region.hpp) ·
[`surface/skin_tui.hpp`](../../surface/skin_tui.hpp) ·
[`surface/terminal_size.hpp`](../../surface/terminal_size.hpp).

Visual intent in, output out. No game, world, or panel weave talks to the terminal, a window,
or a renderer: they **publish** intent, and a **Skin** — a replaceable loadable weave holding
the singleton `zengine.skin` role — claims the actual surface and paints. Claiming is RAII
(the constructor takes the medium, the destructor gives it back; a swap is release-then-claim
because the Manager delivers the unload first), and ownership is enforced ground: loading a
second skin into the held role is a clean refusal.

The vocabulary is deliberately tiny: `SurfaceText{slot, text}` (a line of **plain** text for a
named slot — "status", "score"; styling is the skin's business) and `SurfaceReady` (the active
skin's hello, published once per incarnation on its first message; text publishers re-publish
their current line on hearing it, so a fresh painter starts complete — the tally line survives
the painter being replaced mid-game).

`SurfaceCanvas{width, height, layers}` is the **general** canvas: an extent in cells and an
ordered list of `SurfaceLayer{rects, labels, texts}` — each one a complete plane of filled
`SurfaceRect`s in painter's (list) order, `SurfaceLabel` text runs over them, and bounded text
regions over those. Each
element carries a semantic **role** — `kFill`/`kAccent`/`kMuted`/`kAlert` — never a colour, so
the terminal media pick an SGR *and a glyph* per role (colour alone would be a lie on a
monochrome terminal) while the SDL medium picks RGB, from one unchanged publisher. Cells, not
pixels: a cell is the coarsest unit a terminal can address, so a canvas lands somewhere real in
every medium. It is a *drawing* vocabulary and pointedly not a layout one — no parent/child, no
anchors, no percentages; whoever publishes has already decided where things go.

**Which of the two kinds of text a publisher chooses is one question, and it is not about
importance**: *is the rectangle mine?* A `SurfaceTextRegion` is the
semantic shape — it is the only one a graphical medium sets in real type — and it answers that
question with its `ground`:

| the rectangle is… | say | what the medium does |
|---|---|---|
| **mine** | `kGroundOwn` (the default) | clears the whole of it before a row is drawn: spaces in a character medium, its own fill in a graphical one |
| **somebody else's, and I am writing ON it** | `kGroundBeneath` | draws the rows and nothing else — no padding, no fill, so material published beneath shows wherever a glyph does not |
| **not a rectangle at all — this CELL is the meaning** | a `SurfaceLabel` | one cell per byte, in every medium |

So ordinary tool prose, headings, lists and controls are ordinary regions; a maker's name written
across an authored object is a `kGroundBeneath` region; and `SurfaceLabel` stays exactly right
where the cell itself is the unit — one affordance glyph over the ring that fills it, chrome
sharing a row with another publisher's sentence. None of the three is deprecated and none is a
fallback for the others. One arithmetic decides the rest for you: a canvas cell is
`kCanvasCellPx` and this repository's face has an 18-pixel line, so `fit_region` answers *zero*
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
It replaced three root lists that made painter's order *global across kinds*, under which a text
region belonging to a presentation somebody had sent to the back still covered a label belonging
to the presentation in front. A skin treats a
canvas exactly as a board (same hello, same first-frame flag, same `frames` counter — it is the
same act), an unknown role paints as `kFill` rather than vanishing, and elements outside the
extent are the skin's to clip. **Both media draw labels**: a terminal already owns a font, and
the SDL medium carries its own 6×6 bitmap face (`surface/skin_sdl_glyphs.hpp`), so a canvas
whose meaning lives in its labels is readable in a window as well as in a terminal. That face is
deliberately debug-grade and covers printable ASCII 0x20–0x7E and nothing else; any other byte —
a control character, or any byte of a multi-byte UTF-8 sequence — renders as a visible unknown
box and is never dropped, because a character that silently disappeared would be the
labels-vanish defect again at character granularity. `SurfaceText` — the named *slot*
lines, which are a different shape — still lands in the window's *title*, because the canvas
occupies the whole window.

`SurfaceTextRegion{x, y, w, h, rows, caret_row, caret_col, ground}` is the **one place a canvas admits a medium may be
finer than a cell**. It is placed in cells like everything else — so where it sits is
the same kind of fact as where a rect sits, and every medium can honour it — and what happens
*inside* is the medium's: a terminal draws one `SurfaceTextRow{text, role}` per cell row, cut
at `w` and dropped past `h`; a window that has a real face open draws the rows at its own
advance and line height, inside the pixel rectangle those cells resolve to, clipped to its own
viewport. Neither is pretending: the terminal is never asked to invent a pixel, and the window
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
which is what every row said before this existed and is not a fifth role. It is the smallest
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
`origin_x + caret_col * advance_px`, and the cell projection *inserts* `kCaretGlyph` at the
same column, which for a caret at the end of a line is byte-for-byte the row the Workshop
Terminal used to append for itself. `kNoCaret` is **negative** on purpose, the same argument
`role::kNone` makes: a prose row index is non-negative by construction, so the absence of a
caret cannot collide with a row anybody might mean. It is emphatically *not* a selection (no
range, no anchor), not a focus fact (a canvas has no focus, and two regions may each carry
one), and not blinking — there is no clock on this shape.

## Current wire versions

A shape's version is part of its identity at the admission gate. These compose upward: a
region's identity is computed from its row type and a canvas's from its layer type, so adding
one field to a row moves three numbers.

| shape | version |
|---|---|
| `SurfaceRect` | 1 |
| `SurfaceLabel` | 1 |
| `SurfaceTextRow` | 2 |
| `SurfaceTextRegion` | 4 |
| `SurfaceLayer` | 2 |
| `SurfaceCanvas` | 6 |
| `SurfaceText` | 1 |
| `SurfaceExtent` | 1 |

**A region too small for a medium's own type is a CELL region in that medium**, and
that is the same sentence a zero metric already means rather than a new rule. A face's line is
not a cell — this repository's is 18 device pixels against a 12-pixel cell — so a region **one
cell tall** holds `(12 - 2*inset) / 18` = zero rows of it. Without the fallback such a region
resolves to a graphical fit with no capacity and both media then draw **nothing**:
`plan_text_regions` skips a fit with no rows, and `plan_canvas` has already decided the
regions were the other list's. A bounded region that silently vanishes is the one answer
`region.hpp` exists to make impossible, so `fit_region` falls back — and because the publisher
asks the same function for its capacity, the publisher, the window and the terminal all get
one answer. The split between the two draw lists is the predicate
`fit_region(r, metric).graphical()` rather than a test on the metric alone, so they remain
exactly disjoint and exactly complete.

`SurfaceExtent{width, height, text_advance_px, text_line_px}` is the one fact that travels the
*other* way — a medium answering how much room it has, in canvas cells, and how big one
character of its own type is, in its own device pixels. Every other shape here is intent flowing
publisher → skin; this is the only one flowing skin → publisher, and it exists because
"how many cells is there room for" is a fact **only the medium holds**. The active skin
publishes it when the answer CHANGES and at no other time (its own 10ms beat is what notices
a person dragging a window edge), and a medium with no answer — a window skin before its window
exists, a terminal skin with no terminal — publishes **nothing** rather than publishing zeroes:
"I have no opinion" and "there is no room" are different sentences. It is an *offer*: a
publisher that ignores it keeps publishing whatever extent it likes and the skin clips, which
is the contract `SurfaceCanvas` already states.

**A terminal skin answers too.** It owns a stream rather than a drawable, so it asks
the operating system about the far end of it — `ioctl(TIOCGWINSZ)` on POSIX,
`GetConsoleScreenBufferInfo`'s **visible window** on Windows (never `dwSize`, which is the
9,001-row scrollback buffer) — through `surface/terminal_size.hpp`, the one place in this
repository that names an operating system for this. The question is asked of the **Sink**, because
the Sink is the thing that holds the terminal: `TuiTerminal` has a real console and answers, a
`std::string` in a suite has none and says so, and a pipe is a far end that is not a terminal at
all. What the medium then reports is not the terminal's size but what a **canvas** fits in it:
`kTuiReservedRows` (3) come off the top — two for the status and score slots, and one because
`canvas_body` ends its last row with a feed and a feed on a terminal's bottom row *scrolls*. The
text metric stays `0 / 0`, which is not a missing measurement: in a terminal a character IS a
cell. So a redirected, piped, captured or CI run measures nothing, says nothing, and paints
Workshop's own documented 78×22 minimum exactly as it always did — and an interactive one paints
the terminal a maker actually gave it.

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

The Workshop package is the live consumer that pulled the canvas in. `SnakeVisual`
remains the V1 payload the skins also accept directly — that named coupling is **not** dissolved:
re-expressing snake's proven frames through the canvas is its own evidence-carrying move, and a
general shape existing is not permission to migrate a proven one through it.

Three skins ship: **`zengine-skin-tui-classic`** and **`zengine-skin-tui-block`** (the old
snake drawers' looks, now living where drawing lives — the terminal medium is one header,
golden-byte tested), and **`zengine-skin-sdl`** — a real window, same intent, zero
medium-specific fields added anywhere (the agnosticism proof). The SDL skin is the only
target that sees SDL: it fetches a **pinned shared SDL3** where none is installed
(checksum in the build; `-DZENGINE_SDL_SKIN=OFF` declines), plans every frame as pure math
(`skin_sdl_plan.hpp`, pinned on every lane), and degrades gracefully with no display — the
suite drives it under SDL's dummy driver, and the window title carries the text slots.

**It also owns a real typeface, for text regions and nothing else.** A bitmap letterform is
not good enough for prose: at 5×5, `a`, `e`, `o` and `c` differ by one pixel, so `weave` reads
`woave` — and scaling that face does not fix it, which was measured rather than assumed. So
the skin carries JetBrains Mono Regular (SIL OFL 1.1,
[surface/fonts/PROVENANCE.md](../../surface/fonts/PROVENANCE.md)) **embedded in the weave**: the
build turns the file's bytes into a translation unit (`cmake/EmbedBinary.cmake`) and the medium
opens them from memory through SDL_ttf. Nothing is installed, nothing is staged, nothing is
discovered at runtime, and no host font is assumed — a skin either has its face or does not.
When it does not, it says why on stderr and publishes no metric, which is the same sentence as
"text is a cell", which is what the bitmap face draws: the pane degrades to the Workshop of
before rather than to a blank rectangle, and the publisher's wrapping follows it there because
it is wrapping against the metric it was told. SDL_ttf and its vendored FreeType ride the same
pinned-and-checksummed fetch as SDL3 itself; see
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md).

The SDL window **is an ear as well as a surface**: it was created not-focusable while
the terminal was the game's only ear, and the flag came off when the SDL Reader made the
window able to hear. It keeps itself answering its OS: a skin's own activation asks the Timer
package for the `zengine.skin.pump` role beat (10ms), and the beat calls `SDL_PumpEvents` —
which gathers OS input INTO the process-global queue and removes nothing, so the queue still
has exactly one owner and it is still the Input reader. That servicing happens even when the
world publishes nothing (a dead world starves a frame-driven pump; the OS calls the result
"not responding"). Role-addressed is the load-bearing half: the beat belongs to the SLOT, so a
swapped-in skin inherits it without asking. Terminal media no-op the beat, exactly as they
no-op'd the old host-sent pump; `PumpSurface` stays as the same hands on direct request, for
suites and timer-less hosts.

**The window is the person's to resize**, under one rule with no per-shape special
case: *a window never shows less than the picture asks for, and is otherwise the person's*. It
is created at the size its first picture asks for, that size becomes its **minimum**, it
carries `SDL_WINDOW_RESIZABLE`, and after that it is grown only by a picture that genuinely
does not fit — which is how a snake board that grew mid-run still comes up whole, while a
canvas publisher that heard `SurfaceExtent` never moves it at all. The alternative is two
parties resizing each other: a canvas rounds down to whole cells, so a medium that sized the
window to the canvas would nibble the window a few pixels smaller every time somebody dragged
it. The extent the skin reports is measured from the renderer's own output size, never
remembered, because a person dragging an edge changes that number and no message says so.
