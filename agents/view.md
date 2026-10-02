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
  participant behind that delivery, so Workshop keeps a picture that says the view stopped and no
  control that looks live.
- `view::View` keeps only presentation: field text, focus, caret, its notice, and the latest value
  of each shape it was told. It calls nothing current that it was not told: a label it was never
  told says `waiting`. An intent is published as the participant, by shape: whoever accepts it
  hears it, and the view claims no exclusive audience. A refusal is shown when it answers one of
  the view's own intents, by correlation.
- `view::picture` is pure: the description, what was told and the presentation, in one room. Text
  sits on the medium's lattice inside its element and is fitted by `clip_canvas_text`, so a
  terminal shows the window's picture floored to cells. A prose room gets one row saying the view
  needs a canvas; there is no second, text-row renderer.
- Workshop composes the host beside the Flow host and holds no view behaviour. The View Builder
  is an ordinary pane in `zengine.view.builder`: it edits a description through
  `view_builder::Model`, every edit whole or refused whole, asks the host to run, apply and stop
  one session of its office, and draws its own lists, never the view. An intent is made through
  `flow/shape.hpp`. A reload in place keeps its draft, file and whether its view runs; a dialog
  closes. It carries an intent's shape out as a `zen.SchemaDesc` by a menu choice, and takes a
  shape, a value or an Info field dropped on a label's row as what that label shows.
- `tests/test_view.cpp` witnesses the format, the rules, registration and grant, publication,
  refusal, in-place and fresh apply, stop, and the terminal picture; the panes suite witnesses
  the view through Workshop's real seat, pointer route and canvas admission.
