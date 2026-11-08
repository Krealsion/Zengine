# The Graphics package

A participant that shows something draws it into the window that shows its area. It draws **nodes**
-- the one primitive -- or says an **element** once and then sends only that element's changes.
The window keeps what was drawn, paints it in the colours of the palette its area wears, and shows
only what changed: the cells that changed on a terminal, the picture once a beat in a GUI window. A
**directory** introduces each area to its window and tells each owner its room; nothing a picture
is made of passes through it.

A program spells the vocabulary with `#include "graphics/vocabulary.hpp"` and links
`zengine::graphics`. This page is for whoever draws: the vocabulary, colours by name, the canvas, a
drawing weave, an element of your own, and the seam an element's code is written against; and what
a window answers for its platform, on the bus and to C++ in its own process. The
package ships three weaves, each with its manual: the TUI window,
[`zengine.window.tui`](zengine.window.tui.md), built as `zengine-window-tui`; the GUI
window, [`zengine.window.sdl`](zengine.window.sdl.md), built as `zengine-window-sdl` where the build
has SDL; and the directory, [`zengine.graphics`](zengine.graphics.md), built as `zengine-graphics`.
Three themes come with them as palettes, `night`, `day` and `block`, generated from the JSON they
are written in (`graphics/themes/`); `classic`, in a setting or an `app.json`, is another name for
`night`. The snake (`examples/snake/`) is the worked example:
it draws its frame as nodes and its board as an element of its own.

