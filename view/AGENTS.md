# `view/` — Described views and the View Builder: working on it

The router for `view/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `CMakeLists.txt` | `zengine-view`: a header-only INTERFACE target, internal, nothing installed |
| `description.hpp` | `view::Description`, its saved bytes, and `view::problem`, the rules |
| `view.hpp` | the view renderer, `view::View`: one running view on its pane's canvas |
| `host.hpp` | the view host, `view::Host`: each view its own participant |
| `vocabulary.hpp` | the view host's office and asks: `ViewRun`, `ViewApply`, `ViewStop`, ... |
| `view-builder/` | the View Builder: `zengine-view-builder`, a loadable weave |
| `view-builder/pane.cpp` | the weave: its pane, its asks of the view host, its project file |
| `view-builder/model.hpp` | the semantic edits over one description, each whole or refused |
| `view-builder/picture.hpp` | its picture: bar, kinds, element list, boxes, design canvas |
| `view-builder/vocabulary.hpp` | its office, `ViewEdit`, reload state, `view-builder.json` |
| `docs/` | the reference and the weaver's guide, with their images |

## Law

- [view](../agents/view.md) — described views

## Suites

- `view` — the description's format and rules, the view host, the renderer: `tests/test_view.cpp`
- `view_builder` — edits, canvas, carries, run, files, the weave: `tests/test_view_builder.cpp`

Its cases in shared suites: `workshop_panes` (`tests/test_workshop_panes_canvas.cpp`,
`tests/test_workshop_panes_actions.cpp`, `tests/test_workshop_inventory_info.cpp`,
`tests/test_workshop_panes_guests.cpp`) and `workshop_desk` (`tests/test_workshop_desk.cpp`).
The builder is also named in `workshop_load` (`tests/test_workshop_load.cpp`), `workshop_files`
(`tests/test_workshop_files.cpp`) and `workshop_shapes` (`tests/test_workshop_shapes.cpp`).

## Host side

- Workshop composes and mounts `view::Host` beside the Flow host: `workshop/workshop.cpp`.
- Workshop links `zengine-view` and stages `zengine-view-builder`: `workshop/CMakeLists.txt`.
- The builder's optional plan row: `workshop/default-load-plan.json`,
  `workshop/graphical-load-plan.json`.
- The Pane Manager's `n` shows the View Builder: `workshop/desktop-pane/pane.cpp`.
- The builder's seam: `workshop/pane_seam_vocabulary.hpp`, `workshop/pane_carry.hpp`,
  `workshop/pane_menu.hpp`, `workshop/pane_operation.hpp`; it asks `zengine.project` where its
  file lives, answered in `workshop/pane_doors.hpp`.

## Pages

- [Make a small panel by hand in the View Builder](docs/view-builder.md)
- [Described views](docs/view.md)

## Practices

- [The View Builder](../docs/contributing/best-practices.md#the-view-builder-viewview-builder-view)
