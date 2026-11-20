# Described views

Routed behind [AGENTS.md](../AGENTS.md) for `view/` and `view/view-builder/`. Also read
[panes](panes.md) (the canvas and the carry), [flow](flow.md) (the one shape model) and
[flow host](flow-host.md) (the sibling host). Public contract:
[view reference](../view/docs/view.md); weaver guide:
[the View Builder](../view/docs/view-builder.md).

- A view description is presentation as data: its size, elements inside it, the field a label
  shows, the intent a button says. It holds no business value, no resolved geometry and no
  position in cells or in a fraction of a pixel. Places and sizes are whole pixels,
  `graphics::kCellPx` to a cell. No element sits outside the view's size, and its notice
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
- `view::Host` registers each view as its own participant, holding the view's name as its role,
  granted `view::view_grant` alone: its intents and its wiring to any accepter, the pane conversation to
  Workshop's role, its drawing to any window (a window takes an area's drawing only from the
  area's owner) and the ask for its room to the graphics directory. Never `trust_every_artifact`,
  an allow-any, or the builder's grant.
- Loom fixes a participant's accept-set and emit-set at registration. A change at the same name
  and shapes (`view::same_shapes`) is adopted in place, keeping field text, focus and what the
  view was told; any other change is a fresh registration, and the answer says so.
- A successor registers while the view it replaces still runs, so a registration Loom refuses
  (`loom::SchemaConflict`, a held name) leaves that view and its session as they were, and the
  answer says why.
- `view::View` keeps only presentation: field text, focus, caret, its notice, and the latest value
  of each shape it was told. It calls nothing current that it was not told: a label it was never
  told says `waiting`. An intent is published as the participant, by shape: whoever accepts it
  hears it, and the view claims no exclusive audience. A refusal is shown only when Loom attests
  it answers the view's own delivery (`answers_ask()`); the correlation then says which intent.
- A view says what its wiring has done (`view::ViewWiring`, `view::wiring_of`) as its role, by
  publication: when it starts and whenever a row changes, never when none did; and, to an asker of
  `view::ViewWiringRequested` alone, as it is then.
  It is the view's own
  word; nothing reads the weaver's files or the picture for it.
- ONE RENDERER: `view::picture` is the only drawing of a view.
  Over it the builder draws marks alone, never an element.
- A window's press uses a button only when the
  picture last sent there names it.
- `view::Presenter` is an owner's side of a view in the view's own process: what it tells or `set`s
  is taken as told, and its code for a button runs before the button's intent, a throw said on the
  notice row and nothing published.
  It holds code, so it never crosses a participant boundary: a loadable weave
  reaches a view by its shapes and intents, or keeps a presenter of its own description and draws
  it into its own `graphics::Canvas` (`Presenter::sketch`, `Presenter::act`).
- A view takes `AreaKeyPressed` and `AreaTextEntered` naming its area, from the role of the window
  that shows it, which gives keys only to an area placed to hear them, onto the handlers `PaneKey`
  and `PaneTextInput` reach; from any other role they change nothing.
- A Zengine host composes the host beside the Flow host and holds no view behaviour: Workshop,
  and an app's own host ([app](app.md)), where the app's own weave asks it. The View Builder
  is an ordinary pane in `zengine.view.builder`: it edits a description through
  `view_builder::Model`, every edit whole or refused whole, and asks the host to run, apply and
  stop one session of its role.
- A gesture, the pan, the grid, a box being typed into and a mark are the builder's
  presentation: never saved, kept across a reload or offered to the host; no pan, grid or snap is
  in a description. A reload in place keeps the draft, the name in its File box and, apart from
  it, the file the view was saved to or opened from (`view_builder::ViewBuilderState::file`), and
  whether its view ran, which the successor asks the view host again by `view::ViewResume`:
  refused because this role's session runs a view, it runs; resumed, it runs again. So does a
  relaunch: the builder writes that file (inside the project,
  named relative to it, so a project moved whole still finds it) and whether its view runs to the
  project's `view-builder.json` (`view_builder::ViewBuilderRun`) when either differs from what the
  file holds, and counts it held only once the write succeeds, so a write that failed is made at
  the next chance, a successor's first included; that record is the builder's own keeping,
  written whatever hand acts, while a hand's Save asks Workshop for class `write` under its
  gesture and writes nothing until allowed.
  A launch that
  cannot open that view file keeps the project file as it found it
  (`view_builder::ViewBuilderState::kept`, across a reload too) until the weaver opens, saves or
  starts a view, a name typed into File being none of these, so the file's return brings its
  view back at the next launch.
- Export app makes an app of the saved view through `app/folder.hpp` and nothing of its own
  ([app host](app.md)): an unsaved view or one with no file is refused before anything is asked; a
  hand's export asks Workshop for class `write` under its gesture, as Save does, one at a time; an
  app folder already there waits for a second press; and on Workshop's yes the export is judged
  again, on the view's file as it is then, before anything is written.
- `tests/test_view.cpp` witnesses the format, the rules, registration and grant, publication,
  refusal, the wiring said and asked, in-place and fresh apply, a replacement that cannot register, stop, the terminal
  picture, the picture in a window, its changes, refusals and presses, and the presenter, against
  a window that admits by the window's own judge (`graphics/window/store.hpp`);
  `tests/test_view_builder.cpp` the one renderer, the palette, the drags, the snap,
  the side handles, the pan, the size, nothing outside it, the grid, the boxes, the marks and the
  terminal floor, the builder in a graphics window, the run again at a launch, the launch
  record across a reload, a failed write and a missing view file, Discard, and Export app: the folder, the
  probe's plan byte for byte, Workshop's refusal, the second press and each refusal; `tests/test_view.cpp` also the
  size, a version 1 description, a view asking for its size,
  and a resumed view; the panes suite the view and the builder through Workshop's real seat,
  pointer route, drag carry and canvas admission.
