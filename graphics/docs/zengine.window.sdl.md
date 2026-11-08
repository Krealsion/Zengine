# zengine.window.sdl -- the GUI window

**Manual.** A window on the desktop, drawn in light through SDL3: the weave that does the whole
job of showing what its areas' owners draw, as the TUI window does on a terminal. It answers
to the role `zengine.window.sdl` (a window's role is `zengine.window.` and its name, `sdl`)
and is built as `zengine-window-sdl`, a file a host loads, where the build has SDL. It keeps what
each owner drew, paints it in pixels in the colours of the palette its area wears, draws the whole
picture again when
anything changed, and sends the window's keys, their text and its pointer to the owners. The
directory, [`zengine.graphics`](zengine.graphics.md), introduces its areas to it. It answers for
its desktop too: it copies to and reads the platform's clipboard, says and takes its place on the
desktop, publishes a close its desktop asked for, and reads back the picture it last presented by
picture and chunk; C++ in its own process may ask the clipboard, the place and the picture directly
([the Graphics package](graphics.md#a-window-in-its-own-process)).

## About

The window keeps, in its state, its incarnation and every area it was introduced, with everything
drawn there: its nodes in their paint order, its records, and the elements it presents with their
state -- and every palette offered it. A reload keeps all of it and draws nothing again; started again, it paints what it keeps
once. A window replaced starts empty, with a new incarnation: each owner draws whole once when it
hears its room, and says each element once.

Made, it opens a resizable GUI window of `window::kDefaultWidth` by `window::kDefaultHeight`
pixels (`graphics/window/window_sdl.hpp`), centred on its display's usable area, and closes it
when it goes; reloaded, it leaves it to the instance after it. An area placed past its edge grows
it, as far as its display holds it. With `ZENGINE_SDL_NOT_FOCUSABLE=1` it never takes the
keyboard and opens behind every window on Windows. Where it cannot open (no display, no usable
driver) it says why on standard error and its load fails.

**Text.** Every character, at its columns, its palette's sizes, regular or bold
([text](graphics.md#text-sizes-fonts-weight)).

**Painting.** A node is painted once, when it changes, into pixel primitives -- rectangles, lines,
runs of text in the face, images -- in its area's palette, each name it carries turned into a
colour then ([colours by name](graphics.md#colours-by-name)).

**Showing.** When anything changed, the window draws its whole picture again -- the back-most
area's palette's `ground`, then the areas back to front, each clipped to its bounds -- and presents it; `more`
holds it back. Uncovered or resized, it draws again.

**Keys and the pointer.** It empties SDL's queue on its own beat, the Timer's
`zengine.window.pump` every `window::kPumpBeatMs` milliseconds, and sends what it read on in order.
A pointer moment lands as on a terminal ([`zengine.window.tui`](zengine.window.tui.md#about)),
a node holding the pixels its primitives hold, within a pixel of a line.

## Use

An owner is told its room first (`AreaRoom`), once the directory introduces its area; it then draws
with [`Drawn`](#drawn), says an element once with [`ElementPut`](#elementput) and sends changes
with [`ElementChange`](#elementchange). A drawer does all three through a canvas or a
drawing weave ([the Graphics package](graphics.md#a-drawing-weave)), which draw whole only on a new
window, incarnation, size or measure, or an empty area. To read what it shows, ask
[`WindowShowAsked`](#windowshowasked), or, as an area's arranger, [`AreaShowAsked`](#areashowasked).

A key, its text and its release go to each owner placed to hear that moment. An owner placed to
hear the pointer is given each pointer moment in its area as it was, in the area's pixels
(`AreaButtonChanged`, `AreaPointerMoved`, `AreaWheelTurned`); moves read in one beat go as one. A
press holds its button for its area until its release, under one number; its arranger hears where it
landed (`AreaPressed`; [the Graphics package](graphics.md#who-says-what)). Every moment names its
hand ([`AttributedInput`](#attributedinput)), a press its clicks
([`PointerTimingSaid`](#pointertimingsaid)).

Who may send: whoever the host's grant lets reach the window. It takes a drawing
or an element only from the area's owner, as that role, and the directory's words only from
`zengine.graphics`. Every bound is
judged before anything changes, and a drawing past one is refused whole, in words, to its drawer
(`DrawnRefused`), what the window shows unchanged. The bounds are
`graphics/vocabulary.hpp`'s: `kMaxNodesPerArea`, `kMaxElementsPerArea`, `kMaxPathBytes`,
`kMaxPathDepth`, `kMaxArgsBytes`, and `kMaxAreas` areas a window.

SDL has one event queue to a process, and this window empties it. Loaded, it claims the input
(`ZENGINE_WINDOW_SDL_INPUT_PID`, `input/window_claim.hpp`):
[`zengine-input`](../../input/docs/zengine.input.md) beside it serves input sessions alone, and
leaves the console the run started from to the person at it. A window that cannot open gives it
back.

## Commands

### `Drawn`

`Drawn` v2 is what one drawer changed in one area, applied together.

- **Takes:** `area`; `whole`, true to remove first everything the drawer may draw there; `put`, the
  nodes (a node put again changes in its place, a new one goes after its siblings); `removed`, paths
  whose subtrees go; `said` and `unsaid`, element records; `more`, true when more of the same beat
  follows from this drawer; `number`, the drawer's own for what its drawing means, which the window
  says with each press that lands there.
- **Answers:** nothing; the window paints what changed and, unless `more`, shows it.
- **Refuses:** in `DrawnRefused` to the sender, the whole drawing: `window 'sdl' shows no area
  '<area>'`; `area '<area>' is drawn only by its owner, <owner>`; `'<path>' is not the sender's to
  draw` (or `to remove`), where an element the window presents lies; `'<path>' is drawn in
  '<parent>', which is not drawn`; `a path is empty, longer than <bytes> bytes or deeper than
  <depth>`; `'<path>' has operands over <bytes> bytes`; `area '<area>' would hold more than <bound>
  nodes` (or `elements`); `record '<path>' has no node at its path`; `record '<path>' is not the
  sender's to say`; `'<path>': its <operator> operands do not read as <shape> version <n>, the one
  this window reads (version 1 named its colours by role, and is not read)` (for text, version 2
  too), for a rectangle, an ellipse, a line, a path or text whose operands are not this vocabulary's.
- **Example:** a drawing weave sends the nodes its picture changed in a beat with `more`, then its
  element's change without it, so the beat is shown once.

### `ElementPut`

`ElementPut` v1 presents an element: said once, and again only to start it over.

- **Takes:** `area`; `path`, where the element stands; `type`, its data shape's name; `ref`, the
  drawer's own reference for it in its changes; `data`, the type's value, whole; `more`.
- **Answers:** the code offered for `type` is opened on first use and draws the element; its
  subtree is the code's to draw, and the window keeps the element's state for it. Said again at its
  path, the element starts over from its new data, and nothing of it is left.
- **Refuses:** in `DrawnRefused`: `the sender does not draw in area '<area>' here`; `'<path>' is not
  the sender's to present an element at`; `area '<area>' presents <bound> elements already: '<path>'
  is not presented`; and, each as `element '<path>': <why>`, `no code offered presents '<type>'`,
  the code's own words, or a library that will not open ([Files](#files)). Nothing of an element
  never presented stays.
- **Example:** `ElementPut{"snake.world", "board/grid", "SnakeBoardData", 1, data, false}`, the
  snake's board, said at its room.

### `ElementChange`

`ElementChange` v1 changes an element the sender presents, by its reference.

- **Takes:** `ref`, the drawer's reference; `kind`, the change's index among the changes the
  element's code takes; `change`, that change's body alone (the type implies its shape); `more`.
- **Answers:** the element's code takes the change and draws it again; only what changed is
  painted.
- **Refuses:** in `DrawnRefused`, with no area: `no element this window presents is the sender's by
  reference <ref>`; with the area, `element '<path>': ` and the code's words.
- **Example:** each tick the snake's world sends its board one step,
  `ElementChange{1, 0, step, false}`.

### `WindowShowAsked`

`WindowShowAsked` v1 asks the window what it shows, and what it has spent.

- **Takes:** nothing.
- **Answers:** `WindowShown` v1, to the asker alone: the window's name, `sdl`, and incarnation; its
  rows as text, the window read as a grid of cells of `kCellPx` pixels, each run of text a character
  at its columns from the cell it starts in, front areas over those behind, trailing spaces left off; how
  many areas, nodes and presented elements it keeps; and since it started, its showings (pictures
  presented) and the bytes of the primitives they drew, the calls of its painter and of its element
  code, its refusals, and the last one's words.
- **Refuses:** nothing.
- **Example:** a suite asks it after a drawing, and finds the drawing's words in `rows`.

### `AreaShowAsked`

`AreaShowAsked` v1 asks what one area shows, of the role that placed it.

- **Takes:** `area`; `since`, the `version` of the last answer the asker took, or 0.
- **Answers:** `AreaShown` v1, to the asker alone: the window's name, `sdl`, and incarnation; the
  area's `version`, which moves whenever what it shows may have moved -- its drawing, its place, or
  an area that meets it; `waiting`, true while the window told the owner a room it must draw whole
  and none has come since; its face's measure; and, unless `since` is still the version (then
  `same`, and nothing more), its `words` and `parts` in the area's pixels. A word is the characters
  of text runs on one line, side by side where one ends as the next begins, each inside the area and
  the window and with no area in front drawing over the middle of what the area shows of it -- one
  the asker arranges aside, its cover the asker's to judge -- the blanks after its last character
  left off. A part is a record naming an action, by that name: the box its drawing's primitives
  span, cut to the area; `pressed`, with the point nearest the box's middle where a press reaches it
  (`px`, `py`); and `available`, as the record says it (`PartFace`), else true. A part none of whose
  points shows is not said. What the window holds painted is what it shows at its next picture at
  the latest.
- **Refuses:** in `zen.Refused`: `window 'sdl' shows no area '<area>'`; `area '<area>' is read only
  by the role that placed it`; `area '<area>' shows more than <bound> runs of text`
  (`kMaxAreaWords`).
- **Example:** Workshop reads the area of a pane it placed, `AreaShowAsked{"zengine.info", 0}`, and
  then asks again since the version it took.

### `ClipboardTextRequested`

`ClipboardTextRequested` v1 asks what the platform clipboard holds, for a paste.

- **Takes:** nothing.
- **Answers:** `ClipboardText` v1, to the asker alone: `readable` true and the clipboard's text,
  read now and never on the beat, empty when it holds none; with no window to read for,
  `readable` false, and the asker pastes what its own process last copied.
- **Refuses:** nothing.
- **Example:** a pane asks when a weaver pastes, and pastes the `text` answered.

### `SurfacePlacementRemembered`

`SurfacePlacementRemembered` v1 offers back a place on the desktop a session remembered: a want,
judged against the displays there are now.

- **Takes:** `x`, `y` and `maximized`, as `SurfacePlacement` said them.
- **Answers:** nothing to the sender; the window moves there, whole on a display, and a maximize
  lands on the beat after its next picture. Where it is after is the next `SurfacePlacement`, and a
  size it changed is the next `WindowOffered`.
- **Refuses:** nothing; with no display truth nothing moves.
- **Example:** a host restoring a session offers the place the last `SurfacePlacement` said.

### `PumpSurface`

`PumpSurface` v1 asks for the beat's work now.

- **Takes:** nothing.
- **Answers:** nothing; the window does what its beat does ([`TimerFired`](#timerfired)): it says
  its place when it changed, measures the GUI window, empties SDL's queue and sends it on, and
  publishes a close its desktop asked for.
- **Refuses:** nothing.
- **Example:** a suite, or a host with no Timer, pumps the window after a move or a close.

### `SurfaceCaptureRequested`

`SurfaceCaptureRequested` v1 asks for a picture of what the window last presented.

- **Takes:** `after_frame`: -1 for now; a count at or past the pictures presented so far holds the
  answer back until the picture that passes it.
- **Answers:** `SurfaceCaptured` v1, to the asker alone, the picture retained in place of the last:
  `ok`; `capture`, its number; `frame`, the pictures presented when it was taken; `width` and
  `height` in pixels; `cell_px` `kCellPx`; `format` `image/bmp`, 24-bit and bottom-up; `bytes`, its
  size. A drawing saying `more` is not in it until presented.
- **Refuses:** in `SurfaceCaptured`, `ok` false and `refusal`: `this medium has no picture to read
  back right now`, before its first picture; `a capture is already waiting for frame <n>; one waits
  at a time`; `this delivery cannot defer its answer`; `the picture
  is <n> bytes, over the <bound> this window retains` (`kMaxCaptureBytes`).
- **Example:** `SurfaceCaptureRequested{-1}`, then its bytes by chunk.

### `SurfaceCaptureChunkRequested`

`SurfaceCaptureChunkRequested` v1 fetches one chunk of the retained picture.

- **Takes:** `capture`, the picture's number; `offset`, a byte of it.
- **Answers:** `SurfaceCaptureChunk` v1, to the asker alone: `capture`, `offset`, `total`, and
  `data`, at most `kCaptureChunkBytes` bytes from `offset`; on the last, `offset` and the size of
  `data` make `total`.
- **Refuses:** in `zen.Refused`: `no picture is retained; ask for a capture first`; `capture <n> is
  gone; the retained one is <m>`; `offset <n> is outside a picture of <total> bytes`.
- **Example:** `SurfaceCaptureChunkRequested{1, 0}`, then from each chunk's end until `total`.

### `WindowShaped`

`WindowShaped` v1 names the window and sizes it, said by a role that arranges an area in it.

- **Takes:** `window`, `sdl`; `title`, at most `kMaxWindowTitleBytes` bytes, a longer one cut
  where a character begins and each control character made a space, an empty one keeping the
  window's own; `width` and `height` in pixels, a side of 0 keeping the window's own.
- **Answers:** nothing to the sender; the GUI window takes the title and the size, kept within its
  display's usable area with its frame, and a size it changed is the next `WindowOffered`. What its
  areas reached before does not grow it back; an area placed further out still does. A maximized
  window takes the title and keeps its size.
- **Refuses:** nothing in words: from a role that arranges no area here, or naming another
  window, it changes nothing.
- **Example:** an app's weave, which arranges the app's areas, says `WindowShaped{"sdl", "tally",
  320, 260}`; Workshop, restoring a session, says `WindowShaped{"sdl", "", 1180, 760}`, the room
  its window was last left at.

## Hears

### `AreaIntroduced`

`AreaIntroduced` v9, from the directory: an area, its owner, its arranger, place, order, the key
moments and pointer it hears, the palette it wears, `weaver_only` and `followed`. The window keeps
it, repaints it in another palette, redraws it where it moved, empties it when another owner draws in
it, ends a press held for it or now covered by it, and tells the owner its room (`AreaRoom`). Past
`kMaxAreas` it is not shown, and `WindowShown` reports `the window shows <bound> areas: '<area>'
is not shown`.

### `AreaWithdrawn`

`AreaWithdrawn` v1, from the directory: the area is no longer this window's. What it held goes, a
press held for it ends, and the picture is drawn again without it.

### `ElementCodeOffered`

`ElementCodeOffered` v1, from the directory: the library whose code presents an element type. New
code for a type the window presents draws each such element again, from the state it kept.

### `PaletteOffered`

`PaletteOffered` v1, from the directory: a palette, whole, at a revision. The window keeps the
latest revision of each palette in its state, and paints every area wearing it again from what it
keeps: its owners are told nothing and draw nothing. A revision no higher than the one kept changes
nothing; past `kMaxPalettes` palettes one is not held, and `WindowShown` reports `the window holds
<bound> palettes: '<palette>' is not held`.

### `GraphicsReady`

`GraphicsReady` v1, from a directory started afresh: the window offers itself again
(`WindowOffered`), and keeps its areas until the directory says otherwise.

### `TimerFired`

`TimerFired` v1, from the Timer: the window's beat, `zengine.window.pump`. It says its place when
it changed (`SurfacePlacement`), then measures the GUI window: at another size it draws its picture
again and offers itself again (`WindowOffered`), the same incarnation.
Then it empties SDL's queue, sends the keys, their releases, their text and the pointer on, and
publishes a close its desktop asked for (`SurfaceCloseRequested`).

### `TimerReady`

`TimerReady` v1, from the Timer: the service is there, first or again; the window orders its beat
again.

### `TimerResolution`

`TimerResolution` v1, from the Timer: what became of the window's order for its beat, kept by its
timer binding.

### `loom.host.Waited`

`loom.host.Waited` v1, from the host alone, about what the window claims it waits on after every
delivery (`loom.host.WaitSet`): on Windows' own video driver the thread's messages, else nothing,
and the beat stays. Held, the beat stops, running again only while a remembered maximize waits;
let go, it runs; ready, the window does what its beat does at once. From any participant it is
nothing.

### `ClipboardCopy`

`ClipboardCopy` v1, a weaver's copy: the text put on the platform's clipboard, with no showing and
no answer. With no window there is no clipboard, and nothing is copied.

### `AttributedInput`

`AttributedInput` v1, from `zengine.input` alone: a moment it injected for a participant, named
`actor`. A press, move or wheel is landed as the window's own and told as that hand's, each hand
holding its own presses; a key, text or a `local` moment is not taken. In an area placed for the
weaver's hand alone (`weaver_only`) its press or wheel reaches nobody, its arranger is told
(`AreaHandRefused`), and the moves and release after are dropped.

### `PointerTimingSaid`

`PointerTimingSaid` v1, from `zengine.input` alone: the double-click time, held to 200..2000 ms,
which the weaver's presses count by from then on ([how](graphics.md#how-a-window-counts-clicks)).
The window asks it at each activation (`PointerTimingAsked`); until told, it counts by
<!-- value kDefaultDoubleClickMs -->400<!-- /value --> ms. From anyone else it is nothing.

## Follow

As its own role the window publishes `WindowOffered` v3 on each activation, after `GraphicsReady`,
and at each new size: its name, its pixels, its incarnation, `kCellPx` pixels in a cell and its
face's measure; the directory keeps it. It publishes `SurfacePlacement` v1 when its place changed,
before `WindowOffered` when both did; and `SurfaceCloseRequested` v1 for each close asked. To an
area's owner: `AreaRoom` v4 (with its face); `DrawnRefused` v1; `ActionRequested` v1;
`AreaButtonChanged` v4, `AreaPointerMoved`, `AreaWheelTurned` v2, `AreaHoldEnded`, `AreaPointerLeft`
v1 when placed to hear the pointer; `AreaKeyPressed`, `AreaKeyReleased`, `AreaTextEntered` v1 as
placed to hear each. To an arranger following an area not its own: `AreaPressed` v4,
`AreaHoldEnded`, `AreaPointerLeft` v1; following one, `AreaSettled` v1; to any arranger,
`AreaHandRefused`. To an asker: `WindowShown`, `AreaShown`, `ClipboardText`, `SurfaceCaptured`,
`SurfaceCaptureChunk` v1 and `zen.Refused`. To the Timer, its beat's order.

## Files

- **An element's code** is a library the window opens by the path the directory gives, on first
  use. Code that will not answer is not sent to anyone: it is the last refusal `WindowShown`
  reports, `area '<area>': <why>`. The words: `could not open '<library>' to look for element
  code`; `'<library>' exports no element code`; `'<library>' offers element seam version <n>, not
  <m>`; `'<library>' could not say what it is: <reason>`; `'<library>' described itself in a shape
  that does not read`; `its code answered something that does not read as an ElementOutcome`. What
  a library exports is [the element seam](graphics.md#the-element-seam). The window paints with its
  own painter, which no library replaces; one node its painter could not read back is said as
  `the painter painted '<path>' in light that does not read`.

## See also

- [`zengine.window.tui`](zengine.window.tui.md), the same window on a terminal.
- [`zengine.graphics`](zengine.graphics.md), the directory that introduces areas to it.
- [The Graphics package](graphics.md): the node and the operators, colours by name, drawing with a
  canvas, a drawing weave, an element of your own, and the element seam.
