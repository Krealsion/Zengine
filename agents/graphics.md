# Agent law — Graphics: the window, its seams and the directory

Routed detail behind [`graphics/AGENTS.md`](../graphics/AGENTS.md), for tasks touching
`graphics/` or a participant that draws into a window. Public reference:
[`graphics/docs/graphics.md`](../graphics/docs/graphics.md). Surface's law, which a Skin weave
keeps, is [surface](surface.md); where the two meet is the last section here.

## The window is a compositor above its medium

- A window weave does the whole job of showing: it keeps every area's nodes, records and presented
  elements in its STATE (a reload keeps them and draws nothing again), paints a node only when it
  changes, keeps what it painted in its MEMORY (a reload paints it again from the state, once),
  composes the areas front to back by their order, and writes its medium only the cells whose
  picture changed, as runs. It judges a press against what it painted and routes it, and gives
  keys to the owners of the areas placed to hear them.
- **Nothing on the path a picture takes but the drawer and its window.** The directory introduces;
  it never holds a node, a record or a painted cell, and a skin chosen or an area moved sends the
  window a word, never a drawing.
- **One showing a beat, on the drawer's word.** A drawing or a change saying `more` is held back;
  the next message from that drawer without it shows everything since, once. Any other handler of
  the window shows what it changed. A drawer that sends its beat through `Canvas::show` cannot
  forget the last word.
- **Every bound is judged before anything changes**, and a drawing past one is refused whole, to
  its drawer, in words (`DrawnRefused`): the nodes, records and presented elements of an area, a
  path's bytes and depth, an operator's operands. A record said again at its own path counts as
  itself. An element whose data does not read as its type is refused saying so, and nothing of a
  never-presented element stays.
- A window's incarnation is a random token drawn on its first activation and carried by its state:
  a loaded weave cannot read its own id. Owners draw whole only when the window or its incarnation
  changed (`AreaRoom`, `Canvas::set_window`).

## Colour, pixels and operands

- **A colour is a role or an exact value beside one** (`Paint`): a medium without colour shows the
  role. The roles are Surface's; no medium invents one.
- **Pixels are bytes**: an `image` carries its own red, green, blue and alpha, four bytes a pixel.
- **An operand's shape is implied by its operator** and an element change's by the element's type:
  each travels as its shape's body, and the receiver restores the envelope before the gate reads
  it (`graphics/implied.hpp`), so a body of another shape, version or content is refused there.
- Every skin draws every operator, natively or by its medium's fallback; an operator it does not
  know is painted as its name, muted.

## The two seams

- A skin and an element's code are libraries the window opens by path and calls in its own
  handler: one exported C symbol, the seam's version read first and another refused, bytes
  through sinks the window owns, every answer read through the gate. No exception crosses.
- **Which library is the load plan's word**, through the arranger and the directory; a drawer
  names a type, never a library.
- **Element code draws what it is told.** It holds no game's or pane's rules; behaviour stays with
  the owner, who says and changes. At the element seam's version the window keeps each element's
  state and hands it in with every event, naming the element by a handle of its own: a later
  version may keep the state in the code by that handle.
- A skin's or an element code's crash or stall is its window's: both run in its handler.

## Where Graphics and Surface meet

- A Skin weave (`surface/`) paints whole canvases a publisher sends and composes nothing; a
  window (`graphics/`) composes areas and writes runs of changed cells. Neither paints the
  other's vocabulary: no Skin accepts a node, and no window accepts a `SurfaceCanvas`.
- The terminal's claim (`surface/tui_terminal.hpp`) and its input edge
  (`input/terminal_reader.hpp`) are shared: whichever participant owns the terminal in a process
  takes both, for exactly its life. Two owners of one terminal in one process is not a supported
  arrangement.
