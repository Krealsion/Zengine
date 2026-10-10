# `manual/` — The manual: working on it

The router for `manual/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `manual.hpp` | `Manual` and `Section`, the view a generated `manuals/<office>.hpp` defines; the bounds |
| `vocabulary.hpp` | `ShapeDocumentRequested`, `ShapeDocumentShown`, `shape_document`, `content_id_text` |
| `CMakeLists.txt` | `zengine-manual`, INTERFACE, over `loom::core`; not a weave, not installed |

The grammar a page is split with is `cmake/ZengineManual.cmake`, which `zengine_weave(... MANUAL)`
and `zengine_manual()` call at configure time and `tests/check_manual.cmake` includes.

## Law

No register of its own under `agents/`. Its contract is its reference,
[a weave's manual](docs/manual.md). What Workshop holds of a pane's declaration is WL-DESK-06, in
the desktop's register, routed by [workshop/AGENTS.md](../workshop/AGENTS.md).

## Suites

No CTest entry of its own. Its cases in shared suites: `workshop_panes`
(`tests/test_workshop_panes_manual.cpp`: a declaration judged whole, refused aloud, counted while
its sender holds the office and dropped at a re-offer; `tests/test_workshop_panes_builder.cpp`: the
action census, the Builder's held manual against its declared ids in every mode, a reload in place
with other words) and `builder` (`tests/test_builder.cpp`: the office's answer by content id, and
its page against its accepted shapes). The page form and the key markers are the `manual` text
check's, routed by the root.

## Host side

`workshop/pane_document.hpp` is the pane protocol's side: `PaneDocumentDeclared`, its judgment and
`pane_document_of`. Workshop holds a declaration in `workshop/panes.hpp` (`RuntimePane::document`)
through `workshop/weave_seam.cpp` and `workshop/setup.hpp` `admit_pane_document`; it grants the
Builder office its answer in `workshop/workshop.cpp`.

## Pages

- [A weave's manual](docs/manual.md)

## Practices

- A manual is written from the code, and a manual written from the code is a review of it: say what
  the weave does now, and report where the code disagrees with its law rather than writing the law.
- A command's refusal is quoted in the words the code says; a key only in a key marker.
