# Described views

A **view** is a small pane described as data rather than written in C++: its size and some
elements placed inside it in whole pixels, the fields its labels show, and the intents its
buttons say. The view host
registers each running view as a participant of its own, and the view renderer draws it on its
pane's canvas and in a graphics window. A view is made and run in the
[View Builder](zengine.view.builder.md); this page is the contract beneath it.

## The description

`view/description.hpp` holds `view::Description`:

| part | what it holds |
|---|---|
| `name` | the view's name: its office, its pane's name in the Pane Manager, and its intents' namespace; at most `view::kMaxNameBytes` bytes of letters, digits, `_`, `-` and inner dots |
| `width`, `height` | the view's size in whole pixels, from `view::kMinWidthPx` by `view::kMinHeightPx` to `view::kMaxSizePx` each way: what its elements sit in, with its notice rows beneath it |
| `elements` | at most `view::kMaxElements`, each an `id` (an identifier, since an intent field is named by it), a kind (`label`, `number` field or `button`), a printable `label`, and `x`, `y`, `w`, `h` in whole pixels, inside the view's size; a number field may hold the `text` it starts with |
| `shows` | a label showing one field of a view-model shape: the shape, closure and all, and a scalar field of it |
| `intents` | a button's intent: a shape whose name begins with the view's name and a `.`, every field a required `Int` filled from a number field |

A place or size is a whole pixel, `graphics::kCellPx` pixels to a canvas cell, from 0 to
`view::kMaxPixels`. Nothing in a description is a cell, a fraction of a pixel, a resolved
rectangle or a business value. `view::problem` names the first rule a description breaks, in
one sentence, and every door applies it.

The saved form is the Loom value `zengine.view.Description` at `view::kFormatVersion`, with the
format word `zengine-view-description`. `view::read_description` reads the envelope's claim
first, so a description of a version it does not read is refused by its number before a field is
decoded, then admits the value at that version's schema, then applies the rules. Version 1, saved
before a view had a size, still reads: it takes `view::fitting_size`, the rows and columns its
pane was asked for then (its elements and its notice rows, at least 4 rows by 40 columns and at
most 60 by 200) less the notice rows, which now lie beneath the size, never less than its elements
reach, and is written again as the current version. `view::save_description` and
`view::open_description` use the maker file doors: bounded, and replaced whole.

## The view host

`view::Host` mounts in the office [`zengine.view.host`](zengine.view.host.md) over the host's own
bus. A participant asks it, as its office or as itself:

| ask | answer |
|---|---|
| `ViewRun{session, description}` | registers the view as a participant of its own, holding its name as its office, and tells it to offer its pane |
| `ViewApply{session, description}` | the same name and shapes: adopted in place, the field text, focus and told values kept. Any other change: registered afresh, and `fresh` says so. The successor registers while the running view still runs, which retires only once it stands; a successor Loom refuses (a changed intent at the same name and version, say) changes nothing, and the answer says why. A rename counts as one view against `view::kMaxViews` |
| `ViewStop{session}` | the view's last picture says it stopped, in Workshop and in the window that shows its area, then the participant is unregistered; Workshop, seeing its provider gone, then shows the pane waiting for one |

`ViewAnswer{session, action, ok, reason, office, fresh}` is the one answer. A session belongs to
the office that asked, or to the asker's bus identity, and a host runs at most `view::kMaxViews`.
A view's grant is `view::view_grant(description)`: each intent to any accepter, its pane
conversation to `zengine.workshop` alone, and its drawing to a graphics window, which takes an
area's drawing only from that area's owner, with the ask for its room to `zengine.graphics` alone.
It is derived from the description, never from the asker's grant or from the host's trust in its
plan artifacts.

## The view renderer

`view::View` is the participant's behaviour. It accepts the shapes its labels show, `zen.Refused`,
the pane conversation and a graphics window's; it emits its intents and those conversations, its
actions among them.

- It offers one pane, `view`, as its office, asking for its size in canvas pixels, never more
  than `workshop::kMaxPaneBodyPx` of either, and its notice rows of text beneath
  (`view::offered`), and asks for a seat when it starts. Workshop grants that body exactly: a
  window's pane is the size to the pixel, and a terminal's the cells that hold it.
