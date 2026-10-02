# Described views

Routed behind [AGENTS.md](../AGENTS.md) for `view/` and `view-builder/`. Also read
[panes](panes.md) (the canvas and the carry), [flow](flow.md) (the one shape model) and
[flow host](flow-host.md) (the sibling host). Public contract:
[view reference](../docs/reference/view.md); weaver guide:
[the View Builder](../docs/workshop/view-builder.md).

- A view description is presentation as data: elements, the field a label shows, the intent a
  button says. It holds no business value, no resolved geometry and no position in cells or in
  a fraction of a pixel. Places and sizes are whole pixels, `surface::kCanvasCellPx` to a cell.
- `view::problem` is the one set of rules. Every door applies it -- the writer, the reader, the
  host -- so a description that would be refused is never written. The saved form is read like a
  maker definition: the envelope's claim, then the gate, then the rules; another version is
  refused by its number.
- A view's intent sits inside the view's own name, as a definition's emits do (MW-DEF-08): a
  description is pure data and its participant publishes, so a name outside its namespace could
  speak for another participant. A label may show any scalar field of any shape: a view listens.
- `view::Host` registers each view as its own participant, holding the view's name as its office,
  granted `view::view_grant` alone: its intents to any accepter, and the pane conversation to
  Workshop's office. Never `trust_every_artifact`, an allow-any, or the builder's grant.
- Loom fixes a participant's accept-set and emit-set at registration. A change at the same name
  and shapes (`view::same_shapes`) is adopted in place, keeping field text, focus and what the
  view was told; any other change is a fresh registration, and the answer says so.
- A stop queues the view's last picture as its office while it still holds it, then retires the
  participant behind that delivery: the pane says the view stopped until Workshop sees the
  provider gone and shows it waiting for one (WL-CANVAS-02). Neither picture has a control.
- `view::View` keeps only presentation: field text, focus, caret, its notice, and the latest value
  of each shape it was told. It calls nothing current that it was not told: a label it was never
  told says `waiting`. An intent is published as the participant, by shape: whoever accepts it
  hears it, and the view claims no exclusive audience. A refusal is shown when it answers one of
  the view's own intents, by correlation.
- `view::picture` is pure: the description, what was told and the presentation, in one room. Text
  sits on the medium's lattice inside its element and is fitted by `clip_canvas_text`, so a
  terminal shows the window's picture floored to cells. A prose room gets one row saying the view
  needs a canvas; there is no second, text-row renderer.
- ONE RENDERER: `view::picture` is the only drawing of a view. The View Builder's design canvas is
  it, called in a room the design area's size with the medium's metrics and moved there whole
  from a cell boundary, so the lattice holds on both media and what is designed is what runs.
  Over it the builder draws marks alone, never an element.
- Workshop composes the host beside the Flow host and holds no view behaviour. The View Builder
  is an ordinary pane in `zengine.view.builder`: it edits a description through
  `view_builder::Model`, every edit whole or refused whole, and asks the host to run, apply and
  stop one session of its office. By hand: a kind dragged from its palette is made where it is
  let go (`add` at a place), an element dragged moves and a corner handle resizes it (`place`),
  in whole pixels; a value is typed into its box (`set`). A release keeps a drag's edit and a
  lost press puts back what it moved. The pointer resting on an element marks it
  (`PaneCanvasHover`), and a carried value marks the label it would land on.
- A gesture, a box being typed into and a mark are the builder's presentation: never saved, kept
  across a reload or offered to the host. A reload in place keeps the draft, file and whether
  its view runs. An intent is made through `flow/shape.hpp`. The builder carries an intent's
  shape out as a `zen.SchemaDesc` by a press's drag or a menu choice, and takes a shape, a value
  or an Info field dropped on a label, on the canvas or its row, as what that label shows.
- `tests/test_view.cpp` witnesses the format, the rules, registration and grant, publication,
  refusal, in-place and fresh apply, stop, and the terminal picture; `tests/test_view_builder.cpp`
  the one renderer, the palette, the drags, the boxes, the marks and the terminal floor; the
  panes suite the view and the builder through Workshop's real seat, pointer route, drag carry
  and canvas admission.
