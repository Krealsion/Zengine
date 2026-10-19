# zengine.window.sdl -- the OS window

**Manual.** A window on the desktop, drawn in light through SDL3: the weave that does the whole
job of showing what its areas' owners draw, as the terminal window does on a terminal. It answers
to the office `zengine.window.sdl` (a window's office is `zengine.window.` and its name, `sdl`)
and is built as `zengine-window-sdl`, a file a host loads, where the build has SDL. It keeps what
each owner drew, paints it in pixels with the area's skin, draws the whole picture again when
anything changed, and sends the window's keys, their text and its pointer to the owners. The
directory, [`zengine.graphics`](zengine.graphics.md), introduces its areas to it. C++ in its own
process may also ask it for a close its desktop asked for, which it keeps and sends to no one
(`close_requested`), the clipboard, its place on the desktop and the picture it last presented
([the Graphics package](graphics.md#a-window-in-its-own-process)).

## About

The window keeps, in its state, its incarnation and every area it was introduced, with everything
drawn there: its nodes in their paint order, its records, and the elements it presents with their
state. A reload keeps all of it and draws nothing again; started again, it paints what it keeps
once. A window replaced starts empty, with a new incarnation: each owner draws whole once when it
hears its room, and says each element once.

Activated, it opens a resizable OS window of `window::kDefaultWidth` by `window::kDefaultHeight`
pixels (`graphics/window/window_sdl.hpp`), centred on its display's usable area, and closes it
when it goes; reloaded, it leaves that window to the instance after it. An area placed past its
edge grows it, as far as its display holds it. With `ZENGINE_SDL_NOT_FOCUSABLE=1` it never takes
the keyboard. With no display, it says why once on its standard error, holds no SDL, and still
keeps and composes its picture, at that size.

**Painting.** A node is painted once, when it changes, into pixel primitives -- rectangles, lines,
runs of text, images -- each in its paint's exact colour, else the look's for its role: by the
area's skin when it paints light, else by the window itself, in the day look for the skin named
`day` and the night look for any other.

**Showing.** When anything changed, the window draws its whole picture again -- the back-most
area's ground, then the areas back to front, each clipped to its bounds -- and presents it; `more`
holds it back. Uncovered or resized, it draws again.

**Keys and the pointer.** It empties SDL's queue on its own beat, the Timer's
`zengine.window.pump` every `window::kPumpBeatMs` milliseconds, and sends what it read on in order.
A left press lands on the top node whose primitives hold its pixel (within a pixel of a line),
through an area wherever it painted nothing: an element there whose code takes it; else its
record's action to the owner (`ActionRequested`); else, to an owner placed to hear the pointer, the
press (`PointerButton`).

## Use

An owner is told its room first (`AreaRoom`), once the directory introduces its area; it then
draws with [`Drawn`](#drawn), says an element once with [`ElementPut`](#elementput) and sends its
changes with [`ElementChange`](#elementchange). A drawer usually does all three through a canvas
or a drawing weave ([the Graphics package](graphics.md#a-drawing-weave)), which draw whole only
when the room names another window or incarnation. To read what the window shows, ask
[`WindowShowAsked`](#windowshowasked).

Keys go to each owner placed to hear them: each key, its text and its release. Every pointer
moment but a left press -- any button let go, the middle or right button pressed, a move, the
wheel -- goes to the owner of the area it lands in, when placed to hear the pointer. Each reaches
it at the area's own point in pixels, a move's deltas in pixels too; moves read in one beat go as
one, at the last point, their deltas summed.

Who may send: whoever the host's grant lets reach `zengine.window.sdl`. The window takes a drawing
or an element only from the area's owner, as that office, and refuses anyone else's; it takes the
directory's words only from `zengine.graphics`, and ignores them from anyone else. Every bound is
judged before anything changes, and a drawing past one is refused whole, in words, to its drawer
(`DrawnRefused`), leaving what the window shows as it was. The bounds are
`graphics/vocabulary.hpp`'s: `kMaxNodesPerArea`, `kMaxElementsPerArea`, `kMaxPathBytes`,
`kMaxPathDepth`, `kMaxArgsBytes`, and `kMaxAreas` areas a window. A record said again at its own
path counts as itself, so an area at its bound still takes a change.

SDL has one event queue to a process, and this window empties it. Loaded, it claims the queue
(`ZENGINE_WINDOW_SDL_INPUT_PID`, `input/window_claim.hpp`), so
[`zengine-input`](../../input/docs/zengine.input.md)'s SDL reader may run beside it, reading none
of the queue and serving input sessions alone. With no display, the window gives its claim back.

## Commands

### `Drawn`

`Drawn` v1 is what one drawer changed in one area, applied together.

- **Takes:** `area`; `whole`, true to remove first everything the drawer may draw there; `put`, the
  nodes (a node put again changes in its place, a new one goes after its siblings); `removed`, paths
  whose subtrees go; `said` and `unsaid`, element records; `more`, true when more of the same beat
  follows from this drawer.
- **Answers:** nothing; the window paints what changed and, unless `more`, shows it.
- **Refuses:** in `DrawnRefused` to the sender, the whole drawing: `window 'sdl' shows no area
  '<area>'`; `area '<area>' is drawn only by its owner, <owner>`; `'<path>' is not the sender's to
  draw` (or `to remove`), where an element the window presents lies; `'<path>' is drawn in
  '<parent>', which is not drawn`; `a path is empty, longer than <bytes> bytes or deeper than
  <depth>`; `'<path>' has operands over <bytes> bytes`; `area '<area>' would hold more than <bound>
  nodes` (or `elements`); `record '<path>' has no node at its path`; `record '<path>' is not the
  sender's to say`.
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
  rows as text, the window read as a grid of cells of `kCellPx` pixels, each run of text one cell a
  character from the cell it starts in, front areas over those behind, trailing spaces left off; how
  many areas, nodes and presented elements it keeps; and since it started, its showings (pictures
  presented) and the bytes of the primitives they drew, the calls of its skins and of its element
  code, its refusals, and the last one's words.
- **Refuses:** nothing.
- **Example:** a suite asks it after a drawing, and finds the drawing's words in `rows`.

## Hears

### `AreaIntroduced`

`AreaIntroduced` v2, from the directory: an area, its owner, place, order, keys, pointer and skin.
The window keeps it, paints it again where another skin or library paints it, draws its picture
again where it moved, empties it when another owner draws in it, and tells the owner its room
(`AreaRoom`). Past `kMaxAreas` it is not shown, and `WindowShown` reports `the window shows <bound>
areas: '<area>' is not shown`.

### `AreaWithdrawn`

`AreaWithdrawn` v1, from the directory: the area is no longer this window's. What it held goes,
and the picture is drawn again without it.

### `ElementCodeOffered`

`ElementCodeOffered` v1, from the directory: the library whose code presents an element type. New
code for a type the window presents draws each such element again, from the state it kept.

### `GraphicsReady`

`GraphicsReady` v1, from a directory started afresh: the window offers itself again
(`WindowOffered`), and keeps its areas until the directory says otherwise.

### `TimerFired`

`TimerFired` v1, from the Timer: the window's beat, `zengine.window.pump`. It measures the OS
window, and at another size draws its picture again; at another size in cells it offers itself
again (`WindowOffered`) with the new size and the same incarnation. Then it empties SDL's queue and
sends the keys, their releases, their text and the pointer on.

### `TimerReady`

`TimerReady` v1, from the Timer: the service is there, first or again; the window orders its beat
again.

### `TimerResolution`

`TimerResolution` v1, from the Timer: what became of the window's order for its beat, kept by its
timer binding.

## Follow

The window publishes `WindowOffered` v1 as its own office, on each activation, after
`GraphicsReady`, and on a beat that finds the OS window at another size in cells: its name, `sdl`,
its size in cells of `kCellPx` pixels, and its incarnation, which a resize leaves as it was. The
directory keeps it; an arranger that follows it may lay out in that room. Everything else goes to
one office. To an area's owner: `AreaRoom` v2 for each introduction (the area, this window, its
incarnation, the area's size, the references of the elements it presents there); `DrawnRefused` v1;
`ActionRequested` v1; `PointerButton` v1, `PointerMoved` v1 and `PointerWheel` v1 when placed to
hear the pointer; `KeyPressed` v2, `KeyReleased` v2 and `TextEntered` v1 when placed to hear keys,
a shape it does not take refused on the bus. To the Timer, its beat's order. An observer reads what
it shows with `WindowShowAsked`.

## Files

- **A skin and an element's code** are libraries the window opens by the path the directory gives,
  on first use, and closes when no area paints with a skin. A skin that will not paint is not sent
  to anyone: it is the last refusal `WindowShown` reports, `area '<area>': <why>`, and the window
  paints the area itself. The words: `could not open '<library>' to look for skin` (or `element
  code`); `'<library>' exports no skin` (or `no element code`); `'<library>' offers painter seam
  version <n>, not <m>` (or `element seam`); `'<library>' could not say what it is: <reason>`;
  `'<library>' described itself in a shape that does not read`; `the skin refused to paint:
  <reason>`; `the skin answered something that does not read as a PaintResult`; `the skin in
  '<library>' painted '<path>' in light that does not read`; `its code answered something that does
  not read as an ElementOutcome`. What a library exports is
  [the two seams](graphics.md#the-two-seams).

## See also

- [`zengine.window.terminal`](zengine.window.terminal.md), the same window on a terminal.
- [`zengine.graphics`](zengine.graphics.md), the directory that introduces areas to it.
- [The Graphics package](graphics.md): the node and the operators, drawing with a canvas, a drawing
  weave, an element of your own, and the two seams.
