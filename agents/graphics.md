# Agent law — Graphics: the window, its seams and the directory

Routed detail behind [`graphics/AGENTS.md`](../graphics/AGENTS.md), for tasks touching
`graphics/` or a participant that draws into a window. Public reference:
[`graphics/docs/graphics.md`](../graphics/docs/graphics.md). Surface's law, which a Skin weave
keeps, is [surface](surface.md); where the two meet is the last section here.

## The window is a compositor above its medium

- A window weave does the whole job of showing: it keeps every area's nodes, records and presented
  elements in its STATE (a reload keeps them and draws nothing again), paints a node only when it
  changes, keeps what it painted in its MEMORY (a reload paints it again from the state, once),
  composes the areas front to back by their order, and shows only what changed: a terminal window
  writes the cells whose picture changed, as runs; an OS window draws its whole picture once a
  showing. It judges a press against what it painted and routes it, and gives keys, and the text
  they type, to the owners of the areas placed to hear them.
- **One window, many media.** The store, the judge, the element seam, the directory's words, keys
  and presses are one class over its medium (`graphics/window/window.hpp`); what a medium keeps
  painted, how it shows it and what it reads are the medium's alone (`window_terminal.hpp`,
  `window_sdl.hpp`). A medium never holds a node, a record or an element the store does not.
- **A window answers for its platform, on the bus and to C++ in its own process**: a copy, a
  paste's read of the clipboard (`ClipboardText`), its place on the desktop and a place offered
  back, a close its desktop asked for, the beat on request (`PumpSurface`), and the picture it last
  presented, read back -- never one a drawing saying `more` holds back. The clipboard is read when a
  paste asks, never on the beat. The OS window publishes its place when it changed, before its
  offer when both did, and each close once, as its office; a terminal publishes neither. One
  picture is retained and fetched by chunk, one capture waits for its showing at a time, and a
  picture over `kMaxCaptureBytes` is refused whole.
- **Nothing on the path a picture takes but the drawer and its window.** The directory introduces;
  it never holds a node, a record or a painted cell, and a skin chosen or an area moved sends the
  window a word, never a drawing.
- **One showing a beat, on the drawer's word.** A drawing or a change saying `more` is held back;
  the next message from that drawer without it shows everything since, once. Any other handler of
  the window shows what it changed. `Canvas::show` sends a beat's nodes before its elements and
  cannot forget the last word, and `DrawingFacet` (`graphics/drawing.hpp`) shows at the end of
  every delivery after the weave's own `after_delivery` when composed with `With` (`DrawingWeave`
  keeps the single-base compile-time refusal).
- **Every bound is judged before anything changes**, and a drawing past one is refused whole, to
  its drawer, in words (`DrawnRefused`): the nodes, records and presented elements of an area, a
  path's bytes and depth, an operator's operands. A record said again at its own path counts as
  itself. An element whose data does not read as its type is refused saying so, and nothing of a
  never-presented element stays.
- A window's incarnation is a random token drawn on its first activation and carried by its state:
  a loaded weave cannot read its own id. Owners draw whole only when the window, its incarnation,
  or its size changed (`AreaRoom`, `Canvas::room`).
- **The window tells the room.** It answers every introduction of an area with the owner's room,
  naming the elements it still presents there, so a drawer's state keeps no window or incarnation
  and a drawer that starts again says only what the window does not hold. An area given another
  owner starts empty. A drawing weave takes a room only from the office of the window it names.

## Each owner hears the pointer in its own area

- **An area placed to hear the pointer holds every point inside it**, painted or not, and its owner
  is given every pointer moment there as the moment it was, naming the area: `AreaButtonChanged`,
  `AreaPointerMoved`, `AreaWheelTurned`, at the area's own point in pixels. A record there takes no
  press: the press goes as itself, element code still first. An area deaf to the pointer takes a
  point only where what it drew names an action or presents an element, and lets the pointer through
  elsewhere.
- **A press holds its button for its area until that button's release**: its moves and its release
  go there wherever the pointer is, and carry the press's number as their correlation. A hold ends
  with no release when its area is withdrawn or handed over, or an area hearing the pointer comes to
  stand in front of it where the press was made; the window tells `AreaHoldEnded` and makes up no
  release. A reload ends every hold untold.
- **The arranger learns where presses land, never the moment**: `AreaPressed` to the office that
  placed an area another office owns, under the press's number. The directory names that office from
  the placing's authorship (`AreaIntroduced::arranger`); a placing spoken personally names none.
- A move with no button held that leaves an area hearing the pointer is told, `AreaPointerLeft`; so
  is the pointer resting in an area handed to another owner, to the owner it had.
- **What became of the pointer goes to the area's owner, and to its arranger when another office**:
  `AreaHoldEnded` and `AreaPointerLeft` reach no other office, so none learns where the weaver's
  pointer goes.

## Colour, pixels and operands

- **A colour is a role or an exact value beside one** (`Paint`): a medium without colour shows the
  role. The roles are the graphics vocabulary's (`graphics/role.hpp`), which Surface spells as its
  own; no medium invents one.
- **A skin paints for one medium, and says which in its bytes**: a node painted for light opens
  with its tag (`light::kTag`). A terminal window refuses a skin that paints light, in words; an OS
  window paints an area whose skin paints characters itself, in the look the skin's name chooses.
- **Pixels are bytes**: an `image` carries its own red, green, blue and alpha, four bytes a pixel.
- **An operand's shape is implied by its operator** and an element change's by the element's type:
  each travels as its shape's body, and the receiver restores the envelope before the gate reads
  it (`graphics/implied.hpp`), so a body of another shape, version or content is refused there.
- **An element's changes are a list whose order is the wire**: a change travels as its index, so
  the list only grows at its end, and a change outside it does not compile where its owner sends
  it.
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
- The terminal's claim (`graphics/window/terminal_io.hpp`) and its input edge
  (`input/terminal_reader.hpp`) are shared: whichever participant owns the terminal in a process
  takes both, for exactly its life. Two owners of one terminal in one process is not a supported
  arrangement. A window claims the input edge it shares with an Input reader for its life
  (`input/window_claim.hpp`), and a reader beside it reads none of it: the terminal window the
  terminal's input, the OS window SDL's one queue.
- Graphics includes nothing of Surface: its roles, the terminal's claim and the platform's shapes
  (the clipboard, the place, the close, the pump and the capture) are its own, and Surface's headers
  spell them for the Skins (`surface/vocabulary.hpp`, `surface/tui_terminal.hpp`). A window and a
  Skin both hear every copy published, so a process shows through one of them.
