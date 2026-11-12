# `pane/` — Pane: working on it

The router for `pane/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `vocabulary.hpp` | `PaneCatalogRequested`, `PaneOffered`, `PaneKey`, `PaneTextInput`, `PaneKeys`, `PaneActions`, `PaneActionRequested`, `PaneQuitRequested`, `PaneQuitAnswered` and the rest of the seam |
| `presenter_vocabulary.hpp` | `MenuGranted`, `MenuShown`, `MenuClosed`, `HeldMenu`: the presenter's seam |
| `operation.hpp`, `carry.hpp` | `PaneOperationRequested` and its answer, the act classes it names (`act_class::kBuild`, `act_class::kWrite`, `act_class::kOpen`); a carried envelope, bounded by `kMaxCarryBytes` |
| `settings.hpp`, `settings_taken.hpp` | `PaneSettingsDeclared`, `PaneSettings`, `PaneSetting`; `PaneSettingsFacet` |
| `document.hpp` | a pane's manual, asked of the pane |
| `menu.hpp`, `escape.hpp` | `pane_menu::Asked`, `pane_menu::Offer`; `pane_escape::answer` |
| `drawing.hpp`, `rows.hpp`, `rows_block.hpp` | `WindowDrawing`, `rows_drawing`, `RowsRoom`, `RowsBlock`, `pane_area` |
| `picture.hpp`, `canvas_drawing.hpp`, `canvas_text.hpp` | `CanvasPicture`, `CanvasRoom`, `canvas_sketch`, `CanvasText` |
| `text.hpp`, `text_fit.hpp`, `parts.hpp`, `causes.hpp` | `pane_text::`, text cut to a room, named parts, `ActionCause` |
| `facets.hpp`, `seat.hpp` | `PaneRowsFacet` and the facets, a pane seated on them |
| `CMakeLists.txt` | `zengine-pane-vocabulary`, exported as `zengine::pane`: INTERFACE |

## Law

- [pane-contract](../agents/pane-contract.md) — the pane contract (router)
- [seam](../agents/pane-contract/seam.md) `PANE-SEAM` — the offer, the area, the keys, a declared
  row's id, a menu's answer and the quit's answer

The prose of the seam's both halves and of the tools beyond it is `agents/panes.md`, routed by the
root's router; what Workshop does as the host is its law, routed by
[workshop/AGENTS.md](../workshop/AGENTS.md).

## Suites

None of its own: the seam's suite is `workshop_panes_seam` (`tests/test_workshop_panes_seam.cpp`),
and each pane's suite, `workshop_panes_<part>`, reads its half through these headers. The package's
stranger panes build against the installed headers (`tests/package/`).

## Host side

Workshop (`workshop/`) is one host of it: it offers the catalog, places each pane's area, sends the
keys and asks the quit. The app host and every pane include these headers as any participant does.

## Pages

- [Pane](README.md), the front page
- [Workshop's panes](../workshop/docs/workshop-panes.md), the protocol's reference

## Practices

None of its own yet.
