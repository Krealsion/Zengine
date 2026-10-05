# Described views

Routed behind [AGENTS.md](../AGENTS.md) for `view/` and `view-builder/`. Also read
[panes](panes.md) (the canvas and the carry), [flow](flow.md) (the one shape model) and
[flow host](flow-host.md) (the sibling host). Public contract:
[view reference](../docs/reference/view.md); weaver guide:
[the View Builder](../docs/workshop/view-builder.md).

- A view description is presentation as data: its size, elements inside it, the field a label
  shows, the intent a button says. It holds no business value, no resolved geometry and no
  position in cells or in a fraction of a pixel. Places and sizes are whole pixels,
  `surface::kCanvasCellPx` to a cell. No element sits outside the view's size, and its notice
  rows lie beneath the size, `view::kNoticeRows` lines of the medium's text (`view::notice_band`),
  so no size the rules accept puts an element under the notice.
- `view::problem` is the one set of rules. Every door applies it -- the writer, the reader, the
  host -- so a description that would be refused is never written. The saved form is read like a
  maker definition: the envelope's claim, then the gate, then the rules; a version it does not
  read is refused by its number. A changed shape takes a new version and the old one still reads:
  version 1, without a size, takes `view::fitting_size` and is written as the current version.
- A view's intent sits inside the view's own name, as a definition's emits do (MW-DEF-08): a
  description is pure data and its participant publishes, so a name outside its namespace could
  speak for another participant. A label may show any scalar field of any shape: a view listens.
- `view::Host` registers each view as its own participant, holding the view's name as its office,
  granted `view::view_grant` alone: its intents to any accepter, and the pane conversation to
  Workshop's office. Never `trust_every_artifact`, an allow-any, or the builder's grant.
- Loom fixes a participant's accept-set and emit-set at registration. A change at the same name
  and shapes (`view::same_shapes`) is adopted in place, keeping field text, focus and what the
  view was told; any other change is a fresh registration, and the answer says so.
- A successor registers while the view it replaces still runs, so a registration Loom refuses
  (`loom::SchemaConflict`, a held name) leaves that view and its session as they were, and the
  answer says why. At the same name the successor is registered unbound, sealed to the host's
  office and given the office by `commit_candidate`, with no delivery between; then the old view
  is removed. Renamed, the old view stops. The bound counts the pair as one view.
- A stop queues the view's last picture as its office while it still holds it, then retires the
  participant behind that delivery: the pane says the view stopped until Workshop sees the
  provider gone and shows it waiting for one (WL-CANVAS-02). Neither picture has a control.
- `view::View` keeps only presentation: field text, focus, caret, its notice, and the latest value
  of each shape it was told. It calls nothing current that it was not told: a label it was never
  told says `waiting`. An intent is published as the participant, by shape: whoever accepts it
  hears it, and the view claims no exclusive audience. A refusal is shown only when Loom attests
  it answers the view's own delivery (`answers_ask()`); the correlation then says which intent.
- A running view asks its pane for its size in canvas pixels, never past
  `workshop::kMaxPaneBodyPx`, with its notice rows of text beneath (`view::offered`), and draws
  in its size and notice rows within the room it is granted; a pane smaller than that cuts it.
- `view::picture` is pure: the description, what was told and the presentation, in the view's
  size and its notice rows within one room. Text
  sits on the medium's lattice inside its element and is fitted by `clip_canvas_text`, so a
  terminal shows the window's picture floored to cells. A prose room gets one row saying the view
  needs a canvas; there is no second, text-row renderer.
- ONE RENDERER: `view::picture` is the only drawing of a view. The View Builder's design canvas is
  it, called in a room exactly the view's size and its notice rows with the medium's metrics, and
  moved there whole from a cell boundary less the pan, by whole grains, so the lattice holds on
  both media and what is designed is what runs. Nothing of the view is drawn outside the design
  area. Over it the builder draws marks alone, never an element.
