# zengine.window.tui -- the TUI window

**Manual.** The window over the process's own terminal: the weave that does the whole job of
showing what its areas' owners draw. It answers to the role `zengine.window.tui` (a
window's role is `zengine.window.` and its name, `terminal`) and is built as
`zengine-window-tui`, a file a host loads. It keeps what each owner drew, paints it in the
palette its area wears, composes the areas, writes the terminal only the cells that changed, and
sends the terminal's keys, their text and its pointer to the owners. The directory,
[`zengine.graphics`](zengine.graphics.md), introduces its areas to it. It answers for its terminal
too: a copy goes to the terminal's clipboard, a paste finds none to read, and the cells it last
showed are read back by picture and chunk; C++ in its own process may ask the same directly
([the Graphics package](graphics.md#a-window-in-its-own-process)).

## About

The window keeps, in its state, its incarnation and every area it was introduced: where it stands,
its owner, its palette, and what was drawn there -- every node in its paint order, every element
record, each element it presents with its state -- and each palette offered. A reload keeps
all of it and nothing is drawn again. What it painted, and the libraries it opened, it keeps in
memory: started again, it paints its areas once from what its state keeps. A window replaced starts empty, with a
new incarnation: each owner draws whole once when it hears its room, and says each element once.

It takes the terminal when activated, its output with the terminal's claim
(`graphics/window/terminal_io.hpp`) and its input as the Input weave reads it
(`input/terminal_reader.hpp`), and gives both back when it goes. With no terminal (a pipe, a suite)
it reads and writes nothing, offers no size, and draws its picture as far as its areas reach
(`graphics/window/window_tui.hpp`: at least `kUnmeasuredCols` by `kUnmeasuredRows`).

**Showing.** A node is painted once, when it changes, in its area's palette. The areas are
composed front to back by their order, and the terminal is written only the cells that changed,
as runs: a cursor move, the ink, the glyphs. A drawing or a change saying `more` holds the
showing back; the next message from that drawer without it shows everything since, once. Any other
message the window handles shows what it changed.

**Keys and the pointer.** The window reads its terminal when the host's one wait wakes it
([`Waited`](#loomhostwaited)), and sends what it read on in order. A pointer moment lands in the front-most area that takes its cell: one
placed to hear the pointer takes every cell in it, any other only where it drew an action or an
element. A left press on an element whose code takes it is the element's; else, in an area deaf to
the pointer, its record's action goes to the owner (`ActionRequested`); else, in one hearing it,
the press itself (`AreaButtonChanged`).

## Use

An owner is told its room first (`AreaRoom`), once the directory introduces its area; it then draws
with [`Drawn`](#drawn), says an element once with [`ElementPut`](#elementput) and sends changes
with [`ElementChange`](#elementchange). A drawer does all three through a canvas or a
drawing weave ([the Graphics package](graphics.md#a-drawing-weave)), which draw whole only on a new
window, incarnation, size or measure, or an empty area. To read what it shows, ask
[`WindowShowAsked`](#windowshowasked), or, as an area's arranger, [`AreaShowAsked`](#areashowasked),
told when it settled (`AreaSettled`).

A key, its text and its release go to each owner placed to hear that moment, front to back, naming
the area and the hand (`AreaKeyPressed` and its kin). An owner placed to hear the pointer is given
each pointer moment in its area as it was, at its cell's centre in the area's pixels
(`AreaButtonChanged`, `AreaPointerMoved`, `AreaWheelTurned`); moves read in one look go as one. A
press holds its button for its area until its release, under one number; its arranger hears where it
landed (`AreaPressed`; [the Graphics package](graphics.md#who-says-what)). Each names its hand
([`AttributedInput`](#attributedinput)), a press its clicks
([`PointerTimingSaid`](#pointertimingsaid)).

Who may send: whoever the host's grant lets reach the window. It takes a drawing or an element only
from the area's owner, as that role, and the directory's words only from `zengine.graphics`.
Every bound is judged before anything changes, and a drawing past one is refused whole, in words,
to its drawer (`DrawnRefused`), leaving what the window shows as it was. The bounds are
`graphics/vocabulary.hpp`'s: `kMaxNodesPerArea`, `kMaxElementsPerArea`, `kMaxPathBytes`,
`kMaxPathDepth`, `kMaxArgsBytes`, and `kMaxAreas` areas a window.

A host may load [`zengine-input`](../../input/docs/zengine.input.md) beside it: while the window
is loaded it claims the process's terminal input, so `zengine-input` serves input sessions without
reading the terminal.

## Commands

### `Drawn`

`Drawn` v2 is what one drawer changed in one area, applied together.

- **Takes:** `area`; `whole`, true to remove first everything the drawer may draw there; `put`, the
  nodes (a node put again changes in its place, a new one goes after its siblings); `removed`, paths
  whose subtrees go; `said` and `unsaid`, element records; `more`, true when more of the same beat
  follows from this drawer; `number`, the drawer's own for what its drawing means, which the window
  says with each press that lands there.
- **Answers:** nothing; the window paints what changed and, unless `more`, shows it: text a
  character a cell, a wide one over two, one size, bold through its ink; as itself where the
  terminal's encoding carries UTF-8 (a console's code page, else `LC_ALL`, `LC_CTYPE` or `LANG`),
  else `?` a character ([text](graphics.md#text-styles-sizes-fonts-weight)).
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
  it started, its showings (writes to the terminal) and their bytes, the calls of its painter and
  of its element code, its refusals, and the last one's words.
- **Refuses:** nothing.
- **Example:** a suite asks it after each tick, and compares `rows` with what the game drew.

### `AreaShowAsked`

`AreaShowAsked` v1 asks what one area shows, of the role that placed it.

- **Takes:** `area`; `since`, the `version` of the last answer the asker took, or 0.
- **Answers:** `AreaShown` v1, to the asker alone: the window's name and incarnation; the area's
  `version`, which moves whenever what it shows may have moved -- its drawing, its place, or an area
  that meets it; `waiting`, true while the window told the owner a room it must draw whole and none
  has come since; the text measure, a cell each way; and, unless `since` is still the version (then
  `same`, and nothing more), its `words` and `parts` in the area's pixels. A word is a run of cells
  a text node shows on one row, ending where none does, where an area in front painted (one the
  asker arranges aside, its cover the asker's to judge), or at the window's edge, with the blanks
  after its last character left off. A part is a record naming an action, by that name: the box of
  the cells its drawing shows; `pressed`, with the middle of the cells where a press reaches it
  (`px`, `py`); and `available`, as the record says it (`PartFace`), else true. What the window
  holds painted is what it shows at its next showing at the latest.
- **Refuses:** in `zen.Refused`: `window 'terminal' shows no area '<area>'`; `area '<area>' is read
  only by the role that placed it`; `area '<area>' shows more than <bound> runs of text`
  (`kMaxAreaWords`).
- **Example:** Workshop reads the area of a pane it placed, `AreaShowAsked{"zengine.info", 0}`, and
  then asks again since the version it took.

### `ClipboardTextRequested`

`ClipboardTextRequested` v1 asks what the platform clipboard holds, for a paste.

- **Takes:** nothing.
- **Answers:** `ClipboardText` v1, to the asker alone: `readable` false and `text` empty, since no
  terminal route reads a clipboard; the asker pastes what its own process last copied.
- **Refuses:** nothing.
- **Example:** a pane asks when a weaver pastes, and on this window pastes its own last copy.

### `SurfacePlacementRemembered`

`SurfacePlacementRemembered` v1 offers back a place on the desktop a session remembered.

- **Takes:** `x`, `y` and `maximized`, as a GUI window said them.
- **Answers:** nothing: a terminal's window is its emulator's, so this window takes no place and
  says none (`SurfacePlacement`).
- **Refuses:** nothing.
- **Example:** a host restoring a session offers the place it kept; nothing moves.

### `PumpSurface`

`PumpSurface` v1 asks for a look now, for a host that runs no wait.

- **Takes:** nothing.
- **Answers:** nothing; the window does what a wake does ([`Waited`](#loomhostwaited)): it
  measures the terminal, offering itself again at another size in cells, and sends on what the
  terminal typed.
- **Refuses:** nothing.
- **Example:** a suite, or a host that runs no wait, pumps the window after typing into its
  terminal.

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

### `WindowShaped`

`WindowShaped` v1 names a window and sizes it, said by a role that arranges an area in it.

- **Takes:** `window`, `title`, `width` and `height`, as a GUI window takes them.
- **Answers:** nothing: the terminal's size and title are its emulator's, so this window keeps them.
- **Refuses:** nothing.
- **Example:** an app's weave says its window's title and size; the terminal stays as it is.

## Hears

### `AreaIntroduced`

`AreaIntroduced` v9, from `zengine.graphics`: an area, its owner, its arranger, place, order, the key
moments and pointer it hears, the palette it wears, `weaver_only` and `followed`. The window keeps
it, paints it again where it moved or wears another palette, empties it when another owner draws in it, ends a
press held for it or now covered by it, and tells the owner its room (`AreaRoom`). Past `kMaxAreas`
it is not shown, and `WindowShown` reports `the window shows <bound> areas: '<area>' is not shown`.

### `AreaWithdrawn`

`AreaWithdrawn` v1, from `zengine.graphics`, the directory: the area is no longer this window's. What it held goes, a
press held for it ends, and the cells it covered are shown again.

### `ElementCodeOffered`

`ElementCodeOffered` v1, from `zengine.graphics`, the directory: the library whose code presents an element type. New
code for a type the window presents draws each such element again, from the state it kept.

### `PaletteOffered`

`PaletteOffered` v1, from `zengine.graphics`, the directory: a palette, whole, at a revision. The window keeps the
latest revision of each palette in its state, and paints every area wearing it again from what it
keeps: its owners are told nothing and draw nothing. A revision no higher than the one kept changes
nothing; past `kMaxPalettes` palettes one is not held, and `WindowShown` reports `the window holds
<bound> palettes: '<palette>' is not held`.

### `GraphicsReady`

`GraphicsReady` v1, from `zengine.graphics`, a directory started afresh: the window offers itself again
(`WindowOffered`), and keeps its areas until the directory says otherwise.

### `loom.host.Waited`

`loom.host.Waited` v2, from the host alone, with the time: what the window claims (its
terminal's input, a resize on POSIX, and a time while a lone Escape waits) is ready or due. It
measures the terminal -- at another size in cells it shows its picture again and offers itself
again (`WindowOffered`) -- then sends on what the terminal typed. From any participant it is
nothing.

### `ClipboardCopy`

`ClipboardCopy` v1, from any weave that copies, a weaver's copy: the text written to the terminal as its copy sequence, OSC 52,
which no terminal confirms. It is no showing, and nothing answers it.

### `AttributedInput`

`AttributedInput` v2, from `zengine.input` alone: a moment it injected for a participant, named
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
and at each new terminal size: its name, its cells as `kCellPx` pixels (none with no terminal),
incarnation, its unit the cell and text a cell; the directory keeps it. It never publishes
`SurfacePlacement` or `SurfaceCloseRequested`. To an area's owner: `AreaRoom` v4 for each
introduction; `DrawnRefused` v1; `ActionRequested` v1; `AreaButtonChanged` v4, `AreaPointerMoved`,
`AreaWheelTurned` v2, `AreaHoldEnded`, `AreaPointerLeft` v1 when placed to hear the pointer;
`AreaKeyPressed`, `AreaKeyReleased` and `AreaTextEntered` v1 as placed to hear each. To an arranger
following an area not its own: `AreaPressed` v4, `AreaHoldEnded`, `AreaPointerLeft` v1; following
one, `AreaSettled` v1; to any arranger, `AreaHandRefused`. To an asker: `WindowShown`, `AreaShown`,
`ClipboardText`, `SurfaceCaptured`, `SurfaceCaptureChunk` v1 and `zen.Refused`. To the host's one
wait, its claim, as its role.

## Files

- **An element's code** is a library the window opens by the path the directory gives, on first
  use. Code that will not answer is not sent to anyone: it is the last refusal `WindowShown`
  reports, `area '<area>': <why>`. The words: `could not open '<library>' to look for element
  code`; `'<library>' exports no element code`; `'<library>' offers element seam version <n>, not
  <m>`; `'<library>' could not say what it is: <reason>`; `'<library>' described itself in a shape
  that does not read`; `its code answered something that does not read as an ElementOutcome`. What
  a library exports is [the element seam](graphics.md#the-element-seam). The window paints with its
  own painter, which no library replaces.

## See also

- [`zengine.graphics`](zengine.graphics.md), the directory that introduces areas to it.
- [The Graphics package](graphics.md): the node and the operators, colours by name, drawing with a
  canvas, a drawing weave, an element of your own, and the element seam.
- [The snake](../../examples/snake/README.md), whose host loads the window and draws into it.