- It draws in its size within the room Workshop grants: each element at its pixels, a number
  field's text and caret, a button's label, a label and the value it shows, and beneath the size
  a notice in `view::kNoticeRows` lines of the medium's text (`view::notice_band`), so no element
  is under it. Other parties' words on the notice -- a shape's name, a refusal -- are spelled in
  printable ASCII (`workshop::pane_text::ascii_spelling`). A pane smaller than the view and its
  notice rows cuts the picture.
  Text sits on the medium's own lattice inside its element and is fitted to what the medium
  measures, so a terminal shows the same picture floored to cells.
- Focus, caret and field text are its own; a number field takes plain ASCII, and other typed
  text is not typed and is said on the notice row. A press focuses a number field or uses a button;
  `Tab` moves between number fields, `Return` uses the button whose intent takes the focused
  field, and `Escape` puts the focus down or hands the key back.
- Using a button publishes its intent as this participant, each field read from its number
  field; a field holding no whole number is said on the notice row and nothing is published.
  Publication is by shape: every participant accepting the intent hears it.
- Each button that says an intent is an action of its pane (`view::actions_of`): its id the
  button's, its label the button's, no default key, and what a press causes the intent's shape
  ([what a press may cause](../../workshop/docs/workshop-panes.md#what-a-press-may-cause)). The
  view declares them when it offers its pane and again when a change applies in place, and a
  `PaneActionRequested` naming one uses the button as a press does -- so a weaver keys a button by
  naming its id in their keymap file. Its picture names that button `control:<id>`, available,
  and every other element `element:<id>`; a declaration Workshop refuses is said on the notice
  row, and the buttons still answer a press.
- Until a shape it shows has been told, its labels say `waiting` and so does its notice. A
  `zen.Refused` that Loom attests answers one of its own intents (`answers_ask()`, with that
  intent's correlation) is shown on its notice row; what it was told stays as it was. Anyone
  else's refusal, whatever correlation it carries, is not.
- A prose room, from a host with no canvas, gets one row saying the view needs a canvas.
- In a [graphics window](../../graphics/docs/graphics.md) whose area bears the view's name, it
  draws the same picture. It asks its area's room when it starts (`graphics::RoomAsked`) and
  draws in the room a window gives (`graphics::AreaRoom`), in the window's unit and its text: the
  OS window's face, a character an advance of it, or a terminal's cell (`view::sketch`): each
  rectangle and run of text a node named by what drew it -- `ground`, `box:<id>`, `text:<id>`,
  and `notice` for the notice's lines -- and each button's nodes asking its id. A focused number
  field's caret is drawn there as Workshop's panes draw theirs: `caret`, a bar in the accent role
  at the left of the character it stands before, in front of the rest. A press there (`graphics::ActionRequested`) on a button the
  picture last sent there names uses it as a press in Workshop does. A change is sent as what
  changed; a new window, incarnation, size or measure gets the picture whole, a refused change is sent
  again whole, and a refused whole drawing is said on the notice row, the window sent nothing more
  until the picture changes.

`view::picture(description, told, presentation, room, number)` is the one drawing of a view and
is pure. The View Builder draws its design canvas with it too, in a room exactly the view's
size and its notice rows, so a view is designed in the very picture it runs in, at its own
pixels, and a graphics window is sent that picture through `view::sketch`.

## A presenter

`view::Presenter` is an owner's side of a view, for code in the process that runs it. `tell`
gives the view a value of a shape its labels show, and `set(field, text)` reads `text` as that
field's kind and tells the latest value of each shape a label shows it from; each answers why not,
in words, when it cannot. `on_action(id, code)` runs code when the button `id` is used, in
Workshop or in the window, before any intent the button says, so a button with no intent is the
presenter's alone; code that throws is said on the notice row, and nothing is published.

`view::Host::pair(name, presenter)` pairs a presenter with the view running under `name` and any
registered afresh under it, and a null presenter unpairs it; `refresh(bus)`, from host code, has
the view draw what the presenter told outside a press. A presenter holds code, so it never crosses
into another participant: a loadable weave tells a view by its shapes and hears its intents, or
keeps a presenter of its own description, drawing `Presenter::sketch` into the
`graphics::Canvas` it keeps and answering a press there with `Presenter::act`.
