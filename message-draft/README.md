# Message drafts

**A small C++ library for a typed message that is still being written. A program fills its fields
one at a time, may leave it unfinished, keeps it as a named preset in a file, and checks it against
the message's declared shape before using it.**

A refused edit leaves the draft as it was, and an unset field, `false` and empty text stay three
different things. A preset, or one field picked out of a draft, can also be carried as an ordinary
value. A draft never sends anything and records no destination: sending is a separate act, and
reusing a value never reuses authority.

It is header-only. A program links `zengine::message-draft` and writes
`#include "message-draft/draft.hpp"`, `#include "message-draft/library.hpp"` or
`#include "message-draft/transfer.hpp"`.

## Pages

- [Reusable message and value drafts](docs/message-drafts.md): the reference; editing a draft,
  saving and reopening presets, and carrying a preset or a field as data

## Parts

| file | what it holds |
|---|---|
| `draft.hpp` | `Draft`, a Loom value edited against its schema, and the form rows it shows; `Path`, a field's place as names and indices |
| `library.hpp` | `Library`, named presets, saved to bytes or a file and opened again: `library_bytes`, `read_library`, `save_library`, `open_library` |
| `transfer.hpp` | a preset or a single field wrapped as a complete value: `store_draft`, `grab_field`, `require_type` |
| `CMakeLists.txt` | `zengine-message-draft` (`zengine::message-draft`), header-only, no weave |
| `docs/message-drafts.md` | the reference page |

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`message-draft/` adds to it.

**Law.** The folder has no numbered law: its promises are the reference's, and a rule about one
declaration is in that declaration's comment. Two things the library must never become:

- **A second parser or type system.** A scalar's text is composed by Loom's own lexer and composer,
  as the Composer composes it, and Bytes have no text spelling here.
- **A runtime contract.** The projection that lets a saved preset lack a required field relaxes
  presence only, inside the library's codec. It is never registered or published as a message's
  schema, and the original schema stays the one a draft is admitted against, across a save and a
  reopen.

**Suites.** Each runs through the official lane, `tests/verify.cmake`.

| suite | what it proves | where |
|---|---|---|
| `message_draft` | drafts, preset libraries and transfer envelopes | `tests/test_message_draft.cpp` |

Its users' suites spend it too: Compose's (`tests/test_composer.cpp`), Flow's
(`tests/test_flow_graph.cpp`), Inventory's (`tests/test_inventory.cpp`), the source transfer's
(`tests/test_source_transfer.cpp`), the View Builder's (`tests/test_view_builder.cpp`) and the info
views' in Workshop (`tests/test_workshop_info_views.cpp`, `tests/test_workshop_inventory_info.cpp`).
The installed-package witness `tests/package/message_draft_consumer.cpp` is no CTest entry:
`tests/package/run.cmake` runs it.

**The host's side.**

- Nothing in `workshop/` hosts it: it has no role, pane or seam vocabulary. Its users include it:
  `composer/draft.hpp`, `flow/shape.hpp`, `flow/workspace.hpp`, `info/value_view.hpp`,
  `inventory/inventory-pane/command.hpp`, `editor/source-transfer/material.hpp` and
  `view/view-builder/pane.cpp`; `flow/flow-pane/CMakeLists.txt` links it.
- `library.hpp` reads and writes through `maker/files.hpp`, without linking `zengine-maker`, and
  keeps its writer's rules: one writer for a path, the 1 MiB limit, and the whole candidate
  checked before the old file is replaced.
- `inventory/codec.hpp` follows the library's encoding, without its optional projection.
- `cmake/ZengineInstall.cmake` installs its headers under `include/zengine/message-draft/`.

**Practice.** [Compose](../docs/contributing/best-practices.md#compose-composer-message-draft), in
the best practices.