![The snake in a TUI window wearing the night palette, its pilot flying and the game paused after step 120: the operator's status line, the world's score, board and buttons, and the score weave's own area beside it](images/snake-night.png)

![The same window after 1 chose the block palette: the window painted what it keeps in the other palette, and nothing was drawn again on the bus](images/snake-block.png)

## Who says what

```text
owner -> its window          Drawn (nodes and records), ElementPut once, then ElementChange
window -> the owner          AreaRoom (this window, its incarnation, the room, what it holds,
                             and how it sets text)
window -> the owner          DrawnRefused (why, in words), ActionRequested
window -> pointer's owners   AreaButtonChanged, AreaPointerMoved, AreaWheelTurned
                             (at the area's own point, a press's moments under its number, a
                             button's with what the window drew under the hand)
window -> an area's arranger AreaHandRefused (an injected hand refused in an area the weaver's alone),
                             AreaShown (what the area shows, asked by AreaShowAsked)
window -> its follower       AreaPressed (where a press in an area another role owns landed),
                             AreaSettled (unasked: what the area shows settled, by its version)
input role -> a window     AttributedInput (a participant's moment, landed as that hand's),
                             PointerTimingSaid (the double-click time, asked and when it changes)
window -> the input role   PointerTimingAsked (at each activation)
window -> owner, follower    AreaHoldEnded, AreaPointerLeft (to the follower when another role)
window -> the keys' owners   AreaKeyPressed, AreaKeyReleased, AreaTextEntered (naming the area
                             and the hand)
arranger -> directory        PaletteOffered, PaletteChosen, AreaPlaced, ElementCodeOffered
arranger -> its window       WindowShaped (its title and size, from a role arranging there)
window -> everyone           WindowOffered (as its own role: its pixels, incarnation and text)
directory -> window          AreaIntroduced, AreaWithdrawn, ElementCodeOffered, PaletteOffered
owner -> directory           RoomAsked (an owner that started again)
directory -> everyone        GraphicsReady (a directory that started afresh)
anyone -> a window           ClipboardCopy, PumpSurface, SurfacePlacementRemembered
asker -> a window            ClipboardTextRequested, SurfaceCaptureRequested,
                             SurfaceCaptureChunkRequested
window -> the asker          ClipboardText, SurfaceCaptured, SurfaceCaptureChunk, zen.Refused
GUI window -> everyone        SurfacePlacement, SurfaceCloseRequested (as its own role)
```

The **arranger** is whoever a host lets reach the directory: in the snake's host, its operator;
in Workshop, its own weave, which places each pane's area and its own
([`zengine.workshop`](../../workshop/docs/zengine.workshop.md#graphicsready)), and lays its desk
out in the window's room, in its unit and its text
([`WindowOffered`](../../workshop/docs/zengine.workshop.md#windowoffered)).
What each message does is the manual of the role that takes it: the window's for what an owner
sends it, the directory's for what the arranger and a window send it.

An area placed with `keys` hears every key (`AreaKeyPressed`), with `releases` its release
(`AreaKeyReleased`), with `text` the text it types (`AreaTextEntered`): an owner places its areas
for the moments it takes, and is given no other. Each names the area and whose hand made it, as
every pointer moment does: `local`, the weaver's own, or the participant the input role named. One
placed with `pointer`
holds every point inside it, painted or not, and hears, at its own point in pixels, every pointer
moment there as the moment it was: a button pressed or let go, a part's or not
(`AreaButtonChanged`), a move (`AreaPointerMoved`), the wheel (`AreaWheelTurned`). A press holds its
button for the area until its release, so its moves and its release reach the area wherever the
pointer goes, all three under the press's number; the area's arranger, when another role that
follows the area (`AreaPlaced::followed`), hears where each press and release landed
(`AreaPressed`), as a desktop raises what was clicked. A button's moment says what the window drew
under the hand when it landed: the area that showed it, the part named there, and that area's
drawing as its owner numbered it (`Drawn::number`), so the owner acts on the element the press
landed on, and refuses it only where its newest drawing shows another there ([a pane may be
pressed](../../workshop/docs/workshop-panes.md#a-pane-may-be-pressed)). An area placed without the
pointer takes only the points where what it drew names an action or presents an element, and lets
the pointer through to the areas behind it everywhere else. Each time the window shows, it tells the
arranger following an area, unasked, when the area's version moved (`AreaSettled`); of an area its
arranger does not follow, it tells the arranger none of this. Either window, terminal or OS, routes
them alike ([`zengine.window.tui`](zengine.window.tui.md#use)).

A window also answers for its platform, in shapes spelled here (`graphics/vocabulary.hpp`): a
copy, a paste's read of the clipboard, its place on the desktop and a place offered back, a close
its desktop asked for, the beat's work on request, and the picture it last presented, retained one
at a time and fetched by chunk. What each window does with them is its manual's; a terminal reads
no clipboard and has no place or close box, so it says none.

## How a window counts clicks

A press says its `clicks` (`AreaButtonChanged::clicks`, `AreaPressed::clicks`), and a release 0. The
window counts the weaver's own presses by the double-click time the input role says
(`zengine::input::PointerTimingSaid`, [`zengine.input`](../../input/docs/zengine.input.md#pointertimingasked)),
on its own clock as it reads its platform: a press of one button within that time after the last
press of it counts one more than that press, wherever either landed, else 1 -- so 2 for a double
click's second press. The platform's own word is not read: not SDL's count, which follows the
desktop's time, and not a console's double-click record, so the console's records and a POSIX
terminal's reports count alike. Where a press landed is the owner's to judge: a reader takes a
count of 2 or more as a double click with its own place rule, and never times a press. Each hand
counts on its own: a participant's presses, injected through its input session, are counted by the
same rule as the weaver's, so an agent double clicks by clicking twice, and its click beside the
weaver's is two single clicks. A window asks the time at each activation and is told again when it changes; until it is told, it
counts by `zengine::input::kDefaultDoubleClickMs`.

## A node, and an element's record beside it

A node is a `path`, an operator `op` and the operator's operands `args`. The path is the owner's
name for the node, unique in its area, and its place: the segments before the last name the node
it lives in, and a node is drawn in its parent's space. A node keeps its place in the paint order
until it is removed; a new node goes after its siblings, and a group is a layer.

An operand's shape is implied by its operator, so `args` is that shape's body alone: Loom's
canonical bytes without the envelope that names the shape (`graphics/implied.hpp`). The window puts
the operator's own envelope back before the one gate reads the operands, so operands of another
shape or version are refused there: a drawing whose rectangle, ellipse, line, path or text does not
read as its operands' version now is refused whole, in words naming the shape and its versions.

An `ElementRecord` says what a subtree means: its `type` and `data` (a record shape's value, whole),
and the `action` a press anywhere in it asks of the area's owner.

## The operators

| operator | operands | in a terminal | in light |
|---|---|---|---|
| `group` | an offset, an optional clip | its children's space; paints nothing | the same |
| `rect` | a box, a fill and an outline | the cells it covers; `+ - |` edges | a filled rectangle; edges a pixel wide |
| `ellipse` | a box, a fill and an outline | the cells whose centre falls inside | a span a pixel row; its outline in lines |
| `line` | two points and an ink | a walk of cells in `- | / \` | a line |
| `path` | SVG path data (`M L H V C Q Z`), a fill and an outline | curves flattened; cells by centre | curves flattened; spans a pixel row, even-odd |
| `text` | a point, words, an ink, a size, a font and a weight | a character a cell, a wide one in two and a combining mark in the cell before it; bold where it shows bold; one size; `?` where its encoding carries no character past ASCII | every character, at its columns of its size's advance; bold where asked; a character a clip would cut left out whole, a line a clip crosses cut by its pixels |
| `image` | a box and pixels, four bytes each | a luminance ramp, one glyph a cell | its pixels, scaled into its box |

Coordinates are the area's pixels; a terminal floors each edge to its cell of
<!-- value kCellPx -->12<!-- /value --> pixels. `graphics/pixels.hpp` is the arithmetic on both:
sums and floors that cannot overflow, a pixel in a medium's device unit, and the grain a hit test
floors by. Every colour an operand carries is a `Color` (below). An operator the window does not
know is painted as its name, in `quiet`, never silently dropped.

## Colours by name

A colour a draw carries (`Color`, `graphics/color.hpp`) is one of three: an exact value,
`graphics::rgb(0xRRGGBB)` or `rgba(0xRRGGBBAA)`; a name from the **pool**, the colours every pane
may name by use; or a **slot** its pane exposes for themes to set. The window turns a name into a
colour when it paints, from the **palette** the area wears; nothing before it -- not the drawer,
not the painter -- knows what a name looks like, so a palette changed is every area wearing it
painted again, with nothing drawn again on the bus.

The names are C++ names, generated from JSON, so a name nobody declared does not build. The pool's
are `zengine::graphics::colors` (`graphics/colors.hpp`, from `graphics/pool.json`): `ground`, the
medium's own, what shows where nothing is drawn; `background`, opaque material beneath content;
`text`; `quiet`; `accent`, the one thing pointed at; `alert`, what the weaver must see; `guide`, a
guest's mark for the weaver; and `edge`, a box's quiet outline.

```cpp
#include "graphics/colors.hpp" // generated from graphics/pool.json

sketch.fill_rect("card", x, y, w, h, graphics::colors::background);
sketch.text("card/title", x, y, "Counter", graphics::colors::accent);
// sketch.text("x", 0, 0, "no", graphics::colors::acent);   // does not compile
```

A pane's own slots are a JSON file of its own, generated by the package's `zengine_names()`
(installed with it; CMake 3.22 or newer):

```json
{ "format": "zengine-slots", "format_version": 1, "role": "example.counter",
  "colors": [ { "name": "badge", "color": "#f2a541", "like": "accent",
                "about": "the count's badge" } ] }
```

```cmake
zengine_names(my-pane JSON slots.json NAMESPACE counter::colors HEADER counter/colors.hpp)
```

`counter::colors::badge` then paints the palette's `badge` for the role `example.counter` where a
palette sets one, else its own `#f2a541`. `like` names the pool colour a medium that cannot show a
colour shows in its place: a terminal writes that colour's word and fills a shape with its
character. Without it, a terminal shows the nearest of its sixteen inks.

A **palette** (`PaletteOffered`) is a theme resolved for the window: each pool colour's
`0xRRGGBBAA`, what a terminal shows where it cannot show it (`cells`: `terminal default`, the
terminal's default ink and ground, or one of its sixteen, perhaps `bright`, perhaps `bold`; none for its nearest) and the
character it fills a shape with (`fill`); each role's slots; and a terminal's outline, a corner,
across and down. Whoever arranges the window offers palettes and chooses the one each area wears
(`PaletteChosen`) through the directory; an area wearing none, or one nobody offered, wears the
window's own, the pool's defaults, which are night's. The three shipped themes are
`graphics::themes::night()`, `day()` and `block()` (`graphics/themes.hpp`), each generated with
`zengine_palettes()` from its JSON in `graphics/themes/`. In a terminal the pool shows text and
every ground in the terminal's default colours, a pane's frame in grey (`bright black`), and each
other colour as the nearest of its sixteen
inks; `block` gives every colour an ink and a fill character of its own.

An exact colour can name the pool colour a medium that cannot show it shows in its place
(`rgb(0x2e7d5b, colors::accent)`); where only the pool's colours are shown, any colour shows as
its pool colour (`pool_color_of`).

## Text: sizes, fonts, weight

A text draw carries a size, a font and a weight beside its colour (`TextArgs`, `graphics/ops.hpp`;
`graphics/text_style.hpp`). A size is a pool name or a slot (`TextSize`), a font a pool name, both
generated from `graphics/pool.json` with the colours (pool format 2): `zengine::graphics::sizes`
holds `caption`, `body`, `heading` and `title`, and `zengine::graphics::fonts` holds `text`, which
names the face it sets text in. A text naming no size stands at `body`, and one naming no font is
set in `text`; a weight is `text_weight::kRegular` or `kBold`.

```cpp
sketch.text("card/title", x, y, "Counter", graphics::colors::accent, graphics::sizes::heading,
            graphics::text_weight::kBold);
```

The window turns a size into points and a font into a face when it paints, from the area's
palette (`PaletteOffered`'s `sizes` and `fonts`), and says each pool size's measure in the room
before an owner lays its text out: `AreaRoom::measures`, one `TextMeasured` a size, its points and
its column's advance and line in pixels; `measure(size)` reads one, and `measure()` is `body`'s. A
slot size (`SlotSize`, a slots file's `sizes`) stands at the points its palette sets for the area's
owner, else at its own. A window of characters sets every size in its cell and shows bold through
its ink. Every font is monospaced, so an owner places text by columns: each character takes the
columns the text core gives it (`component::character_columns`), 0, 1 or 2 by Unicode's width
classes, the rule both painters and a terminal draw by.

The GUI window ([`zengine.window.sdl`](zengine.window.sdl.md)) sets text in the embedded JetBrains
Mono, regular and bold, each size measured as it is first asked. A character that face lacks is
drawn from the platform's own fonts -- the Windows font folder, the folders fontconfig reads,
macOS's, at most `window::kMaxPlatformFaces` files, looked in once a character -- and else from GNU
Unifont and Unifont Upper, embedded as the last resort, so every character is drawn somewhere; such
a glyph sits centred in its own columns, scaled down where it would not fit them. With
`ZENGINE_SDL_EMBEDDED_FACES=1` the window draws from its embedded faces alone, so a picture never
moves with the fonts of the machine it is taken on; the suites and the pictures of these pages are
taken so. A face that will not open says why on standard error, and the window sets its text in
SDL's debug face. A terminal ([`zengine.window.tui`](zengine.window.tui.md)) writes every
character its encoding carries, a wide one over two cells, and `?` for each character past ASCII
where its encoding carries none, one column each.

## Drawing: a canvas, and one showing a beat

`graphics/canvas.hpp` is what an owner draws with. A `Sketch` collects one picture; a `Canvas`,
kept for the owner's life, remembers what it last sent its window, so a whole picture drawn again
sends only the nodes that changed. A beat queues its picture (`draw`), an element it presents
(`present`) and its elements' changes (`change`), and `show` sends them, its nodes before its
elements: every message but the last says `more` follows, and the window shows once, after the
last. A tick that changes the snake's board and its score is one showing.

A canvas draws whole when its window is new, started again, resized or sets text another way, or
the window says the area is empty. The window tells the room (`AreaRoom`): its name, its
incarnation, its width and height (`width()`, `height()`), the references of the owner's elements
it still presents there, whether it holds none of the owner's drawing there (`empty`): an area
shown afresh, or given to this owner; and how it sets text (`text()`, a `TextMeasure`: a column's
advance and a line's height in pixels at `body`, the GUI window's face or a terminal's cell, every
other size's in `measures`, and `cell_px()`, its device pixels in a cell). An owner places its text
by that measure, a row a line and a column an advance, and paints its area's ground: what it leaves
unpainted shows what stands behind it. `room` answers whether the window, its incarnation, its size or its measure
changed or the area is empty, and `holds` whether the window presents an element, so an owner says
again only what the window does not hold.

## Drawing the text pieces

`graphics/elements/text_face.hpp` holds the text pieces' looks, a face each medium's, drawn from the
values `component/` keeps measured in columns, and keeps nothing itself. A text box keeps a
column on each side of its text (`kTextBoxInset`): in a row of cells `[text___]`
(`text_box_spelling`), or plain, its text alone from its first column, where a prompt beside it
already marks the place (`TextBoxInCells`, `text_box_plain`, `text_box_text_column`); where the
window sets type its outline in the pool's `edge` round its text,
selection and caret (`text_box_face`). A text area in a row of cells shows its lines between
`N more above...` and `... N more below` (`text_area_cells`, read back by
`text_area_cells_place`). `text_area_face` draws a text area scrolled freely: its lines from the
scroll's place, the first and last cut by their pixels where the scroll stands between two lines,
the selection on a muted ground behind its text, and the bar's thumb in a room kept on the right
whether or not the text overflows (`kTextAreaBarRoom`, drawn `kTextAreaBarDrawn` wide), pressed
across all of that room. The owner reads a press with the geometry the face was drawn with
(`text_area_geometry`, `text_area_place_at`, `text_area_on_bar`, `text_area_bar_scroll`), so a
press lands where the face showed.

## A drawing weave

`graphics/drawing.hpp` owns what every drawer would otherwise repeat, so a drawer writes its
picture and what it reacts to. A weave attaches `graphics::DrawingFacet` through `With` (or uses
the single-facet shorthand `DrawingWeave`):

```cpp
class ScoreWeave
    : public graphics::With<graphics::DrawingFacet>::Weave<ScoreWeave, ScoreState,
                                                           loom::Accept<FoodEaten>> {
public:
    using Weave::on;
    ScoreWeave() : Weave("snake.score") {} // the role it draws as; its area by default
    void on(const FoodEaten&, loom::Mail&) { ++state_.eaten; }
    void picture(graphics::Sketch& f) const {
        f.text("eaten", 0, 0, std::to_string(state_.eaten), graphics::colors::text);
    }
};
```

The drawing facet takes the room from the window it names, asks its room after the first message
each life delivers it while no room has come (a loaded weave's activation, where it hears one),
and takes a refusal and does nothing with it. At the end of every delivery, and of every poke that
wrote the state, `With` runs the weave's own `after_delivery` (when present) and then each facet's
`after_delivery` in declaration order, where `DrawingFacet` draws the drawer's `picture` and shows
the beat: a drawer never calls `show`. An optional `on_room` runs when the window, its incarnation,
or its size changed, where an element the window does not hold is said again (`canvas().holds`).
A drawer that presents elements emits `ElementPut` and `ElementChange` itself and accepts the
presses it wants. A drawer whose parts name actions composes `graphics::ActionFacet` in place of
`DrawingFacet`: the same facet, also taking `ActionRequested`, which is the drawer's
`on_action(action, mail)` only when the window that shows the drawer's area asks it, as that
window's role, about that area; a drawer without actions keeps `DrawingFacet` and accepts no
`ActionRequested`:

```cpp
class NewGame
    : public graphics::With<graphics::ActionFacet>::Weave<NewGame, NewGameState,
                                                          loom::Accept<>> {
public:
    using Weave::on;
    NewGame() : Weave("demo.new-game") {}
    void on_action(const std::string& action, loom::Mail&) {
        if (action == "new-game") ++state_.games;
    }
    void picture(graphics::Sketch& f) const {
        graphics::Button button("new-game", " new game ");
        button.set_bounds(0, 0, 12 * graphics::kCellPx, graphics::kCellPx);
        button.sketch(f, "new-game"); // the action a press on it asks
    }
};
```

A pane Workshop seats draws the same way. `workshop::PaneDrawingFacet`
([`workshop/pane_facets.hpp`](../../workshop/pane_facets.hpp)) holds the pane's room in Workshop
and in a window alike: the weave writes one `picture(graphics::Sketch&)` and one
`on_action(action, mail)`, and calls the facet's `show` where its state changed. The window that
shows the area named by the pane's role is sent what changed; Workshop is sent the same picture
as the pane's rows, each record a part named by its path. A press on a part, in the window or on
Workshop's canvas, reaches `on_action` as the action its record names, so the owner never reads a
place ([The pane facets](../../workshop/docs/workshop-panes.md#the-pane-facets)).

Over the Timer, a drawer passes `zengine::timer::TimedWeave` as the `Base`
parameter to `Weave`; the single-facet `DrawingWeave` shorthand keeps the compile-time wall on a
derived `after_delivery`. A weave that claims names its `loom::Claims<...>` after `loom::WeaveBase`
(`Weave<Self, State, Accept, Emit, loom::WeaveBase, loom::Claims<...>>`); a facet may declare
`Claims` of its own, and the weave declares the union, each shape once; a shape the weave writes
no `on_claim_published` for is applied as it stands when the bus publishes it. Over the Timer a
weave claims nothing.

## An element of your own

An element is presented by code the window opens, as it opens an operator provider. Its owner says
it once
(`ElementPut`: the area, the path, the type, the owner's reference for it, and its data, whole)
and then sends each change by that reference and the change's index among the changes the code
takes (`ElementChange`); the type implies each change's shape, so a change is its body alone. The
code draws what it is told -- the rules stay the owner's. A press it does not take reaches the
owner as the press itself (`AreaButtonChanged`) where the area was placed with `pointer` true, or
else as the action its record names (`ActionRequested`); otherwise it sends nothing.

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

## The element seam

An element's code is a library a window opens by path, as operator providers are opened
(`operator/image.hpp`), and calls in its own handler: no message, no participant. The library
exports one C symbol answering a table whose version the window reads first and refuses when it is
another; bytes cross as Loom canonical values through sinks the window owns, and every answer is
read through the one gate. Ours and a stranger's are opened and judged alike.

| seam | symbol | in | out |
|---|---|---|---|
| element code (`element_abi.h`) | `zengine_element` | `ElementEvent` | `ElementOutcome` |

Which library holds an element's code is the load plan's word, through the arranger and the
directory, never a pane's. What a window says of a library it cannot open or read is
[the window's manual](zengine.window.tui.md#files).

The window paints with its own painter, one of each medium -- the character painter
(`graphics/term_paint.hpp`) and the painter for light (`graphics/light_paint.hpp`) -- handing it each
node with every colour already resolved (`PaintTask`, `PaintNode::inks`): the painter knows no
colour and keeps nothing between calls. Another way of painting is another window.

## A window in its own process

C++ in a window's own process -- a host's, beside the window it loaded -- may ask the window
(`Window<Medium>`, `graphics/window/window.hpp`) directly for what its messages carry, and each
medium answers for its platform:

| ask | the TUI window | the GUI window |
|---|---|---|
| `clipboard_copy(text)` | the text written to the terminal as OSC 52 (`window::terminal_clipboard_sequence`), which no terminal confirms | the text put on the platform's clipboard |
| `clipboard_text()` | none: no terminal route reads a clipboard | the clipboard's text, read only when asked and never on the window's beat; empty when it holds no text, none with no window |
| `placement()` | none: the terminal's window is its emulator's | its normal top-left on the desktop and whether it is maximized; none with no window, or one never seen unmaximized |
| `place(want)` | nothing | moved to `want`, whole on a display; a maximize waits for the window's next picture and lands on the beat after it |
| `capture()` | the cells it last showed, `text/cells`: a row of `width` glyphs a line, each ending in a newline, `cell_px` 0 | the picture it last presented, drawn again and read back: `image/bmp`, 24-bit and bottom-up (`window::bmp_of_bgr24`), in pixels, `cell_px` `kCellPx` |

`capture()` answers none before the window has shown anything, and never shows what a drawing
saying `more` still holds back. A close its desktop asked for is published, not kept: the beat that
reads it publishes `SurfaceCloseRequested` as the window's role, once. The GUI window opens
centred on its display's usable area, and an area placed past its edge grows it, as far as that
display holds it, once for each new reach: a window a weaver made smaller is not grown back while
its areas stay where they are. A role that arranges an area in a window names it and sizes it
(`WindowShaped`): the GUI window keeps the size within its display and, as after a weaver's
resize, is not grown back while its areas reach no further; the terminal keeps its own.
