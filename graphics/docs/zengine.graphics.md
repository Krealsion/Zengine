# zengine.graphics -- the graphics directory

**Manual.** The directory: the weave that knows which windows exist, where each area stands and
who draws in it, which skin paints it, and which code presents each element type. It answers to
the office `zengine.graphics` (an office is the name a weave answers to) and is built as
`zengine-graphics`, a file a host loads beside its windows. It introduces each area to the window
that shows it, and the window then tells the area's owner its room. Nothing a picture is made of
passes through it: an owner draws into its window directly, as
[the terminal window](zengine.window.terminal.md) says.

## About

An *area* is a rectangle of a window that one participant, its *owner*, draws in. The directory
keeps four lists: the windows that have offered themselves, the areas placed and the skin chosen
for each, the skins offered, and the element code offered for each element type. A skin and an
element's code are libraries named by path; which library is the load plan's word, through
whoever arranges the window, never a drawer's.

The directory introduces an area to its window only once both the window and the area's skin are
known, so no window is ever told an area it would have to paint again a moment later. It says
again whatever changes: a skin offered from another library, another skin chosen, an area placed
elsewhere, a window that offers itself again, an owner that asks its room. An area placed in
another window is withdrawn from the one it was in. A window that offers itself is told every
element code first, then each of its areas, in that order, so an element it is asked to present
finds its code there.

The directory keeps its lists in its state, so a reload keeps them and says nothing again. A
directory started afresh knows nothing: it says [`GraphicsReady`](#follow), every window offers
itself again, and the arranger offers, places and chooses again. An owner that hears the same room
again draws nothing.

## Use

Whoever arranges the window -- the *arranger*, in the snake's host its operator weave, in Workshop
Workshop's own -- offers the skins and the element code, places each area and chooses its skin.
Once the window has offered itself, each area whose skin is offered is introduced, and its owner
hears its room from the window. An owner that starts again asks its room with
[`RoomAsked`](#roomasked).

Who may send: whoever the host's grant lets reach `zengine.graphics`. The snake's host grants its
operator [`AreaPlaced`](#areaplaced), [`SkinOffered`](#skinoffered), [`SkinChosen`](#skinchosen),
[`ElementCodeOffered`](#elementcodeoffered) and [`RoomAsked`](#roomasked); Workshop's grant gives
it all but `ElementCodeOffered`. The directory itself takes [`WindowOffered`](#windowoffered) only
from the window's own office, and `RoomAsked` only from the area's owner.

## Commands

### `AreaPlaced`

`AreaPlaced` v3 says where an area stands, in which window, and who draws in it.

- **Takes:** `area`, the area's name; `window`, the window's name (`terminal`); `owner`, the
  office that draws in it; `x`, `y`, `w`, `h`, its place and size in the window's pixels; `order`,
  its place among the window's areas, a higher order in front; `keys`, true for the window to give
  the owner its keys and their text; `pointer`, true for the window to give the owner every
  pointer moment in the area, the area holding every point inside it (else only a press on a part
  naming an action reaches the owner, as that action); `weaver_only`, true for the window to land
  no injected hand there, only the weaver's own. The office the placing is authored as is
  kept as the area's arranger, which the window tells where presses land in an area another office
  owns; a placing spoken personally names none.
- **Answers:** nothing to the sender. Once the window has offered itself and the area's chosen
  skin is offered, `AreaIntroduced` goes to the window's office, and the window tells the owner its
  room. An area already placed in another window is first withdrawn from it (`AreaWithdrawn`). An
  area given another owner starts empty in its window.
- **Refuses:** nothing; an area whose window or skin is not known yet waits, and is introduced
  when both are.
- **Example:** the snake's operator places the world beneath its own status line,
  `send_to_role("zengine.graphics", AreaPlaced{"snake.world", "terminal", "snake.world", 0, c, 50 * c, 28 * c, 0, true})`,
  `c` being a terminal cell's pixels.

### `SkinOffered`

`SkinOffered` v1 says which library holds a skin, the painter code a window paints an area with.

- **Takes:** `skin`, the skin's name; `library`, the path of the library that holds it.
- **Answers:** nothing to the sender. A skin offered again from another library introduces again
  every area that skin paints, so its window paints them with the new code.
- **Refuses:** nothing; the same skin from the same library again changes nothing.
- **Example:** `SkinOffered{"block", path}`, `path` being where the build put
  `zengine-paint-block`, as the snake's operator offers each skin it can choose.

### `SkinChosen`

`SkinChosen` v1 chooses the skin that paints an area.

- **Takes:** `area`, an area placed; `skin`, a skin's name.
- **Answers:** nothing to the sender. The area is introduced again with the skin's library, and its
  window paints what it keeps with it: nothing is drawn again on the bus.
- **Refuses:** nothing; an area never placed, or the skin it already wears, changes nothing, and a
  skin not offered yet waits until it is.
- **Example:** the snake's `1` key, through its operator: `SkinChosen{"snake.world", "block"}`.

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

- **Takes:** `area`, the owner's area. It is taken only from the area's owner, as its office.
- **Answers:** the area is introduced to its window again, and the window answers the owner with
  its room (`AreaRoom`): its window, the window's incarnation, its size, the elements the window
  still presents there, and how the window sets text.
- **Refuses:** nothing; an area never placed, a sender that is not its owner, or an area whose
  window or skin is not known yet, changes nothing.
- **Example:** a drawing weave (`graphics/drawing.hpp`) asks once in each life while no room has
  come: `send_to_role("zengine.graphics", RoomAsked{"snake.score"})`.

## Hears

### `WindowOffered`

`WindowOffered` v3, from a window as its own office, `zengine.window.<name>`, on each activation,
after `GraphicsReady`, and on a resize: its name, its pixels (none with nothing to measure), its
incarnation, its device pixels in a cell (`kCellPx` for the OS window, none for the terminal's) and
its text's advance and line (the OS window's face, the terminal's cell). The directory keeps it,
then tells the window every element code and introduces each of its areas. One from any other
office is ignored.

## Follow

The directory publishes `GraphicsReady` v1 as `zengine.graphics` each time it starts: everyone
offers, places and chooses again. Everything else it says goes to one window's office alone:
`AreaIntroduced` v4 (an area, its owner, its arranger, place, order, keys, pointer, whether it is
the weaver's hand's alone, skin and the skin's library),
`AreaWithdrawn` v1 (an area no longer that window's) and `ElementCodeOffered` v1. It never
answers its senders; an owner learns its area is shown when its window tells it its room.

## See also

- [`zengine.window.terminal`](zengine.window.terminal.md), the window it introduces areas to.
- [The Graphics package](graphics.md), for drawing into a window: the canvas, a drawing weave, an
  element of your own, and who says what.
- [The snake](../../examples/snake/README.md), whose host loads the directory and arranges the
  window.
