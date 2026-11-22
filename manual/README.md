# The manual

**A weave carries its manual: one Markdown page, named by the weave's role, with a section for
each command it takes. The build compiles the page into the weave, so a running weave's words are
always its own. A pane answers its manual when asked, and a role answers what a shape it accepts
does; nothing keeps a copy.**

The page lives in the weave's folder, as `docs/<role>.md`. A weave in this repository names it with
`zengine_weave(<target> <source> MANUAL <page>)`, and any other target with
`zengine_manual(<target> <PRIVATE|INTERFACE> <page>)`: the build splits the page at its headings,
refuses a page out of form with every reason and its line, and compiles the sections into the
weave with the page's content id. The page's form, and what keeps each section true to the
commands the weave takes, are [the reference](docs/manual.md).

It is a small header-only library and a CMake function. A program includes `manual/manual.hpp` for
the page as a weave carries it and `manual/vocabulary.hpp` for the door a role answers, and links
`zengine::manual`.

## Pages

- [A weave's manual](docs/manual.md): what a command is, the page's form and its key markers, how
  a weave carries its page, how a pane declares it and a role answers from it, and the checks
  that keep it true

## Parts

| file | what it holds |
|---|---|
| `manual.hpp` | `zengine::manual::Manual` and `Section`, the view of a page the build compiles into a weave (`manuals/<role>.hpp`), and the bounds a page is judged by; installed |
| `vocabulary.hpp` | the door a role answers for a shape it accepts: `ShapeDocumentRequested`, `ShapeDocumentShown`, `shape_document`, `content_id_text` and the facet `ManualFacet`; installed |
| `CMakeLists.txt` | `zengine-manual`, a header-only target over `loom::core`, aliased `zengine::manual` and exported; not a weave |

The grammar a page is split with is `cmake/ZengineManual.cmake`, which `zengine_weave(... MANUAL)`
and `zengine_manual()` call when the build is configured and `tests/check_manual.cmake` includes,
and which the package installs, so a stranger's weave carries its manual as this repository's do.

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`manual/` adds to it.

**Law.** The folder has no law page of its own: its contract is [the reference](docs/manual.md),
whose last section, "How a manual is kept true", names every check that holds a page and what each
refuses. Workshop keeps no pane's manual, so no Workshop register holds one.

**Suites.** The folder has no CTest entry of its own. Its cases stand in shared suites:
`workshop_panes_project` (`tests/test_workshop_panes_builder.cpp`: the action census, the Builder's
answered manual against its declared ids in every mode, a section or a pane it does not have
refused, a reload in place with other words) and `builder` (`tests/test_builder.cpp`: the role's
answer by content id, and its page against its accepted shapes). The page's form and its key
markers are the `manual` text check's, which the guide's "What crosses folders" claims. Each runs
through its lane, the official lane being `tests/verify.cmake`.

**The host's side.** `pane/document.hpp` is the pane protocol's side: `PaneDocumentRequested`,
`PaneDocumentShown` and `pane_document`, the answer a pane gives. Workshop holds none of it; it
grants the Builder role its answer in `workshop/workshop.cpp`. `cmake/ZengineInstall.cmake`
installs the headers and the grammar.

**Practice.** A manual is written from the code, and writing one is a review of the code: say what
the weave does now, and report where the code disagrees with its law rather than writing the law.
A command's refusal is quoted in the words the code says, and a key only in a key marker.
