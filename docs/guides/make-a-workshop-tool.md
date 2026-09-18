# Make a Workshop tool

**How-to.** Adding a tool to Workshop, and choosing which of the two authoring paths you are
on. The exact contracts are [Workshop panes](../reference/workshop-panes.md); a maker's view of
the same ground is [panes](../workshop/panes.md).

Workshop is Zengine's maker-facing application: a desk of the panes its load plan brings --
Info, the project browser, the Builder, Attention, the Terminal, the source Editor and the
desktop (whose Pane Manager and Hotkeys pane a maker opens with `Ctrl`+`p` and `Ctrl`+`k`), every
one of them a loaded weave — beside the one built-in Workshop still compiles, Layouts. A **tool**
is something a maker can open from the Pane Manager.

There are **two ways** to put one there, and current source deliberately does not merge them.
Pick yours before you read any mechanics.

```text
a compiled-in Workshop panel        you are editing Workshop's own source
    Workshop owns the painter, and may own input, a mode, and session state
    the inventory's built-in half is a compile-time array you add a row to

an office-authored external pane    you are a weave that is not Workshop
    a bounded provider protocol, every shape listed in workshop/pane_vocabulary.hpp
    you publish semantic rows; Workshop owns the room and the presentation
```

Both are real, current, tested code. Neither is a plugin system — see
[What does not exist yet](#what-does-not-exist-yet) before you plan around one.

There is also a pane that is *not* a tool and needs neither path: a maker can make a pane of
their own from inside Workshop — a name and one line of text, placed on the fine lattice
inside the pane, kept in a project file — through the [Pane
Creator](../workshop/panes.md#the-pane-creator--a-pane-made-of-data). It paints text and
does nothing else; it is the first pane whose inside is authored data rather than code, and
it is deliberately not a way to author behaviour, input or wiring. If your tool has to *do*
something, you are on one of the two paths above.

## Which path are you on?

| question | compiled-in Workshop panel | office-authored external pane |
|---|---|---|
| who is this for? | a contributor changing Workshop's source | a weave/host integration using the bounded pane protocol |
| how is it discovered? | the compile-time built-in catalog (`kPanelCatalog`) | an office-authored runtime offer, this session only |
| who paints it? | a painter you write: declared in `workshop/screen.hpp`, its body in the subject's `workshop/screen_<subject>.cpp` | Workshop's one generic external-pane painter |
| can it take input? | yes, if you write it: pointer, hotkey, a typing mode | **the input it chooses to accept, all of it about its own room**: a primary press as a row and column, keys and typed text while it holds the keyboard, the wheel, a sweep while the button is held, and actions it declares with a default key a maker can rebind. No focus notification, capture, hover or release |
| durable identity | the catalog row's `provider` + `pane` | the Loom-stamped provider office + the pane key that office offered |
| saved setup | a `PaneRef` and the maker's authored window | the same, unresolved until some office offers it |
| what state is retained? | whatever Workshop session structures you add | Workshop's bounded cache of the rows you last validly sent |
| where does it go? | `kSideRegion` or `kOverlayStack`, declared in the row | always the overlay stack; you do not ask |
| install / plugin story | none needed — you compiled it in | **no discovery or installer**: a one-file recipe builds it against the installed package (`zengine::pane`) and a load-plan row you author loads it — [edit a running pane](../workshop/edit-a-running-pane.md) |

Two sentences worth keeping:

> **The external pane seam exists; product distribution and plugin onboarding do not.**

> The seam is not hypothetical. Its shapes, the provenance rule, discovery, room, content
> bounds, setup resolution and the generic presentation are current production source with a
> real dynamically loaded witness. What is missing is everything *around* it — a way for an
> end user to install or select a provider.

---

# Part A — a compiled-in Workshop panel

You are editing Workshop. Everything in this part is ordinary source-contributor work.

> ⚠ **Read this part as the mechanics, not as the current catalog.** It was written when Workshop
> compiled several panels in — the Builder, Info, the source Editor, Files, the host's own Pane
> Manager — and every one of them has since become a loaded pane on Part B's path; the built-in
> catalog holds one row, `Layouts` (kind 4; 0–3 and 5 are retired and never reused). So the
> example rows below (`kBuilder`, `kInfo`), the facts that name them, and the helper
> `paint_panel_row` (gone; a painter takes a `FineRect` and writes regions) describe the catalog
> as it was — the example will not compile as written. What is current: `kPanelCatalog`, the
> three catalog assertions (the right column now asserts **zero** kinds), `paint_panels`' arms
> (`Layouts`, then a maker's pane, then the generic external arm), `bounds_of`, and that a
> built-in row reaches the maker through the one inventory the desktop's Pane Manager reads.
> Presence is two doors on the desktop seam, not a picker; there is no object document. Every
> tool added since Info became a weave has taken Part B.

## What you need to know

- **C++20.** Workshop is header-heavy and everything below is ordinary code.
- **Basic Loom concepts** — a weave, a message, a schema, a grant — *only if your panel talks to
  something*. A panel is not a weave and needs no bus: the Layouts pane opens, presents and
  closes without sending a single message. If your panel asks another weave for something you will need
  `mail.send_to_role(...)` and the vocabulary of whoever answers; nothing else here requires it.
- **Where Workshop lives**, which is five files under `workshop/`:

  | file | what you go there for |
  |---|---|
  | `workshop/panel.hpp` | the **built-in catalog**: which panels this build compiles in, what they are called, where they go, what session state each one owns — and the session-local runtime catalog beside it |
  | `workshop/setup.hpp` | **identity and intent**: `PaneRef`, the combined catalog, resolution, and how authored intent becomes open presentations |
  | `workshop/screen.hpp` | **painting** and every **resolved place** — where a thing is, and the inverse a press is answered with. The declarations, the constants and the constexpr functions; the bodies are in `workshop/screen_<subject>.cpp`, one file per subject the header's section banners name |
  | `workshop/weave.hpp` | **input**: the key modes, the pointer chain, and the operations a gesture performs. The class and its declarations; the bodies are in `workshop/weave_<subject>.cpp` |

  (A fifth, `workshop/document.hpp`, held the prototype object document, and retired with the
  object canvas.)

  `workshop/workshop.cpp` is the host. You do not normally touch it, and you rarely touch
  `workshop/CMakeLists.txt`: the vocabulary is a header-only interface target and the bodies
  compile once in `zengine-workshop-logic`, whose source list is the subject files that
  already exist, so a new panel painted in one of them is **no build integration at all**. A
  new subject file is one line in that list — and the list is regenerated from
  `tools/workshop-split/sheet.tsv`, so a hand-added line lives until the next rerun.

You do **not** need to understand the Layouts pane's tab run, the Terminal's transcript, the
Builder's status protocol, the external-pane protocol, or anything in `surface/` beyond two
vocabulary types.

---

## The smallest useful panel

This is a complete, working read-only panel: a kind, a pane key, a catalog row, a painter and
one dispatch arm. There is nothing else — no registration call, no base class, no CMake, no
message.

In `workshop/panel.hpp`, give it an identity, a durable key, and a catalog row:

```cpp
namespace panel {
inline constexpr std::int64_t kBuilder = 0;
inline constexpr std::int64_t kInfo = 1;
inline constexpr std::int64_t kStatus = 2;   // <-- yours
} // namespace panel

namespace pane_key {
inline constexpr const char* kBuilder = "builder";
inline constexpr const char* kInfo = "info";
inline constexpr const char* kStatus = "status";   // <-- yours
} // namespace pane_key

inline constexpr PanelKind kPanelCatalog[] = {
    {panel::kBuilder, placement::kOverlayStack, kWorkshopProvider, pane_key::kBuilder, "Builder",
     "build one known target"},
    {panel::kInfo, placement::kSideRegion, kWorkshopProvider, pane_key::kInfo, "Info",
     "objects and properties"},
    {panel::kStatus, placement::kOverlayStack, kWorkshopProvider, pane_key::kStatus, "Status",
     "what this tool has to say"},
};
```

Declare the painter in `workshop/screen.hpp`, write its body in the subject's
`workshop/screen_<subject>.cpp`, and add one arm to `paint_panels` (whose body is in
`workshop/screen_compose.cpp`, the composition's file):

```cpp
inline void paint_status(surface::SurfaceLayer& layer, const ui::Rect& b) {
    paint_panel_frame(layer, b);
    paint_panel_row(layer, b, 0, "STATUS", surface::role::kAccent);
    paint_panel_row(layer, b, 1, "Workshop can see me.", surface::role::kFill);
}

// ...inside paint_panels, beside the kBuilder and kInfo arms and BEFORE the
// generic `is_runtime_kind` arm:
            } else if (p.kind == panel::kStatus) {
                paint_status(layer, b);
            }
```

**Your painter is handed a `SurfaceLayer`, not the canvas** (WIND-2a). `paint_panels` gives
every presented pane a plane of its own, in the setup's canonical `front` order, so a pane a
maker raised covers the ones behind it *whole* — rects, labels and regions together. You write
into the plane you were given and never reach for the canvas; that is the entire cost of the
ordering to you, and it is what makes "the front the host hits is the front the medium paints"
true of your panel without your panel knowing the rule exists.

Build the host and run it; press `Ctrl`+`p`, move to **Status**, press Return. Your panel is on
screen, in both media, and the Pane Manager's `x` already knows how to close it again.

---

## A1. Give the panel identity

`PanelKind` (`workshop/panel.hpp`) carries **six** fields, and the built-in example above fills
all of them in order:

```text
kind         a plain std::int64_t constant in `namespace panel`
placed_in    which of Workshop's two places this kind occupies
provider     the durable provider/service key -- `kWorkshopProvider` for a built-in
pane         the durable pane key, in that provider's namespace
name         what the Pane Manager lists
summary      one line, so a maker can tell what they are about to open
```

- **`kind` is a build-local handle and is never persisted.** It is this build's index into its
  own vocabulary, renumbered by an edit to the two lines above it, and a saved setup never sees
  it. It is an integer rather than an enum class because it sits beside canvas roles and input
  scancodes in code that is read together.
- **`provider` + `pane` are the durable identity.** Together they are a `PaneRef`
  (`workshop/setup.hpp`), and that pair is what a maker's saved setup file holds — never
  `panel::kInfo`, never a catalog ordinal, never a `WeaveId`.
- **Built-ins use `kWorkshopProvider`** (`"zengine.workshop"`). Spell the pane key as a named
  constant in `namespace pane_key`, not as a literal in the catalog: the suite and the file
  format both have to say it, and a typo in one of three copies is a setup that loads as
  unresolved.
- **A provider key is a route, not a credential.** It says which namespace to read a pane key
  in. It does not say who authored the pane, which binary is running, or that anything claiming
  the string is authentic.

**The compile-time assertions own the requirements, not this prose.** Three `static_assert`s
under the catalog refuse a row that would fail silently:

```text
kinds_placed_in(placement::kSideRegion) == 1   two kinds DEFAULTING to the right column
                                               resolve to ONE rectangle, unasked
every_kind_is_referable()                      an empty provider or pane key is a row
                                               a saved setup can never name
every_reference_is_one_kind()                  two rows sharing a PaneRef make one of them
                                               unreachable through a saved setup
```

`name` and `summary` still default to `""`, and an unnamed row is legal — it produces a blank
Pane Manager row that opens a working panel. Write both.

**A new kind does not join the default setup.** `kDefaultPanels` (`workshop/panel.hpp`) is the
one place "a fresh Workshop shows Info" is decided, and both `default_panels()` and
`default_setup()` read it — so what a fresh Workshop opens is a separate authored decision from
what this build can present.

## A2. `kPanelCatalog` is the **built-in half** of the inventory

`kPanelCatalog` is the source door for a compiled-in panel. It is **not** the whole population a
maker chooses from. Since WP-0 the population is the **combined** catalog
(`combined_catalog`, `workshop/setup.hpp`), and the one inventory Workshop publishes for the
desktop's Pane Manager (`PaneInventory`) is built from it:

```text
every compile-time built-in, in the catalog's own order
then every admitted runtime pane, in first-accepted-offer order
```

- **Still true:** a kind that is in neither half cannot be opened by any gesture at all. Two
  doors change presence — `PaneLaunchRequested` opens or focuses, never toggles;
  `PaneCloseRequested` takes the row off the desk and unloads nothing
  (`workshop/desktop_seam_vocabulary.hpp`) — and the Pane Manager spends them.
- **No longer true:** that "the catalog" means one array. A pane may exist this session without
  being in your source. See [Part B](#part-b--an-office-authored-external-pane).
- **A runtime handle is not an identity.** Runtime kinds start at `kFirstRuntimeKind` (1024) and
  are minted, spent and discarded by one session; `is_runtime_kind` is the one predicate that
  asks. Never write one to a file, read one off a message, or compare one across processes.
- **`panel_kind(kind)` is total and answers with the catalog's first row (`Layouts`, now) for
  anything it does not recognise.** That is correct for its own built-in callers, and a lie about any other kind. The
  fallible direction — the one that meets a file — is `resolve_pane(ref, runtime)`, which
  answers with *nothing*. `kind_name(panels, kind)` is the one that is safe for a runtime kind.

## A3. Let Workshop grant it room

> **Workshop grants a bounded rectangle. The panel publishes inside it.**

A kind **declares an intent**, not a coordinate. There are exactly two places:

| `placement::` | where | rectangle |
|---|---|---|
| `kSideRegion` | the column at the workspace's right edge — a place, reserving nothing | 28 cells wide, as tall as the workspace |
| `kOverlayStack` | over the workspace, from the top-left, stacked downwards | 9 cells tall; 63 cells wide at the minimum composition and wider on a wider surface (below), one blank row between slots — **and all of it a default a maker may override** (WIND-2) |

`placement_bounds(where, slot, screen)` turns an intent into the DEVELOPER'S DEFAULT rectangle;
`bounds_of(panels, setup, kind, screen)` lays the maker's authored override over it, per axis, and
clips the result to the canvas. That second one is what a painter and a press both call. **Your
painter is handed the rectangle**; it never computes one. Read `b.x`, `b.y`, `b.w`, `b.h` and stay
inside them — and note that since WIND-2 those four numbers may be **anything a maker asked for**,
including a rectangle much narrower or much shorter than the default you were designed against, and
including one clipped at the screen's edge. A panel that fits its default and not a maker's is a
panel that will look broken the first time somebody drags its corner.

Four things follow that are easy to get wrong by assuming otherwise:

- **Exactly one built-in kind defaults to the right column.** A second kind declaring it is a
  *compile error* with the reason in the message. A maker's setup file may still put any pane
  there by naming the place, and two panes in one place is a desk asking for that.
- **The overlay stack has a finite number of slots, and it is small.** `stack_slots_that_fit`
  counts them by asking `placement_bounds` — there is no second arithmetic — against the floor
  the workspace ends at, which is the row WS-0 spent on the setup line. **At the 78 × 22 minimum
  composition exactly one overlay slot fits**, asserted in `screen.hpp`.
- **What does not fit is refused or waits, and neither of those edits your source.**
  - A launch that would not be seated is **refused before the active setup moves**: the launch
    door seats a trial candidate first, says `no room for X on this screen -- make the window
    taller, then try again`, and leaves the setup untouched.
  - Authored intent that *already* names more panes than fit is **retained and marked
    `waiting`** — a third state that is neither `open` nor `closed`. The reference is not
    unresolved (this build knows exactly what it would draw) and not closed (the maker authored
    it). Growth opens it with no gesture; a shrink closes the presentation through the ordinary
    close door.
  - Since WIND-2 there are **seven** such words rather than three, and they are one classifier's:
    `closed`, `unresolved`, `refused` (an authored unit this medium cannot project — today, any
    `pixels` amount), `waiting`, `off-room` (the authored place put it off this canvas),
    `covered` (every visible cell is behind the union of what is in front) and `open`. Every pane
    the setup names has exactly one row in the inventory the Pane Manager lists, **whatever state
    it is in** — which is the promise that a maker can never lose one.
- **A maker may move and resize your panel, and reset it back.** Setup version 3 carries an
  authored `place`, `width` and `height` per pane row, each with a MODE — `default` means *no
  override, keep taking whatever the developer's answer becomes* — and since WUX-2 the authored
  amounts are *sub-cell units* (1/48 of a canvas cell), so under a graphical medium a maker's
  drag lands to the pixel while your pane's granted room is still the same prose-rows-and-columns
  budget it always was: fineness changes where the rectangle sits, and the grant door republishes
  only when the resolved capacity actually moves. Every axis is independent, so a
  maker who moved your panel has said nothing about its size, and it goes on following the rule
  below. `w` arranges the desk; `0` inside it resets one dimension at a time. **You author
  nothing about this and there is nothing to opt into**: a kind declares a `placement::` and the
  rest is Workshop's and the maker's.
- **A maker may also change what is in FRONT of what.** Panes can overlap now, so `bounds_of` is
  no longer a promise that your cells are visible. The order is a canonical rank on the setup row
  (`front`), paint walks it back-to-front and the pointer walks it front-to-back, and reordering
  writes nothing that moves, resizes, seats or regrants anything.
- **An authored place takes your panel out of the tiling.** A pane the maker put somewhere spends
  no stack slot and cannot become `waiting` — which is what frees the tile for whatever was
  waiting behind it. Resetting the place puts it back.
- **A wider window widens your stacked panel, and by half.** The right column keeps its width at
  every extent and only its *height* follows the screen. An overlay slot's width is
  `48 + (room_w - 48)/2`, floored, and `room_w` is the whole surface — 63 cells at the 78 × 22
  minimum, 74 at 100 columns of surface, 84 at 120, 124 at 200 — so the surplus a bigger surface
  gives the workspace is split evenly between your panel and the material underneath it. At the
  narrowest screens that width reaches into the right column's place, which is what an overlay
  does and is why your panel wears a boundary. Its **height does not follow the
  screen at all**, and neither does the number of slots: those answer to `kStackRows` and to how
  much room is left above the setup line.
- **Every cell of your rectangle is yours for the POINTER as well as the paint.** A press inside
  your bounds never reaches the room under it — it is answered with *"<your panel> is here"*
  — so a wider panel is a wider thing standing between a maker and their work. That is the price
  of the width and it is the reason the share is a half rather than the whole room: at every
  extent above the minimum, columns of your own rows are still the maker's to press.

**A resolved rectangle is not authored and is not a maker's to move.** There is no docking, no
dragging and no saved layout; the catalog row is the whole of a kind's say in where it goes. The
overlay rectangle — its height, and the half-share its width is resolved by — is current
behaviour, not a promise; it is a pure function of the place, the seated slot and the screen, and
never of your panel's kind, its content or its size a frame ago. A later layout phase may change
it, and the one-measurer rule above is what makes that a change to one function.

## A4. Publish meaning

Your painter writes into a `surface::SurfaceCanvas`. **You never write medium-specific code**:
one painter feeds the terminal Skin and the SDL Skin, and each medium answers for itself how a
semantic row becomes visible.

There are three ways to say something, and choosing between them is one question:

> **Is the rectangle mine?**
>
> **Yes** — a `SurfaceTextRegion`. It clears its whole bounds first, so nothing may be under it.
> **No, but I am writing ON it** — the same region with `ground = surface::kGroundBeneath`. Same
> bounds, same fit, same real type; it draws its rows and nothing else, so whatever is beneath
> shows wherever a glyph does not.
> **It is not a rectangle at all — this CELL is the meaning** — a `SurfaceLabel`.

Almost every tool wants the first. The second exists for one shape of problem, described below;
reach for it only when there is real material underneath that a maker must keep seeing.

**One bounded region — the default, and what your panel wants.** Push a
`surface::SurfaceTextRegion` onto your plane's `texts`, carrying `rows` of
`SurfaceTextRow{text, role, background}`. A region is the only shape on the canvas that a
graphical medium sets in **real type**, and the only one that can carry an **insertion point**
(`caret_row` / `caret_col`, in rows and columns of *your* prose — never a pixel). For a panel
whose whole rectangle is yours, `panel_prose_place(b, sc)` is the one call: it asks
`surface::fit_region` **once** for how many rows and columns your bounds hold, and
`panel_prose_region(b)` hands you the region they were resolved for. Spend that budget and never
multiply a font metric yourself. The same rectangle honestly holds 9 prose rows in a terminal and
5 of an 18-pixel face; that is two projections of one body, not two designs — so a list of
unknown length is *windowed* against the budget you were given (`list_window`, `omitted_text`)
rather than against the cells.

**A region whose ground is BENEATH — for semantic type ON material somebody else owns.** Set
`region.ground = surface::kGroundBeneath` and everything else stays exactly as above: the bounds
are still what `fit_region` resolves and what your rows are cut against, and a graphical medium
still sets them in real type. What changes is that nothing is painted that you did not write — no
padding in a character medium, no fill in a graphical one — so a rect published earlier in the
same plane shows through. Workshop's one consumer is the maker's name written across an authored
object: the name is semantic (its cell occupancy is no part of what the maker authored) and the
object's body is authored *material*, so neither of the other two answers was true. Three things
to know before you reach for it:

- **A row that names its own `background` still paints its strip**, at the region's full width, in
  both media. Giving up the region's ground does not give up the row's.
- **You are giving up the erasing, and that is the whole point.** If there is nothing meaningful
  underneath, an ordinary region is the honest shape and this one just makes your panel
  see-through.
- **Bound it to the material it is written on, and not to the room you happen to have.** An
  ordinary region carries its own ground, so its ink is guaranteed to read against something you
  chose; a `kGroundBeneath` region gives that guarantee up along with the ground, and its ink is
  only as legible as whatever it lands on. Workshop learned this by measurement: the object name
  was bounded by the *workspace's* right edge, so every character past the object's own edge was
  set in the backdrop's exact colour and could not be read (QR-3). Give such a region the bounds
  of the thing it is about, and let `detail::fit` say when they were not enough.

**Labels — for glyphs whose CELL is the meaning.** `paint_panel_row(layer, b, line, text, role)`
writes one row at cell `b.y + line`, `detail::fit`-cut and space-padded to `b.w`, and a raw
`layer.labels.push_back(SurfaceLabel{...})` writes text at a cell with no fit and no padding at
all. Both are one cell per byte in **every** medium, always, and a label always takes its cell.
Reach for them when the cell itself is the unit: a single affordance glyph at one cell (the `+`
size handle, the eight pane-edge marks), or chrome sharing a row with somebody else's sentence.

Three facts about the picture that nothing warns you about:

- **A region takes its rectangle, and that is what makes it honest.** It clears its whole bounds
  before a row is drawn — spaces in a character medium, its own ground in a graphical one — so a
  region is an *overlay* that owns its interior. You get the padding for free (a panel that says
  three rows into a nine-row slot is opaque for all nine, and `paint_panel_frame`'s backdrop
  rect underneath it is erased in both media). The same property is why you must **not** put an
  ordinary region over material you meant to keep: it will erase it. `kGroundBeneath` is the
  *deliberate* way to say you meant to keep it — never an afterthought when a panel turns out to
  be covering something.
- **A one-cell-tall region is not set in the medium's own type.** A canvas cell is
  `kCanvasCellPx` (12 device pixels) and this repository's face has an 18-pixel line, so
  `fit_region` answers *zero* rows for a one-cell region and hands it back to the cell projection
  (HD-5) — the same bitmap letterform a label draws. Publishing a one-row label as a one-row
  ordinary region therefore changes nothing a maker can see. **Two cells is the smallest room
  that holds one row of real type**; a run of rows is what makes an ordinary region worth having.
  (A `kGroundBeneath` region is worth publishing at *any* height, because what it buys you is
  not only the type — it is that a cell it does not write is a cell it does not touch, in either
  fidelity.)
- **A bare `c.labels.push_back(SurfaceLabel{...})` is not padded or fitted.** `detail::fit` marks
  what it cut; a raw `resize` does not, and a shorter name that looks finished is a lie about the
  thing it names.

Roles are semantic, not colours: `kAccent` for a heading, `kFill` for material, `kMuted` for the
panel's own furniture, `kAlert` for something live. A row may also sit on a `background`; the one
ground every ink reads on is `role::kMuted`, and **never pair a role with its own ground**.

## A5. Add interaction only when you need it

The mechanics in this section and the two after it are **compiled-in only**: the press chain,
`command()` and a mode kept in Workshop's session. An external pane gets the same abilities —
a press, keys and text, actions with rebindable keys, a text field of its own — through the pane
protocol instead ([B5](#b5-a-press-and-the-input-you-may-accept)).

A press arrives at `WorkshopWeave::on(const input::PointerButton&)`, declared in
`workshop/weave.hpp` and defined in `workshop/weave_pointer.cpp`, which is a chain of handlers
under `if (b.pressed)`. Each answers one question:

```text
true   this layer CONSUMED the press -- stop routing
false  this layer did NOT consume it -- carry on to the next
```

Nothing else. Not "it succeeded", not "something changed", not "that was mine". **A consumed
press does not have to change anything** — it only has to have reached the layer that owns what
the press means. A press on the row a caret is already on is consumed; a press your panel
deliberately declines returns `false` and the layer around it answers.

Add your handler to the chain **before** the `occupied_at` fallback (which is what tells a maker
"Status is here — nothing under it can be taken hold of"), and after any handler whose claim is
narrower than yours.

The chain already carries the resolved canvas cell (`const PointedAt at`), so a cell-grained panel
needs no geometry at all beyond its own row:

```cpp
inline constexpr std::int64_t kStatusActionLine = 3;   // said ONCE

inline bool status_action_hit(const ui::Rect& b, std::int64_t cx, std::int64_t cy) noexcept {
    return b.contains(cx, cy) && cy - b.y == kStatusActionLine;
}
```

The painter writes at `kStatusActionLine` and the hit test reads it. **The geometry that draws a
thing and the geometry that hits it must be the same geometry** — do not add a
`click_..._bounds()` beside a `paint_..._bounds()`, and do not round a press to a cell if you
drew with a font metric (a graphical row is 18 device pixels against a 12-pixel cell). For a
region, `prose_at(space, x, y, region_x, region_y, fit)` is the one route from a raw pointer fact
to a row and column of your prose; pass it the same `fit` your painter spent.

Motion is *not* occluded by a panel, and a release always ends a drag wherever the hand is. You
do not need to think about either unless your panel starts a gesture of its own.

## A6. Share one semantic operation between the pointer and a hotkey

A command-mode key is one line in `command()`, declared in `workshop/weave.hpp` and defined in
`workshop/weave_terminal.cpp`:

```cpp
case input::scan::kI: status_bump(); break;
```

and the operation is an ordinary private method that **both** the key and the control call:

```cpp
void status_bump() {
    if (!session_.panels.has(panel::kStatus)) {
        return;   // an unbound key while this panel is closed
    }
    ++session_.panels.status.count;
    say("status = " + std::to_string(session_.panels.status.count), false);
}

bool status_press(const PointedAt& at) {
    const PanelBounds me = bounds_of(session_.panels, panel::kStatus, screen_of(session_));
    if (!me.open || !status_action_hit(me.rect, at.cell.x, at.cell.y)) {
        return false;
    }
    status_bump();
    return true;   // consumed
}
```

There is no callback, no command id, no action registry and no `std::function` anywhere in this.
Two gestures, one write, one sentence. Guard on your panel being open so the key means nothing
while it is closed — and pick a key that is currently unbound, because `command()` is the whole
list.

`say(text, is_bad)` writes the one notice line. Say something: a press that changed nothing and
said nothing leaves the previous gesture's sentence sitting beside a maker who has just done
something else.

## A7. Use a component when it owns a useful invariant

Not everything interactive is a component. `[ Create ]` and `[ Delete ]` in the Info panel are a
label, a bit and a bracket convention in a two-row table — presentation with no rule to keep —
and no `Button` type exists. Reach for a component when something owns an **invariant**.

`zengine::component::TextBox` (`component/text_box.hpp`) is the one that does. Hold one in your
panel's session state and it owns:

```text
the text, the caret, and the horizontal window, as ONE state
the UTF-8 character walk (never split a character)
the four caret-follow rules
the visible slice, and column <-> byte through that window
```

and **you** own exactly four things:

```text
where the box lives        the row it is drawn on, and the columns it may use (an ARGUMENT)
what the text MEANS        parse, match, validate, refuse
what a commit does         Return/Escape, and what they leave behind
the reconcile              box.keep_caret_visible(columns), ONCE PER REPAINT
```

That last one is the rule that is easy to miss. Call it before anything is painted and before
the next press is mapped — not on edits — because a **resize** changes the room without being an
edit, and the window a press is answered with must be the window the maker is looking at. The
existing reconciles (`refresh_terminal`, `refresh_inspector`, `refresh_setup_name`) sit side by
side in `repaint`; put yours beside them.

Publish the caret as `region.caret_col = <what your row begins with> + box.caret_column()`, and
resolve a press as `box.position_at_column(pressed_column - <what your row begins with>)`. Those
two offsets are one number; spell it once as a constant.

**Typing needs a mode.** Workshop has no focus object and no z-order — it has an ordered list of
contexts (`keyboard_context`, `workshop/screen_arrange.cpp`), and the order is the priority: the
arrangement scopes, the contextual surface, the layout's name line, a loaded pane holding the
keys, else command. A panel that takes text is one more branch, and its bit lives in your own
pane struct. You cannot skip this by "taking text whenever my panel is open": in command mode
every printable key is already a command, so `.` would both step the layout and type a `.`. (The
last built-in that took text was the host's Pane Manager; every one since is a loaded pane.)

**A printable hotkey that opens a typing mode types itself.** A key transition and the character
it produced are two facts that are both true and both arrive, so pressing `f` to start filtering
puts an `f` in the box unless you swallow the next `TextEntered`. Workshop has one owner for
that: `swallow_text_`, a string set by the trigger and cleared by the very next key or the very
next text. The expectation is derived from the consumed binding itself (`expected_text_of` in
`workshop/keymap.hpp`), so every printable application binding — authored or default — pays the
rule through one mechanism, with no per-trigger line to write.

## A8. Keep state in the layer that owns its meaning

These are the kinds of state around a panel, and the ones that look alike are the ones worth
telling apart:

| kind of state | where it lives | survives |
|---|---|---|
| authored document truth | `WorkshopDoc` (`workshop/document.hpp`) | its own file, and every panel being closed |
| **authored setup intent** | `setup.active` — a name plus ordered `PaneRef`s with their authored geometry (`workshop/setup.hpp`) | **two files**: the one a maker names with `s`, and the last-session file Workshop writes for itself |
| open presentation list | `panels.open` — reconciled from the setup against this build's catalog and this screen's room | until the setup or the room changes |
| built-in panel view/session state | your own struct beside `Panels` in `panel.hpp` | until the panel is closed |
| component mechanical state | inside the `component::TextBox` you hold | as long as you hold it |
| another party's facts | that party; your panel holds a **copy** and says so | the panel closing (the tool does not) |

**Which panes a maker has open IS saved now — as intent, and only as intent.** WS-0 gave the
setup a name and an ordered list of `PaneRef`s; WIND-2 gave each row its authored place, size and
rank; and that value is written to `workshop-setup.json` (format version 2) when a maker presses
`s`, and to `workshop-session.json` when Workshop closes. **One representation, two files** — a
desk saved automatically and a desk saved under a name are the same bytes in the same shape. What
is *not* saved, and must not become saved by accident:

```text
saved in the setup file      the setup's human name
                             ordered PaneRefs, exactly as authored, unresolved ones included

never saved                  which kinds resolved this run, and to what
                             the runtime handle any of them got
                             per-panel view/session state (a Builder's copied status, a draft)
                             an external pane's granted room and cached rows
                             whether a pane is currently waiting for room
```

- **`panels.open` is a projection, not a record.** `reconcile` (`workshop/setup.hpp`) is the only
  thing that opens or closes a panel on a setup's behalf, and the launch and close doors edit the
  **setup** rather than the panel list — so neither can leave the two describing different
  arrangements.
- **Your panel's own view state is still forgotten on close, and that is still the default.**
  Saving pane *intent* does not save what a panel was showing. If your panel owns state, add a
  struct beside `BuilderPane` in `panel.hpp`, a member on `Panels`, and one arm in `close_panel`
  so closing forgets it:

  ```cpp
  if (kind == panel::kStatus) {
      panels.status = StatusPane{};
  }
  ```

  A panel with nothing to forget adds no arm — Layouts holds no copy of anything, which is why
  it can present the layout run without owning any of it.
- **The setup is a value, a law and a file of its own**, on purpose, and so is anything a pane
  holds: a pane that owns a document (the Editor's source is the example) keeps it in its own
  weave, never in the setup. The prototype object document was the first such separate file,
  until it retired with the object canvas.
- **The last session is a third file and a third promise, and your panel owes it nothing.**
  Workshop writes the active setup and the surface's room to `--session` when it closes and reads
  them back when it starts (WUX-0, `workshop/session_persist.hpp`). It carries the same
  `PaneRef`s and the same authored geometry your pane already persists through the setup, so a
  new panel kind is restored automatically with no line of yours. What it must never grow is a
  place for your panel's own view state: the list of what is never saved, above, is the same list
  for both files.
- If your panel shows another weave's facts, hold a copy plus **one fact of your own**: whether
  you have been answered yet. "The tool has never built anything" and "the tool has not answered
  me" are different sentences and a maker acts on only one of them.

## A9. Test the truths your panel owns

Workshop's tests are six suites, one per area, and a case goes in the one whose subject it
proves:

| suite | what it proves |
|---|---|
| `tests/test_workshop_document.cpp` | the typed rows a property is edited through, the keymap, and Workshop's own text boxes |
| `tests/test_workshop_screen.cpp` | composition and geometry — what is painted where |
| `tests/test_workshop_panels.cpp` | the panels Workshop ships, and the attention surface |
| `tests/test_workshop_panes_*.cpp` | the external pane seam, from both sides — five sources under one suite: the seam, the window, input, introspection, sampling |
| `tests/test_workshop_persistence.cpp` | what survives a process |
| `tests/test_workshop_load.cpp` | which artifacts are in the room at all |

A panel of your own is `test_workshop_panels.cpp`; a pane your office authors and Workshop
grants a room to is one of the `test_workshop_panes_*.cpp` sources — the one whose subject
yours is. Each is already registered, so nothing in
`tests/CMakeLists.txt` or `tests/test_population.txt` changes unless you are deliberately raising
a floor. Fixtures more than one suite needs live in `tests/workshop_support.hpp`; a helper only
your suite uses stays in your suite's file, and moving one into the shared header makes every
Workshop suite rebuild. Three shapes cover almost everything a panel owes:

**It gets the room the placement path says it gets** — resolved the painter's way, never by
computing a rectangle in the test:

```cpp
Session s;
REQUIRE(open_panel(s.panels, panel::kStatus));
const Screen sc = screen_of(s);
const PanelBounds me = bounds_of(s.panels, panel::kStatus, sc);
REQUIRE(me.open);
CHECK(me.rect == placement_bounds(placement::kOverlayStack, 0, sc));

const surface::SurfaceCanvas c = paint(d, s);
CHECK(label_at(c, me.rect.x, me.rect.y).substr(0, 6) == "STATUS");
```

**A gesture reaches the operation** — through `Live`, the fixture that drives the real weave on a
real bus with nothing but published input messages:

```cpp
Live t;
hand_launch(t, PaneRef{kWorkshopProvider, pane_key::kStatus});  // the door the Pane Manager asks
t.key(input::scan::kI);            // the hotkey
CHECK(t.session().panels.status.count == 1);
t.press(/* workspace cell of the control */);
CHECK(t.session().panels.status.count == 2);   // and the pointer reaches the same write
```

**The picture and the hit test agree** — assert the row through `label_at` (which goes through
the real cell projection), then press the place you just read and check the answer names the same
row. If your body is a region, resolve it with the same call the painter used and check it under
*both* metrics: `screen_of(w, h)` for cells and `screen_of(w, h, 8, 18)` for a graphical face.
A helper that assumed cells passes on the terminal lane and lies on the SDL one.

**Do not write a catalog census.** `REQUIRE(kPanelKinds == 2)` and
`kinds_placed_in(kOverlayStack) == 1` were both in this suite and both cost a panel author two
red cases and a decision about whether they had broken something. Prefer a claim over the
*population* — a walk asserting every built-in row reaches the inventory, and the partition law
`side + stack + top == kPanelKinds` — which a new kind satisfies for free.

**And a census comes back through whoever needs a fixed population to compute an expected
value.** The example above was recreated against current source and the whole Workshop suite run
over it, and the first time that was done a third built-in kind reddened exactly two cases — both
in the external-pane tier, eleven assertions between them. Neither was a census by intention;
both were arithmetic anchored to one:

```text
the runtime catalog is beside the compile-time one and never inside it
    `rows[2]` was the first RUNTIME row only while there were two built-ins,
    and the case closed with `CHECK(kPanelKinds == 2)`

a list population larger than its rows is windowed, not truncated
    the window arithmetic was computed by hand from a population of exactly
    two built-ins plus eighteen offers
```

Both have since been repaired the way this section describes: the first captures the built-in
prefix before any offer arrives and proves the whole of it survives one, without being told how
long it is; the second takes its population from the catalog's own capacity and its cursors and
omission counts from the list's measured row budget (the picker's, until it retired). **Re-measured against the repaired suite,
a third built-in kind reddens no case at all.**

Take the measurement rather than the number. WG-0 wrote that adding a panel kind cost zero red
cases, and it was true when written; the phase that added external panes made it false in its own
tier without touching a word of it, and nothing announced the change. Add your kind, run the
suite, and believe the run.

## A10. Common mistakes

Each of these was reproduced while writing this page. Several fail **silently** — each row says
which.

| what you did | what happens |
|---|---|
| declared a second kind in `placement::kSideRegion` | **compile error**, with the reason: that region has room for one |
| left the `provider` or `pane` key empty, or reused another row's pair | **compile error** — the two catalog assertions catch it, because a setup that could not name your panel would otherwise fail silently |
| forgot the arm in `paint_panels` | **silent and worse than invisible.** The launch says `opened Status`, nothing is drawn, and a press inside the rectangle is still answered `Status is here — nothing under it can be taken hold of`. An open panel occupies pointer space whether or not anybody painted it |
| put your arm *after* the `is_runtime_kind` arm | harmless today (a built-in kind is below `kFirstRuntimeKind` and never matches it), but the built-in arms belong first: the generic arm is the fallback for panes nobody in this source compiled |
| forgot the catalog row | **silent.** Your painter and its dispatch arm compile as dead code and no gesture can reach the panel |
| left `name` / `summary` empty | **silent.** A blank Pane Manager row that opens a working panel, and a notice reading `opened ` with nothing after it |
| expected your new kind to be open at boot | **silent.** `kDefaultPanels` decides that, and it names Layouts only |
| opened a typing mode from a printable key | **silent.** The key types itself into the box (§A7) |
| wrote your hit test as `constexpr` | **compile error:** `ui::Rect::contains` is not `constexpr`. Drop the `constexpr`; `noexcept` is fine |

One more that is a *lane* fact rather than a mistake: `Home`, `End` and `Delete` do not reach a
Workshop running on the POSIX terminal backend — it drops their CSI sequences, and the Win32
console maps their VKs to `kUnknown`. Bind them anyway (the SDL backend delivers all three), but
test them through the suite rather than by hand in a terminal.

---

# Part B — an office-authored external pane

You are a weave that is **not** Workshop. You can offer Workshop a **pane**: a row in the Pane
Manager, a panel a maker can open, and a bounded budget of prose to fill it with — plus whatever of a
maker's input into it you choose to accept.

This is a different contract from Part A, not a lighter version of it. You get no painter, no
coordinates and no placement. What you get is a budget, a way to speak into it, and sentences
about what a maker did in it.

The exact reference is `workshop/pane_vocabulary.hpp` and
[A weave may offer a pane](../reference/workshop-panes.md#a-weave-may-offer-a-pane). This
section is the orientation; that one is the contract.

## B1. The five shapes every pane speaks

These five are how a pane arrives, is sized, says its rows and hears a press. The protocol has
more — keys, text, the wheel, a sweep, declared actions, a caret, a reveal and the quit, some of
them in a second version beside the first — and each is optional: a pane that neither sends nor
accepts one is unchanged by it. `workshop/pane_vocabulary.hpp` lists every one, with what each
says.

```text
PaneCatalogRequested   Workshop  ->  everyone   "who has panes?"
PaneOffered            provider  ->  Workshop   "I have this one."
PaneRoom               Workshop  ->  provider   "here is how much prose it gets."
PaneContent            provider  ->  Workshop   "here is what it says."
PanePressed            Workshop  ->  provider   "a maker pressed here, in that room."
```

with exactly these payloads:

```text
PaneCatalogRequested   {}
PaneOffered            { pane, name, summary }
PaneRoom               { pane, rows, columns }
PaneContent            { pane, rows<surface::SurfaceTextRow> }
PanePressed            { pane, row, column }
```

- **`PaneCatalogRequested` carries nothing.** A field on it would be a filter, and a filter is a
  policy about which providers may answer that nothing has asked for.
- **`PaneContent` reuses `surface::SurfaceTextRow` directly**, so your row carries the same
  semantic `role` and `background` every first-party row does and the Skin's palette answers for
  it unchanged. You supply no `SurfaceRect`, no `SurfaceLabel`, no `SurfaceTextRegion`, no
  coordinate, no z-order, no viewport and no caret.
- **`PanePressed` is the room you were granted, read backwards** — a row and a column of the same
  lattice `PaneRoom` gave you, and nothing else. [B5](#b5-a-press-and-the-input-you-may-accept)
  is the whole contract.
- **There is no provider field in any payload.** That absence is load-bearing; the next section
  says why.

## B2. Provenance: the office authors, the payload does not

`PaneOffered` and `PaneContent` say *which pane* and never *whose*. The provider half of the
durable `PaneRef` is `mail.authored_role()` — the office Loom verified at the moment the
sentence was authored, carried as delivery provenance that no payload can write and no sender
can choose (MSG-07).

So the absence is enforcement rather than economy: there is nothing to compare against the stamp
because there is nothing to compare. A `provider` field would be a second answer to a question
that already has one, and the second answer is the forgeable one.

Three consequences you must design around:

- **Author deliberately, through your office.** `mail.as_role(kMyOffice).send_to_role(...)`
  registers; `mail.send_to_role(...)` does not — **even when you currently hold that office**.
  Holding an office is not speaking as one. Personal speech is dropped silently: no catalog
  change, no cache, no notice.
- **Verify the ask before you answer it.** `PaneCatalogRequested` is a *publication*, so it
  reaches every weave that accepts the shape and there is no addressing to read intent from.
  Check `mail.authored_from_role("zengine.workshop")` before offering, or you hand your catalog
  to whoever asked. Do the same for `PaneRoom`: a forged room grants nothing.
- **A role is a live service route, and nothing more.** It proves the sender held this office at
  this moment, on this bus, in this process. It is **not** a package author, a signature, a
  publisher, a marketplace identity, or evidence that the same author came back after a restart.
  Do not read `mail.sender()` as identity either — it is a `WeaveId`, so a reloaded provider
  would be a different pane.

## B3. Discovery, and the host limit right beside it

Discovery converges in either load order, with no polling and no timer:

- a provider loaded **first** announces on its attested `loom::Activated` to an office nobody
  holds yet, and that sentence is simply gone — nothing is retried, queued or buffered;
- Workshop then office-publishes `PaneCatalogRequested` from its ordinary `SurfaceReady` startup
  hook, and every provider that verified the authorship offers again;
- repetition is harmless: `admit_pane_offer` refreshes an existing `PaneRef` in place and grows
  the catalog by nothing, so identity does the de-duplication and the protocol needs none.

**And here is the limit, immediately — but LOAD-0 moved where it sits.** The production host
(`workshop/workshop.cpp`) mounts its own weave, the build runner and the Builder tool, and a
terminal participant in-process. Everything else — every provider contribution it mounts and every
weave it loads — comes from **one authored file**,
[`workshop/default-load-plan.json`](../../workshop/default-load-plan.json), staged beside the
binary and read at startup. The host names no artifact stem at all; a tripwire in
`tests/test_operator_provider.cpp` reads its source and refuses one.

So adding a **first-party** tool to a running Workshop is now a row in a file:

```json
{ "artifact": "my-tool", "provider": [], "weave": [ { "role": "my.tool" } ] }
```

...plus getting `my-tool.so` / `my-tool.dll` beside the executable. A one-file build recipe
answers that for a stranger: it builds the source against the installed package, and the
Builder's `load it` and frontier actions put the product where the plan loads it from
([edit a running pane](../workshop/edit-a-running-pane.md), with the Tally example). Read
[reference/load-plan.md](../reference/load-plan.md) before you write one: a plan row is an
**execution-authority decision**, not configuration.

**What has NOT changed:** the host does not scan a directory. Dropping a shared library beside
`zengine-workshop` still makes it nothing — only a named row participates, and `LOAD-0` pins that
with a test that leaves a perfectly valid provider artifact in the host's own artifact directory
and proves it is neither opened nor mounted. There is still no installation workflow, no plugin
registry, no discovery and no SDK.

So read the split precisely, because the three parts are in different places:

```text
WORKSHOP           needs no change for a new pane, and INTR-0 proved it: not one
                   line of the presentation's sources under workshop/ -- weave.hpp,
                   screen.hpp, panel.hpp and the subject .cpp files beside them,
                   walked rather than listed -- names Introspection, no kind was
                   minted for it, and the inventory learned its row from a live offer
THE HOST           names no stem at all since LOAD-0; it reads a plan and executes it
THE PLAN           names every artifact. A FIRST-PARTY tool is a row; a THIRD PARTY
                   still has no door -- because putting a row in the plan is granting
                   that artifact execution authority in this process, and nothing yet
                   decides who may do that
```

The Hello provider is the other kind of artifact and is unchanged: a dynamic library loaded by the
Workshop **test fixture**, built by `tests/`, named in no host's boot list, and a protocol
reference rather than a product.

## B4. Room and content

You are granted **prose capacity**, not a rectangle:

```text
rows       how many rows of the ACTIVE medium's type fit under Workshop's header
columns    ...and how wide each is
```

That is `surface::fit_region`'s answer for your pane's body, resolved by Workshop through the
same one call every other bounded region in the application goes through. Never cells, never
pixels, never an extent, never a font, and never the identity of the medium that answered — a
provider that knew any of those could compute a second layout, and two parties measuring one
region is the defect the region type exists to forbid.

Workshop owns placement, coordinates, the header (`<name> @<office>`), the body, occupancy,
spatial capacity and the final `SurfaceCanvas`. You own what your rows *say*.

A grant arrives on exactly three occasions and no others: the pane opens, a valid re-offer
refreshed it, or the resolved rows/columns moved. A screen that changed cells but not prose
capacity says nothing.

The retention boundary, in the order it is applied:

```text
authorship    an empty authored role is personal speech -- dropped
identity      the (office, pane) pair must already be an admitted row -- otherwise dropped
the room      the pane must be open AND have been granted a room -- otherwise dropped
every row     row count <= granted rows; each row's bytes <= granted columns;
              every byte inside SurfaceTextRow's plain-ASCII contract
```

- **An invalid update is refused whole, never truncated.** A pane showing the first eight rows
  of a twelve-row answer, unmarked, presents a partial sentence as your complete one. Workshop
  clears what it was showing, leaves one bounded refusal sentence of its own in the pane, and
  stays open so a later valid update recovers it. The reason is a **standing condition** the
  maker can read in full — named against the already-admitted `PaneRef` and carrying none of
  your message's bytes — and it disappears the moment your next valid update arrives, without
  anything having to be said over it ([what needs a maker's attention](../workshop/attention.md)).
  **Measure your rows against the room you were granted**; a provider that does not loses the
  whole update.
- **A new room clears the stale cache before it is sent**, so the cache can never hold rows
  admitted under a wider room than the one in force.
- **Silence is `waiting`, and never `unavailable`.** Loom gives Workshop no participant-visible
  provider-unload notification, so nothing times out, nothing polls, no catalog row is withdrawn
  and no setup reference is deleted. If you disappear after sending valid content, Workshop
  cannot know that happened and goes on showing your last rows. That is a stated limit, not
  liveness.
- **Closing destroys Workshop's copy and nothing else.** Your weave, your office, your semantic
  state and your catalog row all outlive the presentation; no unload is sent and the pane reopens
  from the catalog and asks for room again.

Everything a live message can make Workshop retain is bounded before a byte is kept: both key
halves by the setup file's own `check_pane_key` (64 bytes, no spaces or control characters), a
name at 32 bytes and a summary at 64 — neither empty, neither all spaces, neither carrying a
control byte — and the combined catalog at 32 total entries, built-ins included. Admission is
atomic in both directions: an invalid first offer adds nothing, and an invalid *refresh* leaves
the last accepted descriptor whole.

## B5. A press, and the input you may accept

### How does my pane receive a maker press?

Accept `PanePressed`. That is the whole of it — there is no registration, no subscription, no
focus request and no opt-in:

```cpp
void on(const PanePressed& press, loom::Mail& mail) {
    if (!mail.authored_from_role("zengine.workshop")) { return; }  // see B2
    if (press.pane != kMyPaneKey) { return; }
    // press.row, press.column -- a place in the room YOU were granted
}
```

A pane that does not accept the shape receives nothing and is unaffected. `PanePressed` is
delivered like any other message: **a read-only pane stays read-only by doing nothing.**

- **`row` and `column` are the `PaneRoom` lattice.** Row 0 is the first row of *your* body — under
  Workshop's header row, which you were never granted and are never told about — and the pair is
  always inside `[0, rows) × [0, columns)` of the room currently in force.
- **You are told no screen coordinate at all.** No pixel, no cell, no canvas position, no window
  origin, no pane rectangle, no chrome geometry and no medium identity. Workshop resolves the press
  through the same `fit_region` that granted your room, so the same gesture in a terminal and in a
  window reaches you as the same two numbers.
- **A press that names no row of your body is not sent.** The header, the pixel remainder under the
  last prose line of a graphical medium, and anything outside the lattice are all still *consumed*
  by your pane — a pane that owns visible room owns pointer refusal for that room — and simply
  produce no message. Workshop does not round them to a nearest row.
- **A press wants no reply.** No input shape has a `consumed` or a disposition: which pane owns a
  press is geometry Workshop already holds, so it is decided there and never asked of you. The
  protocol's few questions are each scoped to one conversation — a pane may ask to be seated,
  and Workshop asks each pane that accepts the question whether it may quit — and
  `workshop/pane_vocabulary.hpp` names them with their answers.

### Who interprets the press?

**You do, and only you can.** Workshop knows a hand landed at row 3 of the budget it granted; it
does not know whether row 3 is a heading, a list item, a note, an omission marker, a blank
separator or nothing at all. That vocabulary exists in your weave and nowhere else.

Two consequences worth designing for:

- **Interpret the press against the projection currently on screen, not a fresh reading.** If your
  rows came from a snapshot, keep enough of that snapshot to map its visible rows back to what they
  represented. Re-querying your source to interpret a press means a maker can select something they
  were never shown — silently, and only sometimes. `introspection/loaded.hpp` returns its row map
  *from the function that builds the rows*, so there is no second calculation to drift.
- **A room grant replaces the projection.** Workshop clears its cache before every grant and shows
  `(waiting for the provider)` until you answer, so during that gap there is no material of yours on
  screen — drop your map with the grant rather than reading a press against a picture nobody is
  looking at.

### Did the maker's keys already point at my pane?

Only if you ask for the press's second version. A press is also the gesture that points the
keyboard at your pane, so a pane that activates a row on a press — the Files pane opens the
selected file — needs to know whether this press *brought* the keys or found them already there.
`v2::PanePressed { pane, row, column, keys_went_here }` says exactly that: `keys_went_here` is
true when ordinary keys were reaching your pane at the instant of the press, and false when they
were elsewhere or when a mode — a layout's name line, the contextual menu — had them.

- **Accept it beside `PanePressed`, never instead of it.** Workshop sends each press once, as the
  second version when the pane's office is held by a weave that accepts it and as the first
  otherwise — so a pane that accepts only `PanePressed` is unchanged, and a Workshop that predates
  the second version still sends you the first.
- **A first-version press says nothing about the keys.** Treat it as "not known", never as "they
  were here": the Files pane only selects on one, and Return still opens.
- **Nothing is sent when the keys leave**, and no answer or retry follows a press your office could
  not take. Adding the door changes what your weave accepts, which a reload in place refuses:
  restart Workshop to load it.

### What is deliberately absent

Still no focus-changed notification, capture, hover, release or double-press of any kind, and no
reply, disposition or acknowledgement for a gesture — with one exception, and it is about `Esc`
alone. Workshop's last meaning for `Esc` is to put the selected pane down, and it cannot see
whether your pane spent the key, so a pane that has something to do with `Esc` keeps it by saying
nothing. If yours had **nothing** more specific to do with the one it was just sent — no draft to
cancel, no list to dismiss, no mode to leave — say so with `PaneEscapeUnspent{pane}`, as the office
that offered the pane, and Workshop puts your pane down: the maker's keys go back to the desk and
your pane keeps its rows, its place and its state. **Send it under the correlation of the message
that handed you the `Esc`** — `mail.correlation()` of the `PaneKey` or `PaneActionRequested` you
are answering — because that number is what says *which* `Esc` you mean. Workshop mints one per
bare `Esc`; an answer that echoes nothing, or that echoes an older `Esc`'s number, moves nothing,
and so does a second copy of one it already spent. Say it only from the `Esc` you were handed and
only when it meant nothing to you; Workshop ignores it once the maker has typed, pressed or
scrolled since, and there is no answer to wait for. (A sweep crosses as `PaneDragged`; keys and
text cross as `PaneKey` and `PaneTextInput` once a maker has pressed into your pane; the wheel
crosses as `PaneWheel` `{pane, dx, dy}` — the notches over your body, unchanged, whether or not
you hold the keys.
Accept it and spend it as your own Up/Down step; a pane that does not accept it is unchanged.
And the actions you declare beside your offer — `PaneActions{pane, rows}`, each row an id in
your namespace, a label and a default gesture as `PaneKey`'s two numbers — come back as
`PaneActionRequested{pane, id}` in place of the key whenever a maker presses the binding that
requests one, after their own keymap has moved it; act on the id, never on the key, and keep
matching raw keys only for what a component of yours owns, such as a text field's editing.
That declaration has a second version, `PaneActions v2`, whose rows carry one more field:
`supersedes`, naming a Workshop action your row stands in for while your pane owns the keyboard.
Use it only if your pane performs one of those operations on a subject of its own — a pane
holding its own document says `document.save` — and keep sending version one otherwise. Version
one is unchanged and will stay unchanged, so a pane built against it keeps working.)
`PanePressed` carries no
button, no modifier and no timestamp, because SEL-0 earned exactly one gesture and the shape's
arrival *is* that gesture.

**One id means one operation, in every mode.** A pane with modes — a list, a chooser, a line being
typed — declares only the current mode's rows and sends `PaneActions` again whenever its mode
changes; each declaration replaces the last. Give an operation that only one mode has an id of its
own, even when it shares a default key with another mode's: the Files pane's Return is
`files.open` while browsing, `files.choose` in its chooser and `files.commit-field` on its recipe
line, all on Return and never declared together. Keep one id across modes only where it means the
same thing in each, as `files.cancel` backs out of whichever mode is open. Workshop names a key by
the rows it holds when it reads the key, so an id can reach you after your pane has moved on: if the
mode you are in does not declare it, do nothing with it, and leave what your pane shows as it was
([Files](../workshop/files.md#moving-around) shows the same from a maker's side).

The mechanics of [A5](#a5-add-interaction-only-when-you-need-it), [A6](#a6-share-one-semantic-operation-between-the-pointer-and-a-hotkey)
and [A7](#a7-use-a-component-when-it-owns-a-useful-invariant) — the press chain, `command()`, a
mode in Workshop's session — are compiled-in only; a pane reaches the same abilities through the
shapes above, and planning a pane around those mechanics plans one that cannot be built.

### Does receiving a reference grant authority over the referenced thing?

**No, and nothing about this seam changes that.** A pane may publish a fact naming something — the
`Loaded` pane publishes `LoadedSelected{pane, library, role}` when a maker presses one of its rows
— and a listener that hears it has learned some strings. It has not thereby been permitted to send
that thing anything, interrogate it, load or unload it, read its state, or assume its role.

> **Values may flow. Authority must not flow implicitly with them.**

A grant in this Loom is per `(shape, version, target)` and is written by whoever mounts a weave. A
value arriving in a message is not one and can never become one. If your pane publishes a reference
and something later acts on it, that actor must still hold authority it was legitimately given.

(The standing limit from [B7](#b7-the-reference-implementation) is unchanged and is a different
statement: an in-process dynamic weave already shares this program's memory. That is the isolation
tier's problem, it predates this seam, and no protocol rule here solves it.)

## B6. Your pane in a saved setup

An external `PaneRef` is authored in exactly the same version-1 setup a built-in is, and the
setup file did not change to allow it:

- **before a valid offer it is unresolved, and it is preserved.** A setup naming
  `third.party/hello` loads, stays exactly as authored, produces no panel and no placeholder, is
  counted on the setup line as `1 unresolved` and named in the notice. The word is
  **unresolved**, never *unavailable*;
- **after an authenticated matching offer it resolves through ordinary reconciliation** — the
  same `apply_setup` path a launch and a restore go through, without the file being touched;
- **if the screen has no room it may be resolved but `waiting`** (see [A3](#a3-let-workshop-grant-it-room));
- **and in a fresh process where you are absent, it is unresolved again.**

Never persisted: your display text, the runtime handle you were given, the room you were granted,
and the rows you sent.

**Two offices offering one pane key are two panes.** The `PaneRef` is the pair, so
`a.tools/hello` and `b.tools/hello` are two rows, two handles and two presentations, and neither
office can refresh or overwrite the other's. A runtime offer also cannot shadow a built-in.

## B7. The reference implementation

`tests/weavelib/workshop_hello.cpp` is the smallest complete witness of this protocol: one
office, one pane key, both authorship checks, a content body formatted from the room it was
actually granted, and nothing else. The one to copy is
[`examples/tally-pane/tally.cpp`](../../examples/tally-pane/tally.cpp): the same shape with a
declared action and state a reload keeps, built from the installed package.

> **It is a test fixture and a protocol reference — not a product plugin and not a deployment
> recipe.** It is built by `tests/`, loaded only by the Workshop suite, and named in no host's
> boot list.

Two excerpts are worth reading before the whole file. Answering the ask, verified:

```cpp
void on(const PaneCatalogRequested&, loom::Mail& mail) {
    if (!mail.authored_from_role(kWorkshopRole)) {
        ++state_.refused;
        return;
    }
    announce(mail);
}
```

and speaking as the office rather than as yourself:

```cpp
void announce(loom::Mail& mail) {
    ++state_.offers;
    (void)mail.as_role(kHelloRole)
        .send_to_role(kWorkshopRole, PaneOffered{kPaneKey, kPaneName, kPaneSummary});
}
```

One thing the fixture reports rather than hides: **a pane grants a provider no authority, and
that is a fact about the protocol rather than about the loader.** A weave loaded normally
in-process through the current Loom receives `Grant{}.allow_any()` by default, so a provider is
trusted in-process code sharing this program's memory. Visibility did not create that and this
seam does not solve it.

---

# What does not exist yet

Named here so you do not plan around it, and so a reader can tell a bounded seam from a product:

```text
no public plugin SDK, registry, marketplace or installation workflow
no provider scan directory, autoload list, or --provider option on the Workshop host
no package, publisher, signature or cross-restart author identity
no provider focus notification, pointer capture, hover or release, and no drag between panes
no provider-owned placement, coordinates, docking, tabs, resize handles or geometry
no multiple instances of one PaneRef
no unload notification, timeout, heartbeat, liveness query or `unavailable` state
no provider acknowledgement of a refused update
no out-of-process provider support
no opaque provider configuration
```

Each is a decision rather than an omission, and every one of them is stated again with its
reasoning in `workshop/pane_vocabulary.hpp` and in the root README section this guide routes to.

---

# The normal maker loop

```bash
cmake --build build -j"$(nproc)" --target zengine-workshop-panels-tests && ./build/tests/zengine-workshop-panels-tests
```

— naming the suite your change can falsify (`document`, `screen`, `panels`, `panes`,
`persistence`, `load`), so the loop builds one Workshop source rather than all six —

then

```bash
cmake --build build -j"$(nproc)" --target zengine-workshop && ./build/workshop/zengine-workshop
```

`--load-plan graphical-load-plan.json`, from the host's own directory, runs the same tool in a
window — it is the second plan this repository ships, and it differs from the default in exactly
the two rows that name the medium and the reader. That is the whole loop while you are working:
build one target, run one suite, look at the thing.

**Before you contribute**, the repository's own lane is what a green must mean — the full
population check, the sanitizer lane, and the documentation-link check. `AGENTS.md` owns it; run
`cmake -DZEN_BUILD_DIR=build -P tests/verify.cmake` rather than a bare `ctest`, because a bare
`ctest` cannot tell you whether the population that ran is the population this repository meant
to run. You do not need any of that to see your panel work.

# Choose only what you need

```text
I am a weave that wants to show a maker some rows
    -> Part B. The pane protocol (workshop/pane_vocabulary.hpp), an office you author
       through, a prose budget you must measure against, and the input shapes you choose
       to accept. Read tests/weavelib/workshop_hello.cpp

I am editing Workshop and I only display information
    -> a kind, a pane key, a six-field catalog row, a painter, one arm in paint_panels

I need session state of my own
    -> a struct beside Panels, a member, one arm in close_panel

I need a pointer action
    -> one constant for the row, a hit test that reads it, a handler in the press chain
       returning true = consumed, and one semantic operation

I also want a hotkey
    -> one case in command(), calling that same operation, guarded on my panel being open

I need editable text
    -> hold a component::TextBox, add a mode, reconcile its window once per repaint,
       and define what the text means and what a commit does

I need real type or a caret
    -> publish one SurfaceTextRegion instead of labels, sized by fit_region, and resolve
       presses with prose_at against the same fit

I need real type ON material somebody else drew
    -> the same region with ground = surface::kGroundBeneath. It keeps the bounds and gives
       up the ground, so it draws its rows and touches nothing else. Only when there is
       real material underneath that a maker must keep seeing

I need to talk to another weave
    -> mail.send_to_role(theirRole, TheirMessage{}), hold a COPY of what they last said,
       and record whether you have been answered yet

I want a maker's arrangement to come back
    -> nothing. Pane presence, place, size and front order are all already authored
       setup intent and are already saved (WIND-2). What your panel was SHOWING is
       session state and is not, deliberately

I want to remember where MY panel was
    -> nothing, and do not. The rectangle is Workshop's answer, recomputed every paint
       and stored nowhere; a copy inside your panel is the stale number the whole tool
       is arranged against
```

# Where to look next

**For a compiled-in panel**, read these in order; each is real, current source:

1. **`paint_layouts`** (declared in `workshop/screen.hpp`, its body in
   `workshop/screen_layouts.cpp`) — the one built-in left: handed a rectangle, writes rows,
   reads nothing but the setup's own facts, and answers presses through the same geometry.
2. **`launch_pane` and `close_pane`** (`workshop/weave.hpp`, with the bodies in
   `workshop/weave_desktop.cpp`) — the two presence doors, and the whole open/focus/refuse and
   close decision a built-in row reaches through.
3. **`paint_maker_pane`** (`workshop/screen_pane_subject.cpp`) — the one other arm in
   `paint_panels`: a pane whose inside is a maker's authored data, drawn by the host.
4. **`component::TextBox`** (`component/text_box.hpp`) and its consumers — the layout's name
   editor in this host, and, as loaded panes, the Terminal's command line and the Pane Manager's
   name line (`desktop-pane/pane.cpp`).

**For an external pane:**

1. **`workshop/pane_vocabulary.hpp`** — the exact wire shapes, their payloads, and the written
   reasons the absent ones are absent.
2. **`tests/weavelib/workshop_hello.cpp`** — the smallest complete protocol witness.
3. [A weave may offer a pane](../reference/workshop-panes.md#a-weave-may-offer-a-pane) — the
   reference account, including every bound and every non-claim.
4. **`workshop/setup.hpp`** — `PaneRef`, `admit_pane_offer`, `resolve_pane`, `seat_panes` and
   `reconcile`, which is where an authored reference becomes a presentation or does not.

For the arrangement a maker saves,
[A setup has a name](../reference/workshop-panes.md#a-setup-has-a-name) is the reference, and
[The code authors a default; the maker authors an override; the host resolves the room](../reference/workshop-panes.md#the-code-authors-a-default-the-maker-authors-an-override-the-host-resolves-the-room)
is the reference for what a maker may then do to your panel's rectangle; for the panel system
itself it is [The panel system](../reference/workshop-panes.md#the-panel-system). A maker's own
view of the same ground is [panes](../workshop/panes.md) and [setups](../workshop/setups.md).
