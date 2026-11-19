# Agent law — Graphics: the window, its colours, its seam and the directory

Routed detail behind [`graphics/AGENTS.md`](../graphics/AGENTS.md), for tasks touching
`graphics/` or a participant that draws into a window. Public reference:
[`graphics/docs/graphics.md`](../graphics/docs/graphics.md).

## The window is a compositor above its medium

- **One window, many media.** The store, the judge, the element seam, the directory's words, keys
  and presses are one class over its medium (`graphics/window/window.hpp`); what a medium keeps
  painted, how it shows it and what it reads are the medium's alone (`window_tui.hpp`,
  `window_sdl.hpp`). A medium never holds a node, a record or an element the store does not.
- **A window answers for its platform, on the bus and to C++ in its own process**: a copy, a
  paste's read of the clipboard (`ClipboardText`), its place on the desktop and a place offered
  back, a close its desktop asked for, the beat on request (`PumpSurface`), and the picture it last
  presented, read back -- never one a drawing saying `more` holds back. The clipboard is read when a
  paste asks, never on the beat. A
  window's title and size are said by a role that arranges an area in it (`WindowShaped`),
  judged by the window from the arrangers its areas were introduced with, and from no other; the
  GUI window keeps the size within its display (`window::shaped_within`) and, as after a weaver's
  resize, grows again only for an area reaching further than its areas did; the terminal keeps its
  emulator's. One
  picture is retained and fetched by chunk, one capture waits for its showing at a time, and a
  picture over `kMaxCaptureBytes` is refused whole.
- **Nothing on the path a picture takes but the drawer and its window.** The directory introduces;
  it never holds a node, a record or a painted cell, and a palette chosen or offered or an area
  moved sends the window a word, never a drawing.
- **One showing a beat, on the drawer's word.** A drawing or a change saying `more` is held back;
  the next message from that drawer without it shows everything since, once. Any other handler of
  the window shows what it changed.
- **Every bound is judged before anything changes**, and a drawing past one is refused whole, to
  its drawer, in words (`DrawnRefused`): the nodes, records and presented elements of an area, a
  path's bytes and depth, an operator's operands.
  A record said again at its own path counts as
  itself. An element whose data does not read as its type is refused saying so, and nothing of a
  never-presented element stays.
- Owners draw whole only when the window, its incarnation,
  its size or its measure changed, or the window says the area is empty (`AreaRoom`,
  `Canvas::room`).
- **The window says how it sets text, and every owner places its text by it.**
  Nothing in a window that shows every pixel is set a character a cell. Each owner paints its
  area's ground, so nothing behind a pane shows through what it leaves blank; no window paints an
  area's ground for its owner.
- **The window tells the room.** It answers every introduction of an area with the owner's room,
  naming the elements it still presents there, so a drawer's state keeps no window or incarnation
  and a drawer that starts again says only what the window does not hold. An area given another
  owner, or withdrawn and introduced again, starts empty, and its room says so (`empty`). A drawing
  weave takes a room only from the role of the window it names.

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
- **The arranger learns where presses land, never the moment**: `AreaPressed` to the role that
  placed an area another role owns, under the press's number. The directory names that role from
  the placing's authorship (`AreaIntroduced::arranger`); a placing spoken personally names none.
- **What became of the pointer goes to the area's owner, and to its arranger when another role**:
  `AreaHoldEnded` and `AreaPointerLeft` reach no other role, so none learns where the weaver's
  pointer goes.

## Every moment says whose hand it is

- **The window lands a participant's pointer as it lands the weaver's.** It takes
  `input::AttributedInput` from `zengine.input`'s role alone, and lands a press, a move or a wheel
  that role injected for a participant by its own judge, as one its platform gave. A moment the
  platform gave (`local`) is the window's own to read. An injected key or text is not taken there:
  a participant's keys reach their consumers from the input role, attributed.
- **Every moment it gives names the hand**: `AreaButtonChanged`, `AreaPointerMoved`,
  `AreaWheelTurned` and `AreaPressed` say `local`, or the `actor` the input role named;
  `AreaKeyPressed`, `AreaKeyReleased` and `AreaTextEntered` say `local`, the platform's own. An
  owner takes a moment's hand only from the role of the window that shows its area, so a key no
  window gave names no hand. Each hand holds its own presses, so a guest's press and release never
  end or take the weaver's.
- **An area placed for the weaver's hand alone refuses every injected hand**
  (`AreaPlaced::weaver_only`): a participant's press or wheel landing there reaches nobody, its
  arranger is told (`AreaHandRefused`), and the moves and the release after it are dropped. The
  weaver's own hand reaches it as any area.

## A window says what an area shows, to its arranger

