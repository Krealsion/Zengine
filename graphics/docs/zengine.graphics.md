# zengine.graphics -- the graphics directory

**Manual.** The directory: the weave that knows which windows exist, where each area stands and
who draws in it, which palette it wears, the palettes offered, and which code presents each element
type. It answers to
the role `zengine.graphics` (a role is the name a weave answers to) and is built as
`zengine-graphics`, a file a host loads beside its windows. It introduces each area to the window
that shows it, and the window then tells the area's owner its room. Nothing a picture is made of
passes through it: an owner draws into its window directly, as
[the TUI window](zengine.window.tui.md) says.

## About

An *area* is a rectangle of a window that one participant, its *owner*, draws in. A *palette* is
a set of colours by name -- the shared pool's and each role's slots -- which a window turns the
names a drawing carries into when it paints (see [The Graphics package](graphics.md), "Colours by
name"). The directory keeps four lists: the windows that have offered themselves, the areas placed
and the palette chosen for each, the palettes offered, each at its latest revision, and the element
code offered for each element type. An element's code is a library named by path; which library is
the load plan's word, through whoever arranges the window, never a drawer's.

The directory introduces an area to its window once the window is known, wearing the palette
chosen for it: a palette chosen before the area is placed is kept for it, so an arranger that
chooses first has each area introduced once. It says again whatever changes: another palette
chosen, an area placed elsewhere, a window that offers itself again, an owner that asks its room.
Every palette offered is passed to every window, which paints again each area wearing it. An area
placed in another window is withdrawn from the one it was in. A window that offers itself is told
every element code and every palette first, then each of its areas, in that order, so an element it
is asked to present finds its code there and an area its palette.

The directory keeps its lists in its state, so a reload keeps them and says nothing again. A
directory started afresh knows nothing: it says [`GraphicsReady`](#follow), every window offers
itself again, and the arranger offers, places and chooses again. An owner that hears the same room
again draws nothing.

## Use

Whoever arranges the window -- the *arranger*, in the snake's host its console weave, in Workshop
Workshop's own -- offers the palettes and the element code, chooses each area's palette and places
it. Once the window has offered itself, each area placed is introduced, and its owner hears its
room from the window. An area wearing a palette no one offered, or none, is painted in the
window's own until one is offered. An owner that starts again asks its room with
[`RoomAsked`](#roomasked).

Who may send: whoever the host's grant lets reach `zengine.graphics`. The snake's host grants its
console [`AreaPlaced`](#areaplaced), [`PaletteOffered`](#paletteoffered),
[`PaletteChosen`](#palettechosen),
[`ElementCodeOffered`](#elementcodeoffered) and [`RoomAsked`](#roomasked); Workshop's grant gives
it all but `ElementCodeOffered`. The directory itself takes [`WindowOffered`](#windowoffered) only
from the window's own role, and `RoomAsked` only from the area's owner.

## Commands

### `AreaPlaced`

`AreaPlaced` v7 says where an area stands, in which window, and who draws in it.

- **Takes:** `area`, the area's name; `window`, the window's name (`terminal`); `owner`, the
  role that draws in it; `x`, `y`, `w`, `h`, its place and size in the window's pixels; `order`,
  its place among the window's areas, a higher order in front; `keys`, `releases` and `text`, true
  for the window to give the owner each key, its release and the text it types, as the owner takes
  them; `pointer`, true for the window to give the owner every pointer moment in the area, the area
  holding every point inside it (else only a press on a part naming an action reaches the owner, as
  that action); `weaver_only`, true for the window to land no injected hand there, only the
  weaver's own; `wheel` and `right`, false for an area hearing the pointer to let the wheel and the
  third button through to the area behind it; `followed`, true for the arranger to follow the
  area. The role the placing is authored as is kept as the area's arranger; following an area,
  it is told where presses land there while another role owns it, that a hold there ended or the
  pointer left, and each time what it shows settled. An arranger that takes none of these places
  its areas without `followed`. A placing spoken personally names no arranger.
- **Answers:** nothing to the sender. Once the window has offered itself, `AreaIntroduced` goes to
  the window's role, wearing the palette chosen for the area, and the window tells the owner its
  room. An area already placed in another window is first withdrawn from it (`AreaWithdrawn`). An
  area given another owner starts empty in its window.
- **Refuses:** nothing; an area whose window is not known yet waits, and is introduced when it
  is.
- **Example:** the snake's console places the world beneath its own status line,
  `send_to_role("zengine.graphics", AreaPlaced{"snake.world", "terminal", "snake.world", 0, c, 50 * c, 28 * c, 0, true})`,
  `c` being a terminal cell's pixels.

### `PaletteOffered`

`PaletteOffered` v1 offers a palette, whole, at a revision: its pool colours, each role's slots
and a terminal's outline characters.

- **Takes:** `palette`, its name; `revision`, which a later offer of the same palette raises;
  `colors`, each a `PaletteColor` -- a pool colour's `name`, its `rgba` (0xRRGGBBAA), `cells`, what
  a terminal shows where it cannot show the colour ("" its nearest), and `fill`, the character a
  terminal fills a shape with in it; `slots`, each role's colours for the slots it exposes; and
  `outline`, a terminal's corner, across and down.
- **Answers:** nothing to the sender. The palette is kept and passed on to every window that has
  offered itself, as `PaletteOffered` from `zengine.graphics`, and each window paints every area
  wearing it again from what it keeps: no owner is told, and nothing is drawn again on the bus. A
  window offering itself later is told it first.
- **Refuses:** nothing; a revision no higher than the one kept changes nothing, and a palette past
  the most the directory holds (`kMaxPalettes`) is not kept.
- **Example:** `PaletteOffered` from `graphics::themes::block()`, the block theme as the build
  generated it, as the snake's console offers each palette it can choose.

### `PaletteChosen`

`PaletteChosen` v1 chooses the palette an area wears.

- **Takes:** `area`, an area placed or about to be; `palette`, a palette's name ("" for the
  window's own).
- **Answers:** nothing to the sender. A placed area is introduced again wearing it, and its window
  paints what it keeps in it: nothing is drawn again on the bus. An area not placed yet keeps the
  choice, and is introduced wearing it once placed.
- **Refuses:** nothing; the palette an area already wears changes nothing, and one not offered yet
  is painted in the window's own until it is.
- **Example:** the snake's `1` key, through its console: `PaletteChosen{"snake.world", "block"}`.

### `ElementCodeOffered`

`ElementCodeOffered` v1 says which library holds the code that presents an element type.

- **Takes:** `type`, the element's data shape's name; `library`, the path of the library.
- **Answers:** nothing to the sender. The code is kept and passed on to every window that has
  offered itself, as `ElementCodeOffered` from `zengine.graphics`; a window offering itself later
  is told it first.
- **Refuses:** nothing; the same type from the same library again changes nothing.
- **Example:** `ElementCodeOffered{"SnakeBoardData", path}`, `path` being where the build put
  `snake-board`, the snake's board.

### `RoomAsked`

`RoomAsked` v1 asks for an owner's room again: an owner that started again sends it.

- **Takes:** `area`, the owner's area. It is taken only from the area's owner, as its role.
- **Answers:** the area is introduced to its window again, and the window answers the owner with
  its room (`AreaRoom`): its window, the window's incarnation, its size, the elements the window
  still presents there, and how the window sets text.
- **Refuses:** nothing; an area never placed, a sender that is not its owner, or an area whose
  window is not known yet, changes nothing.
- **Example:** a drawing weave (`graphics/drawing.hpp`) asks once in each life while no room has
  come: `send_to_role("zengine.graphics", RoomAsked{"snake.score"})`.

## Hears

### `WindowOffered`

`WindowOffered` v3, from a window as its own role, `zengine.window.<name>`, on each activation,
after `GraphicsReady`, and on a resize: its name, its pixels (none with nothing to measure), its
incarnation, its device pixels in a cell (`kCellPx` for the GUI window, none for the terminal's) and
its text's advance and line. The directory keeps it, then tells the window every element code and
every palette and introduces each of its areas. One from any other
role is ignored.

## Follow

The directory publishes `GraphicsReady` v1 as `zengine.graphics` each time it starts: everyone
offers, places and chooses again. Everything else it says goes to one window's role alone:
`AreaIntroduced` v9 (an area, its owner, its arranger, place, order, the key moments and pointer it
hears, whether it is the weaver's hand's alone, the wheel and third button it hears, whether its
arranger follows it, and the palette it wears),
`AreaWithdrawn` v1 (an area no longer that window's), `ElementCodeOffered` v1 and `PaletteOffered`
v1. It never
answers its senders; an owner learns its area is shown when its window tells it its room.

## See also

- [`zengine.window.tui`](zengine.window.tui.md), the window it introduces areas to.
- [The Graphics package](graphics.md), for drawing into a window: the canvas, a drawing weave, an
  element of your own, and who says what.
- [The snake](../../examples/snake/README.md), whose host loads the directory and arranges the
  window.
