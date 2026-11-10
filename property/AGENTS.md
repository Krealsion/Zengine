# `property/` — Property: working on it

The router for `property/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `property.hpp` | `Property`, `TextForm`, `Commit`, `Row` |
| `CMakeLists.txt` | `zengine-property`: INTERFACE, over `zengine-persist` |

## Law

No register of its own. A property, its text form and an inspector's row are Workshop's document
law, `agents/workshop/document.md` (WL-DOC-02), with the Pane Manager's section rows
(`agents/workshop/pane-manager.md`); both are routed by [workshop/AGENTS.md](../workshop/AGENTS.md).

## Suites

None of its own: `workshop_document` (`tests/test_workshop_document.cpp`) edits rows through it, and
the suites of Workshop's files, the editors, Files and Info read and write their properties through
it.

## Host side

Workshop's files (`workshop/*_persist.hpp`, the session's, the layouts'), the editors' panes, Files
and Info's settings rows hold their properties through it.

## Pages

- [Property](README.md), the front page

## Practices

None of its own yet.