- Workshop composes the host beside the Flow host and holds no view behaviour. The View Builder
  is an ordinary pane in `zengine.view.builder`: it edits a description through
  `view_builder::Model`, every edit whole or refused whole, and asks the host to run, apply and
  stop one session of its office. By hand: a kind dragged from its palette is made where it is
  let go (`add` at a place); an element dragged moves, a side's handle moves that side alone and
  a corner's the two it joins (`place`). The view's size is set by the handles on its right and
  bottom edges or typed into its boxes (`size`). Each place by hand snaps (`view_builder::snap`,
  `view_builder::sized_by_hand`): an edge the hand moves comes to another element's edge, or the
  view's, within `view_builder::kSnapReach` pixels; else to the weaver's grid when one is set, up
  to `view_builder::kMaxGrid`, and without one it stays at its whole pixel. The edge it met is
  marked while held. Alt held sets every snap aside. Nothing sits outside the view: a drag stops
  at its edges, and a typed value or a key that would cross one, or a size that would shrink past
  an element, is refused in words. The arrow keys and a value typed into its box (`set`) place
  exactly. A middle-button drag pans the design canvas within `view_builder::pan_reach`, to the
  view's far edges and their handles; a press, a hover and a drop are read against the
  picture they name, panned as it was drawn, and New and Open show a view from its corner. A
  terminal pans by cells, and a side's handle there is the cell beside that side's middle,
  outside the element, or the side's own where the canvas ends. A box narrower than its value keeps
  its caret in view, and the picture and a press read that one scroll, so a press lands on the
  byte drawn under it. A release keeps a drag's edit and a lost press puts back what it moved.
  The pointer resting on an element marks it (`PaneCanvasHover`), and a carried value marks
  the label it would land on.
- A gesture, the pan, the grid, a box being typed into and a mark are the builder's
  presentation: never saved, kept across a reload or offered to the host; no pan, grid or snap is
  in a description. A reload in place keeps the draft, the name in its File box and, apart from
  it, the file the view was saved to or opened from (`view_builder::BuilderState::file`), and
  whether its view runs. So does a relaunch: the builder writes that file (inside the project,
  named relative to it, so a project moved whole still finds it) and whether its view runs to the
  project's `view-builder.json` (`view_builder::ViewBuilderRun`) when either differs from what the
  file holds, and counts it held only once the write succeeds, so a write that failed is made at
  the next chance, a successor's first included. At its first activation the builder asks
  `zengine.project` for the project and reads that file; at a launch it opens the view the file
  names and, if its view ran, runs it again by `view::ViewResume`: a run whose pane asks nothing
  of the desk, so the restored desk seats it where it stood or leaves it hidden. A launch that
  cannot open that view file keeps the project file as it found it
  (`view_builder::BuilderState::kept`, across a reload too) until the weaver opens, saves or
  starts a view, a name typed into File being none of these, so the file's return brings its
  view back at the next launch. An intent is made
  through `flow/shape.hpp`. The builder carries an intent's shape out, with the shapes it nests
  (`flow::shape::carried`), by a press's drag or a menu choice, and takes a shape, a value or an
  Info field dropped on a label, on the canvas or its
  row, as what that label shows, a description read with the shapes it carries alone
  (`flow::shape::described`); dropped on no label it is refused in words, whatever is selected.
  A shape of several fields it could show is a choice of every one: buttons wrapped within the
  values column (`view_builder::lay_choices`), and More where the rows cannot hold them. A
  control in the values column (Remove, Unshow, an intent's box) is drawn whole inside it: the
  words before it give way, cut to end in `...`.
- `tests/test_view.cpp` witnesses the format, the rules, registration and grant, publication,
  refusal, in-place and fresh apply, a replacement that cannot register, stop, and the terminal
  picture; `tests/test_view_builder.cpp` the one renderer, the palette, the drags, the snap,
  the side handles, the pan, the size, nothing outside it, the grid, the boxes, the marks and the
  terminal floor, the run again at a launch, and the launch record across a reload, a failed
  write and a missing view file; `tests/test_view.cpp` also the size, a version 1 description, a
  view asking for its size,
  and a resumed view; the panes suite the view and the builder through Workshop's real seat,
  pointer route, drag carry and canvas admission.
