# zengine.window.terminal -- the terminal window

**Manual.** The window over the process's own terminal: the weave that does the whole job of
showing what its areas' owners draw. It answers to the office `zengine.window.terminal` (a
window's office is `zengine.window.` and its name, `terminal`) and is built as
`zengine-window-terminal`, a file a host loads. It keeps what each owner drew, paints it with the
area's skin, composes the areas, writes the terminal only the cells that changed, and sends the
terminal's keys, their text and its pointer to the owners. The directory,
[`zengine.graphics`](zengine.graphics.md), introduces its areas to it. It answers for its terminal
too: a copy goes to the terminal's clipboard, a paste finds none to read, and the cells it last
showed are read back by picture and chunk; C++ in its own process may ask the same directly
([the Graphics package](graphics.md#a-window-in-its-own-process)).

## About

The window keeps, in its state, its incarnation and every area it was introduced: where it stands,
its owner, its skin, and what was drawn there -- every node in its paint order, every element
record, and each element it presents with that element's state. A reload keeps all of it and
nothing is drawn again. What it painted, and the libraries it opened, it keeps in memory: started
again, it paints its areas once from what its state keeps. A window replaced starts empty, with a
new incarnation: each owner draws whole once when it hears its room, and says each element once.

It takes the terminal when activated, its output with the terminal's claim
(`graphics/window/terminal_io.hpp`) and its input as the Input weave reads it
(`input/terminal_reader.hpp`), and gives both back when it goes. With no terminal (a pipe, a suite)
it reads and writes nothing, offers no size, and draws its picture as far as its areas reach
(`graphics/window/window_terminal.hpp`: at least `kUnmeasuredCols` by `kUnmeasuredRows`).

**Showing.** A node is painted once, when it changes, with its area's skin. The areas are composed
front to back by their order, and the terminal is written only the cells whose picture changed, as
runs: a cursor move, the role's ink, the glyphs. A drawing or a change saying `more` holds the
showing back; the next message from that drawer without it shows everything since, once. Any other
message the window handles shows what it changed.

**Keys and the pointer.** The window reads its terminal on its own beat, the Timer's
`zengine.window.pump` every `window::kPumpBeatMs` milliseconds, and sends what it read on in
order. A pointer moment lands in the front-most area that takes its cell: one
placed to hear the pointer takes every cell in it, any other only where it drew an action or an
element. A left press on an element whose code takes it is the element's; else, in an area deaf to
the pointer, its record's action goes to the owner (`ActionRequested`); else, in one hearing it,
the press itself (`AreaButtonChanged`).

## Use

An owner is told its room first (`AreaRoom`), once the directory introduces its area; it then draws
with [`Drawn`](#drawn), says an element once with [`ElementPut`](#elementput) and sends its changes
with [`ElementChange`](#elementchange). A drawer usually does all three through a canvas or a
drawing weave ([the Graphics package](graphics.md#a-drawing-weave)), which draw whole only on a new
window, incarnation or size, or an empty area. To read what it shows, ask
[`WindowShowAsked`](#windowshowasked).

Keys go to each owner placed to hear them, front to back: each key, its text and its release, which
a terminal reports just after the press. An owner placed to hear the pointer is given each pointer
moment in its area as it was, at its cell's centre in the area's pixels (`AreaButtonChanged`,
`AreaPointerMoved`, `AreaWheelTurned`); moves read in one beat go as one. A press holds its button
for its area until its release, under one number; its arranger hears where it landed
(`AreaPressed`; [the Graphics package](graphics.md#who-says-what)).

Who may send: whoever the host's grant lets reach `zengine.window.terminal` (the snake's host
grants its operator `Drawn`). The window takes a drawing or an element only from the area's owner,
as that office, and refuses anyone else's; it takes the directory's words only from
`zengine.graphics`, and ignores them from anyone else.
Every bound is judged before anything changes, and a drawing past one is refused whole, in words,
to its drawer (`DrawnRefused`), leaving what the window shows as it was. The bounds are
`graphics/vocabulary.hpp`'s: `kMaxNodesPerArea`, `kMaxElementsPerArea`, `kMaxPathBytes`,
`kMaxPathDepth`, `kMaxArgsBytes`, and `kMaxAreas` areas a window. A record said again at its own
path counts as itself, so an area at its bound still takes a change.

A host may load [`zengine-input`](../../input/docs/zengine.input.md) beside it: while the window
is loaded it claims the process's terminal input, so `zengine-input` serves input sessions without
reading the terminal.

## Commands

### `Drawn`

`Drawn` v1 is what one drawer changed in one area, applied together.

- **Takes:** `area`; `whole`, true to remove first everything the drawer may draw there; `put`, the
  nodes (a node put again changes in its place, a new one goes after its siblings); `removed`, paths
  whose subtrees go; `said` and `unsaid`, element records; `more`, true when more of the same beat
  follows from this drawer.
- **Answers:** nothing; the window paints what changed and, unless `more`, shows it.
- **Refuses:** in `DrawnRefused` to the sender, the whole drawing: `window 'terminal' shows no area
  '<area>'`; `area '<area>' is drawn only by its owner, <owner>`; `'<path>' is not the sender's to
  draw` (or `to remove`), where an element the window presents lies; `'<path>' is drawn in
  '<parent>', which is not drawn`; `a path is empty, longer than <bytes> bytes or deeper than
  <depth>`; `'<path>' has operands over <bytes> bytes`; `area '<area>' would hold more than <bound>
  nodes` (or `elements`); `record '<path>' has no node at its path`; `record '<path>' is not the
  sender's to say`.
- **Example:** the snake's world, a drawing weave, sends the nodes its picture changed in a tick
  with `more`, then its board's change without it, so the tick is shown once.

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
  the code's own words (`its data does not read as SnakeBoardData`), or a library that will not
  open ([Files](#files)). Nothing of an element never presented stays.
- **Example:** `ElementPut{"snake.world", "board/grid", "SnakeBoardData", 1, data, false}`, the
  snake's board, said at its room.

### `ElementChange`

`ElementChange` v1 changes an element the sender presents, by its reference.

- **Takes:** `ref`, the drawer's reference; `kind`, the change's index among the changes the
  element's code takes; `change`, that change's body alone (the type implies its shape); `more`.
- **Answers:** the element's code takes the change and draws it again; only what changed is
  painted.
- **Refuses:** in `DrawnRefused`, with no area: `no element this window presents is the sender's by
  reference <ref>`; with the area, `element '<path>': ` and the code's words, as `change <kind> is
  none this code takes, or its body does not read`.
- **Example:** each tick the snake's world sends its board one step,
  `ElementChange{1, 0, step, false}`.

### `WindowShowAsked`

`WindowShowAsked` v1 asks the window what it shows, and what it has spent.

- **Takes:** nothing.
- **Answers:** `WindowShown` v1, to the asker alone: the window's name and incarnation; its rows as
  text, trailing spaces left off; how many areas, nodes and presented elements it keeps; and since
  it started, its showings (writes to the terminal) and their bytes, the calls of its skins and of
  its element code, its refusals, and the last one's words.
- **Refuses:** nothing.
- **Example:** a suite asks it after each tick, and compares `rows` with what the game drew.

### `ClipboardTextRequested`

`ClipboardTextRequested` v1 asks what the platform clipboard holds, for a paste.

- **Takes:** nothing.
- **Answers:** `ClipboardText` v1, to the asker alone: `readable` false and `text` empty, since no
  terminal route reads a clipboard; the asker pastes what its own process last copied.
- **Refuses:** nothing.
- **Example:** a pane asks when a weaver pastes, and on this window pastes its own last copy.

### `SurfacePlacementRemembered`

`SurfacePlacementRemembered` v1 offers back a place on the desktop a session remembered.

- **Takes:** `x`, `y` and `maximized`, as an OS window said them.
- **Answers:** nothing: a terminal's window is its emulator's, so this window takes no place and
  says none (`SurfacePlacement`).
- **Refuses:** nothing.
- **Example:** a host restoring a session offers the place it kept; nothing moves.

### `PumpSurface`

`PumpSurface` v1 asks for the beat's work now.

- **Takes:** nothing.
- **Answers:** nothing; the window does what its beat does ([`TimerFired`](#timerfired)): it
  measures the terminal, offering itself again at another size in cells, and sends on what the
  terminal typed.
- **Refuses:** nothing.
- **Example:** a suite, or a host with no Timer, pumps the window after typing into its terminal.

### `SurfaceCaptureRequested`

`SurfaceCaptureRequested` v1 asks for a picture of the cells the window last showed.

- **Takes:** `after_frame`: -1 for now; a count at or past the window's showings so far holds the
  answer back until the showing that passes it.
- **Answers:** `SurfaceCaptured` v1, to the asker alone, the picture retained in place of the last:
  `ok`; `capture`, its number; `frame`, the showings when it was taken; `width` and `height` in
  cells; `cell_px` 0; `format` `text/cells`, a row of `width` glyphs a line, each ending in a
  newline; `bytes`, its size. A drawing saying `more` is not in it until shown.
- **Refuses:** in `SurfaceCaptured`, `ok` false and `refusal`: `this medium has no picture to read
  back right now`, before its first showing; `a capture is already waiting for frame <n>; one waits
  at a time`; `this delivery cannot defer its answer`; `the picture is <n> bytes, over the <bound>
  this window retains` (`kMaxCaptureBytes`).
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

## Hears

### `AreaIntroduced`

`AreaIntroduced` v3, from the directory: an area, its owner, its arranger, place, order, keys,
pointer and skin. The window keeps it, paints it again where it moved or another skin paints it,
empties it when another owner draws in it, ends a press held for it or now covered by it, and tells
the owner its room (`AreaRoom`). Past `kMaxAreas` it is not shown, and `WindowShown` reports `the
window shows <bound> areas: '<area>' is not shown`.

### `AreaWithdrawn`

`AreaWithdrawn` v1, from the directory: the area is no longer this window's. What it held goes, a
press held for it ends, and the cells it covered are shown again.

### `ElementCodeOffered`

`ElementCodeOffered` v1, from the directory: the library whose code presents an element type. New
code for a type the window presents draws each such element again, from the state it kept.

### `GraphicsReady`

`GraphicsReady` v1, from a directory started afresh: the window offers itself again
(`WindowOffered`), and keeps its areas until the directory says otherwise.

### `TimerFired`

`TimerFired` v1, from the Timer: the window's beat, `zengine.window.pump`. It measures the
terminal, and at another size in cells shows its whole picture at that size and offers itself
again (`WindowOffered`) with the new size and the same incarnation. Then it reads what the terminal
typed and sends the keys, their releases, their text and the pointer on.

### `TimerReady`

`TimerReady` v1, from the Timer: the service is there, first or again; the window orders its beat
again.

### `TimerResolution`

`TimerResolution` v1, from the Timer: what became of the window's order for its beat, kept by its
timer binding.

### `ClipboardCopy`

`ClipboardCopy` v1, a weaver's copy: the text written to the terminal as its copy sequence, OSC 52,
which no terminal confirms. It is no showing, and nothing answers it.

## Follow

As its own office the window publishes `WindowOffered` v2 on each activation, after `GraphicsReady`,
and at each new terminal size: its name, size in cells (none with no terminal), incarnation, kept by
a resize, and no pixels in a cell, its unit the cell. The directory keeps it; an arranger may lay
out in it. It never publishes `SurfacePlacement` or `SurfaceCloseRequested`. To an area's owner:
`AreaRoom` v3 for each introduction (`empty` while it holds none of the owner's drawing);
`DrawnRefused` v1; `ActionRequested` v1; `AreaButtonChanged`, `AreaPointerMoved`, `AreaWheelTurned`,
`AreaHoldEnded` and `AreaPointerLeft` v1 when placed to hear the pointer; `KeyPressed` v2,
`KeyReleased` v2 and `TextEntered` v1 when placed to hear keys, a shape it does not take refused on
the bus. To an arranger not the owner: `AreaPressed`, `AreaHoldEnded` and `AreaPointerLeft` v1. To
an asker: `WindowShown` v1, `ClipboardText` v1, `SurfaceCaptured` v1, `SurfaceCaptureChunk` v1 and
`zen.Refused`. To the Timer, its beat's order.

## Files

- **A skin and an element's code** are libraries the window opens by the path the directory gives,
  on first use, and closes when no area paints with a skin. A skin that will not paint is not sent
  to anyone: it is the last refusal `WindowShown` reports, `area '<area>': <why>`. The words:
  `could not open '<library>' to look for skin` (or `element code`); `'<library>' exports no skin`
  (or `no element code`); `'<library>' offers painter seam version <n>, not <m>` (or `element
  seam`); `'<library>' could not say what it is: <reason>`; `'<library>' described itself in a
  shape that does not read`; `the skin refused to paint: <reason>`; `the skin answered something
  that does not read as a PaintResult`; `its code answered something that does not read as an
  ElementOutcome`. What a library exports is [the two seams](graphics.md#the-two-seams).

## See also

- [`zengine.graphics`](zengine.graphics.md), the directory that introduces areas to it.
- [The Graphics package](graphics.md): the node and the operators, drawing with a canvas, a drawing
  weave, an element of your own, and the two seams.
- [The snake](../../examples/snake/README.md), whose host loads the window and draws into it.
