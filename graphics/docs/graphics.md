# The Graphics package

A participant that shows something draws it into the window that shows its area. It draws **nodes**
-- the one primitive -- or says an **element** once and then sends only that element's changes.
The window keeps what was drawn, paints it with the skin chosen for the area, and writes only the
cells that changed. A **directory** introduces each area to its window and tells each owner its
room; nothing a picture is made of passes through it.

A program spells the vocabulary with `#include "graphics/vocabulary.hpp"` and links
`zengine::graphics`. This package ships the terminal window (`zengine-window-terminal`), the
directory (`zengine-graphics`), and two skins (`zengine-paint-classic`, `zengine-paint-block`). The
snake (`examples/snake/`) is the worked example: it draws its frame as nodes and its board as an
element of its own.

![The snake in a terminal window under the classic skin: the operator's status line, the world's score and board, and the score weave's own area beside it](images/snake-classic.png)

![The same window after 1 chose the block skin mid-game: the window opened the other painter code and painted what it keeps, and nothing was drawn again on the bus](images/snake-block.png)

## Who says what

```text
owner -> its window          Drawn (nodes and records), ElementPut once, then ElementChange
window -> the owner          AreaRoom (this window, its incarnation, the room, what it holds)
window -> the owner          DrawnRefused (why, in words), ElementPressed, ActionIssued, KeyPressed
arranger -> directory        AreaPlaced, SkinOffered, SkinChosen, ElementCodeOffered
window -> directory          WindowOffered (as its own office, with its incarnation)
directory -> window          AreaIntroduced, AreaWithdrawn, ElementCodeOffered
owner -> directory           RoomAsked (an owner that started again)
directory -> everyone        GraphicsReady (a directory that started afresh)
```

The **arranger** is whoever a host lets reach the directory: in the snake's host, its operator.

## A node, and an element's record beside it

A node is a `path`, an operator `op` and the operator's operands `args`. The path is the owner's
name for the node, unique in its area, and its place: the segments before the last name the node
it lives in, and a node is drawn in its parent's space. A node keeps its place in the paint order
until it is removed; a new node goes after its siblings, and a group is a layer.

An operand's shape is implied by its operator, so `args` is that shape's body alone: Loom's
canonical bytes without the envelope that names the shape (`graphics/implied.hpp`). The window puts
the operator's own envelope back before the one gate reads the operands, so operands of another
shape are refused there.

An `ElementRecord` says what a subtree means: its `type` and `data` (a record shape's value, whole),
and the `action` a press anywhere in it asks of the area's owner.

## The operators

| operator | operands | in a terminal |
|---|---|---|
| `group` | an offset, an optional clip | its children's space; paints nothing |
| `rect` | a box, a fill and an outline | the cells it covers; `+ - |` edges |
| `ellipse` | a box, a fill and an outline | the cells whose centre falls inside |
| `line` | two points and an ink | a walk of cells in `- | / \` |
| `path` | SVG path data (`M L H V C Q Z`), a fill and an outline | curves flattened; cells by centre |
| `text` | a point, words and an ink | one cell a character; outside ASCII, `?` |
| `image` | a box and pixels, four bytes each | a luminance ramp, one glyph a cell |

Coordinates are the area's pixels; a terminal floors each edge to its cell of
<!-- value kCellPx -->12<!-- /value --> pixels. A colour is a `Paint`: a theme role (Surface's roles),
or an exact `0xRRGGBB` beside the role a medium without colour shows it as. An operator a skin does
not know is painted as its name, muted, never silently dropped.

## Drawing: a canvas, and one showing a beat

`graphics/canvas.hpp` is what an owner draws with. A `Sketch` collects one picture; a `Canvas`,
kept for the owner's life, remembers what it last sent its window, so a whole picture drawn again
sends only the nodes that changed. A beat queues its picture (`draw`), an element it presents
(`present`) and its elements' changes (`change`), and `show` sends them, its nodes before its
elements: every message but the last says `more` follows, and the window shows once, after the
last. A tick that changes the snake's board and its score is one showing.

A canvas draws whole when its window is new or started again. The window tells the room
(`AreaRoom`): its name, its incarnation, and the references of the owner's elements it still
presents there. `room` answers whether the window or its incarnation changed, and `holds` whether
the window presents an element, so an owner says again only what the window does not hold.

## A drawing weave

`graphics/drawing.hpp` owns what every drawer would otherwise repeat, so a drawer writes its
picture and what it reacts to. A weave opts in as it opts into the Timer's `TimedWeave`:

```cpp
class ScoreWeave : public graphics::DrawingWeave<ScoreWeave, ScoreState, loom::Accept<FoodEaten>> {
public:
    using DrawingWeave::on;
    ScoreWeave() : DrawingWeave("snake.score") {} // the office it draws as; its area by default
    void on(const FoodEaten&, loom::Mail&) { ++state_.eaten; }
    void picture(graphics::Sketch& f) const {
        f.text("eaten", 0, 0, std::to_string(state_.eaten),
               graphics::by_role(graphics::role::kFill));
    }
};
```

The drawing weave takes the room from the window it names, asks its room after the first message
each life delivers it while no room has come (a loaded weave's activation, where it hears one),
and takes a refusal and does nothing with it. At the end of every delivery, and of every poke that
wrote the state, it draws the drawer's `picture`, so only what changed is sent, and shows the beat:
a drawer never calls `show`. An optional `on_room` runs when the window or its incarnation
changed, where an element the window does not hold is said again (`canvas().holds`). A drawer that
presents elements emits `ElementPut` and `ElementChange` itself and accepts the presses it wants.
Over the Timer, a drawer derives
`DrawingWeave<Self, State, Accept<...>, Emit<...>, zengine::timer::TimedWeave>`. A drawer that
keeps a handshake of its own derives `loom::WeaveBase` and draws with a `Canvas`; one that defines
its own `after_delivery` is refused at build time, since nothing it drew would show.

## An element of your own

An element is presented by code the window opens, as it opens a skin. Its owner says it once
(`ElementPut`: the area, the path, the type, the owner's reference for it, and its data, whole)
and then sends each change by that reference and the change's index among the changes the code
takes (`ElementChange`); the type implies each change's shape, so a change is its body alone. The
code draws what it is told -- the rules stay the owner's -- and a press it does not take reaches
the owner as `ElementPressed`.

`graphics/element_export.hpp` exports one with a struct and a macro:

```cpp
struct BoardCode {
    using Data = SnakeBoardData;                       // said once, whole
    using State = SnakeBoardState;                     // kept by the window between events
    using Changes = std::tuple<SnakeStep, SnakeNewGame>; // the wire's order: grows at its end
    static State start(const Data& d);
    static void change(State& s, const SnakeStep& step);  // one overload a change
    static void change(State& s, const SnakeNewGame& game);
    static bool press(State& s, const std::string& node, const std::string& path);
    static void draw(const State& s, graphics::Sketch& f, const std::string& path);
};
ZENGINE_EXPORT_ELEMENT(BoardCode)
```

The code draws its whole element from its state each time; the window is handed the difference
from the drawing before, so only what changed is painted. The window keeps each element's state
and hands it in with every event, naming the element by a handle of its own.

A change travels as its index in `Changes`, so the list's order is a wire contract: it only grows
at its end. A change of a type outside the list, or listed twice, does not compile where its owner
sends it (`Canvas::change`).

## The window

A window weave does the whole job of showing. It keeps each area's nodes, records and presented
elements in its state, so a reload keeps them and draws nothing again. It paints a node once, when
it changes, with the area's skin, and keeps it painted in its memory. It composes the areas, front
to back by their order, and writes its terminal only the cells whose picture changed, as runs:
a cursor move, the role's ink, the glyphs.

It reads its own terminal on its own beat (every
<!-- value window::kPumpBeatMs -->10<!-- /value --> ms of the Timer) with Zengine's parser. A key goes
to the owner of each area placed to hear keys. A press lands on the top node painted at its cell:
an element presented there whose code takes it handles it; else a record's action goes to the
area's owner as `ActionIssued`; else the press goes to the owner as `ElementPressed`.

Every bound is judged before anything changes, and a drawing past one is refused whole, in words,
to its drawer (`DrawnRefused`), leaving what the window shows as it was: an area holds at most
<!-- value kMaxNodesPerArea grouped -->4,096<!-- /value --> nodes and
<!-- value kMaxElementsPerArea grouped -->1,024<!-- /value --> elements (records, and elements it
presents, each counted); a path at most <!-- value kMaxPathBytes -->256<!-- /value --> bytes and
<!-- value kMaxPathDepth -->16<!-- /value --> deep. A record said again at its own path counts as
itself, so an area at its bound still takes a change. An element whose data does not read as its
type is refused saying so.

`WindowShowAsked` asks a window what it shows, as rows of text, and what it has spent: its
showings, the bytes it wrote, and the calls of its skins and element code.

## The two seams

A skin and an element's code are libraries a window opens by path, as operator providers are
opened (`operator/image.hpp`), and calls in its own handler: no message, no participant. Each
library exports one C symbol answering a table whose version the window reads first and refuses
when it is another; bytes cross as Loom canonical values through sinks the window owns, and every
answer is read through the one gate. Ours and a stranger's are opened and judged alike.

| seam | symbol | in | out |
|---|---|---|---|
| a skin (`painter_abi.h`) | `zengine_painter` | `PaintTask` | `PaintResult` |
| element code (`element_abi.h`) | `zengine_element` | `ElementEvent` | `ElementOutcome` |

A skin for a terminal is the shared painter (`graphics/term_paint.hpp`) and a look -- a glyph for
each role, the glyphs an outline wears -- exported by `ZENGINE_EXPORT_PAINTER`. Which library holds
a skin or an element's code is the load plan's word, through the arranger and the directory, never
a pane's.

## The directory

The directory introduces an area to its window once the window and the area's skin are known --
never an introduction a look would have to change after -- and the window answers each
introduction with the owner's room. The directory says again whatever changes: a skin chosen, an
area placed elsewhere, a window that offers itself again, an owner that asks its room. A window
started afresh is told every element code, then its areas, in that order. An area given another
owner starts empty. A directory started afresh says `GraphicsReady`, and everyone offers, places
and chooses again; an owner that hears the same room again draws nothing.

## What it costs

A replaced window draws nothing until its owners hear their room: each draws its picture whole
once and says each element once. A reloaded window keeps everything and draws nothing. A reloaded
owner asks its room, draws its picture whole once, and says an element again only where the window
does not hold it. A skin chosen sends no drawing on the bus: the window paints what it keeps.
