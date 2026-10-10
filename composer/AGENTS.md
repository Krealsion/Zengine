# `composer/` — Compose: working on it

The router for `composer/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| Path | What it is |
|---|---|
| `CMakeLists.txt` | its three targets; the weave is skipped where Loom exports no `loom::kernel` |
| `vocabulary.hpp` | office, pane and stem names; `zengine-composer-vocabulary`, links nothing |
| `draft.hpp` | the draft model and what a draft composes to; `zengine-composer-draft`, no bus |
| `view.hpp` | what the pane shows and what each row means; also in `zengine-composer-draft` |
| `command_context.hpp` | `ComposerCommandContext` v1: the target office stored with a command |
| `composer.cpp` | the Compose pane, `zengine-composer`: a loadable weave |

## Law

Its law is the section "The Composer is a schema-directed message form" of the pane seam's law,
`agents/panes.md`, which the root routes. Its drafts' law is `agents/message-drafts.md`, which
`message-draft/` routes.

## Suites

- `composer` — drafts and views as values: what composes and shows (`tests/test_composer.cpp`)

Its cases in shared suites: `workshop_panes` (`tests/test_workshop_panes_input.cpp`,
`tests/test_workshop_inventory_info.cpp`), `workshop_document`
(`tests/test_workshop_document.cpp`), `workshop_shapes` (`tests/census_headers.hpp`) and
`workshop_files` (`tests/test_workshop_files.cpp`, its development catalog row).

## Host side

- Boot: the optional `zengine.composer` row in `workshop/default-load-plan.json` and
  `workshop/graphical-load-plan.json`; `workshop/CMakeLists.txt` stages it and lists it among
  the development setup's pane weaves.
- Seam: no seam vocabulary of its own; the pane protocol `workshop/pane_vocabulary.hpp` and the
  pane parts it includes (`workshop/pane_parts.hpp`, `pane_menu.hpp`, `pane_carry.hpp`, ...).
- Submit: `PaneOperationRequested` (`workshop/pane_operation.hpp`), judged for the gesture's actor
  in `workshop/weave_operation.cpp` with `workshop/actor_scope.hpp`.
- Grants: `workshop/guests.cpp` lets the `demo` power reset its pane.
- Owners it asks: the target office (`zen.DescribeAccepted`), Introspection (`LoadedSelected`,
  `introspection/vocabulary.hpp`), the Skin's clipboard and Inventory (`inventory/pane_client.hpp`).

## Pages

- [Compose](README.md) — this folder's front page
- [Reuse a stored command through Compose](../inventory/docs/inventory-compose.md)
- [Panes](../workshop/docs/panes.md) — where Compose sits among Workshop's panes
- [Reusable message and value drafts](../message-draft/docs/message-drafts.md)

## Practices

- [Compose](../docs/contributing/best-practices.md#compose-composer-message-draft)
