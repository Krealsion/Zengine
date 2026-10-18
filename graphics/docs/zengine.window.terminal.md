# zengine.window.terminal -- the terminal window

**Manual.** The window over the process's own terminal: the weave that does the whole job of
showing what its areas' owners draw. It answers to the office `zengine.window.terminal` (a
window's office is `zengine.window.` and its name, `terminal`) and is built as
`zengine-window-terminal`, a file a host loads. It keeps what each owner drew, paints it with the
area's skin, composes the areas, writes the terminal only the cells that changed, and sends the
terminal's keys, their text and its presses to the owners. The directory,
[`zengine.graphics`](zengine.graphics.md), introduces its areas to it.

## About

The window keeps, in its state, its incarnation and every area it was introduced: where it stands,
its owner, its skin, and what was drawn there -- every node with its place in the paint order,
every element record, and each element it presents with that element's state. A reload keeps all
of it and nothing is drawn again. What it painted, and the libraries it opened, it keeps in memory:
started again, it paints its areas once from what its state keeps. A window replaced starts empty,
with a new incarnation: each owner draws its picture whole once when it hears its room, and
says each element once.

It takes the terminal when it is activated, its output as a terminal skin takes it
(`surface/tui_terminal.hpp`) and its input as the Input weave reads it (`input/terminal_reader.hpp`),
and gives both back when it goes. With no terminal (a pipe, a suite) it writes and reads nothing,
and still keeps and composes its picture, at the size `window::kUnmeasuredCols` by
`window::kUnmeasuredRows` (`graphics/window/window.hpp`).

**Showing.** A node is painted once, when it changes, with its area's skin. The areas are composed
front to back by their order, and the terminal is written only the cells whose picture changed, as
runs: a cursor move, the role's ink, the glyphs. A drawing or a change saying `more` holds the
showing back; the next message from that drawer without it shows everything since, once. Any other
message the window handles shows what it changed.

**Keys and presses.** The window reads its terminal on its own beat, the Timer's
`zengine.window.pump` every `window::kPumpBeatMs` milliseconds, with Zengine's parser. A key and
its text go to the owner of each area placed to hear keys, front to back. A left press lands on
the top node painted at its cell: an element presented there whose code takes the press handles
it; else the action of the record it lies in goes to the area's owner (`ActionRequested`); else
the press, in the area's pixels, to an owner placed to hear the pointer (`PointerButton`); else
nothing.

## Use

An owner is told its room first (`AreaRoom`), once the directory introduces its area; it then
draws with [`Drawn`](#drawn), says an element once with [`ElementPut`](#elementput) and sends its
changes with [`ElementChange`](#elementchange). A drawer usually does all three through a canvas
or a drawing weave ([the Graphics package](graphics.md#a-drawing-weave)), which draw whole only
when the room names another window or incarnation. To read what the window shows, ask
[`WindowShowAsked`](#windowshowasked).

Who may send: whoever the host's grant lets reach `zengine.window.terminal` (the snake's host
grants its operator `Drawn`). The window takes a drawing or an element only from the area's owner,
as that office, and refuses anyone else's; it takes the directory's words only from
`zengine.graphics`, and ignores them from anyone else.
Every bound is judged before anything changes, and a drawing past one is refused whole, in words,
to its drawer (`DrawnRefused`), leaving what the window shows as it was. The bounds are
`graphics/vocabulary.hpp`'s: `kMaxNodesPerArea`, `kMaxElementsPerArea`, `kMaxPathBytes`,
`kMaxPathDepth`, `kMaxArgsBytes`, and `kMaxAreas` areas a window. A record said again at its own
path counts as itself, so an area at its bound still takes a change.

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

## Hears

### `AreaIntroduced`

`AreaIntroduced` v2, from the directory: an area, its owner, place, order, keys, pointer and skin.
The window keeps it, paints it again where it moved or another skin paints it, empties it when
another owner draws in it, and tells the owner its room (`AreaRoom`). Past `kMaxAreas` it is not
shown, and `WindowShown` reports `the window shows <bound> areas: '<area>' is not shown`.

### `AreaWithdrawn`

`AreaWithdrawn` v1, from the directory: the area is no longer this window's. What it held goes,
and the cells it covered are shown again.

### `ElementCodeOffered`

`ElementCodeOffered` v1, from the directory: the library whose code presents an element type. New
code for a type the window presents draws each such element again, from the state it kept.

### `GraphicsReady`

`GraphicsReady` v1, from a directory started afresh: the window offers itself again
(`WindowOffered`), and keeps its areas until the directory says otherwise.

### `TimerFired`

`TimerFired` v1, from the Timer: the window's beat, `zengine.window.pump`. It reads what the
terminal typed and sends the keys, their text and the presses on.

### `TimerReady`

`TimerReady` v1, from the Timer: the service is there, first or again; the window orders its beat
again.

### `TimerResolution`

`TimerResolution` v1, from the Timer: what became of the window's order for its beat, kept by its
timer binding.

## Follow

The window publishes `WindowOffered` v1 as its own office, on each activation and after
`GraphicsReady`: its name, its size in cells, its incarnation. The directory keeps it, and an
arranger that follows it may lay out in that room. Everything else goes to one office. To an
area's owner, `AreaRoom` v2 for each introduction: the area, this window, its incarnation, the
area's size, and the references of the owner's elements it still presents there; `DrawnRefused`
v1 for a drawing refused; `ActionRequested` v1 for a press on an action, and `PointerButton` v1 for
any other press to an owner placed to hear the pointer; and `KeyPressed` v2 and `TextEntered` v1
to each owner placed to hear keys, an owner that takes no text refusing it on the bus. To the
Timer, its beat's order. An observer reads what it shows with `WindowShowAsked`.

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