- **The role that placed an area asks, and only it**: `AreaShowAsked` is answered `AreaShown`,
  and anyone else is refused in words. The answer is what the window holds painted, after its own
  composition: each run of text on one line, and each record naming an action as the part it
  names, with the box its drawing shows, in the area's pixels; a cell or point another role's
  area in front paints over is not said, and one the asker's own area paints over is.
- Past `kMaxAreaWords` runs of text a reading is refused whole, in words, rather than said in part.
- **What a window says of an area unasked goes to an arranger that follows it**
  (`AreaPlaced::followed`): where a press there landed while another role owns it, a hold
  ended, the pointer left, and what it shows settled (`AreaSettled`, at each showing its version
  moved). An arranger that takes none of these places its areas without, and is told none, so no
  showing refuses a word on its bus.
- **A window gives a key moment only to an owner placed to hear it**: a key (`AreaPlaced::keys`),
  its release (`releases`), the text it types (`text`). An owner places its areas for the moments
  it takes, and is given no other, so no key refuses a word on its bus.

## Colour, pixels and operands

- **A colour is an exact value, a pool name or an own value, and the window alone turns a name into a
  colour** (`Color`, a union whose case is the kind of colour, as `TextSize` and `TextStyle` are
  of a size and a style; `graphics/color.hpp`; `graphics/window/palette.hpp`), when it paints, from the
  palette the area wears: a pool name as the palette says, else the pool's default, else `text`; a
  own value as the palette says for the area's owner, else the default the drawing carries, an
  exact colour or a pool colour as the palette says.
  A text's style
  is turned into its font, size, colour and weight there too, each field the draw changes standing
  over the style's (`resolve_text`). The painter is handed exact colours (`PaintNode::inks`) and a
  resolved weight and measure, and keeps none; no drawer, painter or medium resolves a name.
- **Names are generated or declared, never spelled by hand**: the pool's are C++ names generated
  from JSON (`cmake/ZengineNames.cmake`), so a name nobody declared does not compile; a pane's own
  values are declared where they are used (`own()`), or generated from its own JSON, and drawn by
  the name declared. Nothing judges a name's spelling but what C++ cannot spell.
- **A palette is offered whole, at a revision**, through the directory, and kept by every window: a
  higher revision paints every area wearing it again from what the window keeps, and no owner is
  told.
- **A drawer names a colour, never a role**: a pool name, an own value of its own, or an exact value
  beside the pool colour it is `like`; where only the pool's colours are shown, a colour shows as
  its pool colour (`pool_color_of`, `graphics/ops.hpp`).
- **What a drawing's nodes share is said once**: the looks they wear (`Drawn::looks`, `Look`: a
  paint, an ink or a text's look), each node naming one by its place there or none, its operator's
  default; the window keeps an area's looks once and lets go of one no node wears, and its judge
  refuses a look the drawing does not say or the operator does not wear.
- **An element's changes are a list whose order is the wire**: a change travels as its index, so
  the list only grows at its end, and a change outside it does not compile where its owner sends
  it.

## The element seam, and the window's own painter

- An element's code is a library the window opens by path and calls in its own handler: one
  exported C symbol, the seam's version read first and another refused, bytes through sinks the
  window owns, every answer read through the gate. No exception crosses.
- **The window paints with its own painter**, one of each medium behind one interface (`PaintTask`
  in, `PaintResult` out, every colour resolved): no library replaces it, and another way of
  painting is another window weave.
- **A terminal is never made to show every element a window shows.**
- **Which library is the load plan's word**, through the arranger and the directory; a drawer
  names a type, never a library.
- **Element code draws what it is told.** It holds no game's or pane's rules; behaviour stays with
  the owner, who says and changes. At the element seam's version the window keeps each element's
  state and hands it in with every event, naming the element by a handle of its own: a later
  version may keep the state in the code by that handle.
- An element code's crash or stall is its window's: it runs in its handler.

## The terminal's claim and the input edge

- The terminal's claim (`graphics/window/terminal_io.hpp`) and its input edge
  (`input/terminal_reader.hpp`) are shared: whichever participant owns the terminal in a process
  takes both, for exactly its life. Two owners of one terminal in one process is not a supported
  arrangement. The TUI window claims the terminal's input for its life
  (`input/window_claim.hpp`), and the Input reader beside it reads none of it. The GUI window reads
  SDL's one queue itself and claims the process's input for its life: the Input reader beside it
  leaves the console the run started from as it found it, read by nobody, its echo and its Ctrl+C
  the person's.
- The colours, the roles, the platform's shapes (the clipboard, the place, the close, the pump and the capture)
  and the pixel arithmetic (`graphics/pixels.hpp`) are Graphics' own, and every drawer spells them
  by their graphics names.
