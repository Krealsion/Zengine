# Described views

A **view** is a small pane described as data rather than written in C++: its size and some
elements placed inside it in whole pixels, the fields its labels show, and the intents its
buttons say. The view host
registers each running view as a participant of its own, and the view renderer draws it on its
pane's canvas. A view is made and run in the [View Builder](view-builder.md); this
page is the contract beneath it.

## The description

`view/description.hpp` holds `view::Description`:

| part | what it holds |
|---|---|
| `name` | the view's name: its office, its pane's name in the Pane Manager, and its intents' namespace; at most `view::kMaxNameBytes` bytes of letters, digits, `_`, `-` and inner dots |
| `width`, `height` | the view's size in whole pixels, from `view::kMinWidthPx` by `view::kMinHeightPx` to `view::kMaxSizePx` each way: what its elements sit in, with its notice rows beneath it |
| `elements` | at most `view::kMaxElements`, each an `id` (an identifier, since an intent field is named by it), a kind (`label`, `number` field or `button`), a printable `label`, and `x`, `y`, `w`, `h` in whole pixels, inside the view's size; a number field may hold the `text` it starts with |
| `shows` | a label showing one field of a view-model shape: the shape, closure and all, and a scalar field of it |
| `intents` | a button's intent: a shape whose name begins with the view's name and a `.`, every field a required `Int` filled from a number field |

A place or size is a whole pixel, `surface::kCanvasCellPx` pixels to a canvas cell, from 0 to
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

`view::Host` mounts in the office `zengine.view.host` over the host's own bus. A participant
asks it, as its office or as itself:

| ask | answer |
|---|---|
| `ViewRun{session, description}` | registers the view as a participant of its own, holding its name as its office, and tells it to offer its pane |
| `ViewApply{session, description}` | the same name and shapes: adopted in place, the field text, focus and told values kept. Any other change: registered afresh, and `fresh` says so. The successor registers while the running view still runs, which retires only once it stands; a successor Loom refuses (a changed intent at the same name and version, say) changes nothing, and the answer says why. A rename counts as one view against `view::kMaxViews` |
| `ViewStop{session}` | the view's last picture says it stopped, then the participant is unregistered; Workshop, seeing its provider gone, then shows the pane waiting for one |

`ViewAnswer{session, action, ok, reason, office, fresh}` is the one answer. A session belongs to
the office that asked, or to the asker's bus identity, and a host runs at most `view::kMaxViews`.
A view's grant is `view::view_grant(description)`: each intent to any accepter, and its pane
conversation to `zengine.workshop` alone. It is derived from the description, never from the
asker's grant or from the host's trust in its plan artifacts.

## The view renderer

`view::View` is the participant's behaviour. It accepts the shapes its labels show, `zen.Refused`
and the pane conversation; it emits its intents and that conversation.

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
- Until a shape it shows has been told, its labels say `waiting` and so does its notice. A
  `zen.Refused` that Loom attests answers one of its own intents (`answers_ask()`, with that
  intent's correlation) is shown on its notice row; what it was told stays as it was. Anyone
  else's refusal, whatever correlation it carries, is not.
- A prose room, from a host with no canvas, gets one row saying the view needs a canvas.

`view::picture(description, told, presentation, room, number)` is the one drawing of a view and
is pure. The View Builder draws its design canvas with it too, in a room exactly the view's
size and its notice rows, so a view is designed in the very picture it runs in, at its own
pixels.
