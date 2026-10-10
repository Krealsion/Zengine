# `message-draft/` — Typed value forms and presets: working on it

The router for `message-draft/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| Path | What it is |
|---|---|
| `CMakeLists.txt` | `zengine-message-draft` (`zengine::message-draft`): header-only, no weave |
| `draft.hpp` | `Draft`, `Path`: a Loom value edited against its schema, and its form rows |
| `library.hpp` | `Library` of named presets: `library_bytes`, `save_library`, `open_library` |
| `transfer.hpp` | a preset or one field as data: `store_draft`, `grab_field`, `require_type` |
| `docs/message-drafts.md` | the reference page |

## Law

- [message-drafts](../agents/message-drafts.md) — reusable typed drafts

## Suites

- `message_draft` — drafts, preset libraries, transfer envelopes (`tests/test_message_draft.cpp`)

Consumers' suites spend it too: `composer` (`tests/test_composer.cpp`), `flow`
(`tests/test_flow_graph.cpp`), `inventory` (`tests/test_inventory.cpp`), `source_transfer`
(`tests/test_source_transfer.cpp`), `view_builder` (`tests/test_view_builder.cpp`) and
`workshop_panes` (`tests/test_workshop_info_views.cpp`, `tests/test_workshop_inventory_info.cpp`).
The installed-package witness `tests/package/message_draft_consumer.cpp` is no CTest entry;
`tests/package/run.cmake` runs it.

## Host side

- Nothing in `workshop/` hosts it: no office, pane or seam vocabulary. Consumers include it:
  `composer/draft.hpp`, `flow/shape.hpp`, `flow/workspace.hpp`, `info/value_view.hpp`,
  `inventory/inventory-pane/command.hpp`, `editor/source-transfer/material.hpp`,
  `view/view-builder/pane.cpp`; `flow/flow-pane/CMakeLists.txt` links it.
- Files: `library.hpp` reads and writes through `maker/files.hpp`, without linking `zengine-maker`.
- `inventory/codec.hpp` follows the library's encoding, without its optional projection.
- `cmake/ZengineInstall.cmake` installs its headers under `include/zengine/message-draft/`.

## Pages

- [Reusable message and value drafts](docs/message-drafts.md)

## Practices

- [Compose](../docs/contributing/best-practices.md#compose-composer-message-draft)
