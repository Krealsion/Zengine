# The manual

**A weave carries its manual: one Markdown page, named by the weave's office, with a section for
each command it takes. The build compiles the page into the weave, so a running weave's words are
always its own; a pane declares them to Workshop beside its offer, and an office answers what a
shape it accepts does when asked.**

It is a small header-only library and a CMake function. A weave in this repository names its page
with `zengine_weave(<target> <source> MANUAL <page>)`.

## Pages

- [A weave's manual](docs/manual.md): what a command is, the page's form and its key markers, how
  a weave carries its page, how a pane declares it and an office answers from it, and the checks
  that keep it true

## What is in this folder

| | |
|---|---|
| `manual.hpp` | a page as a weave carries it, `zengine::manual::Manual`, and the bounds a page is judged by |
| `vocabulary.hpp` | the door an office answers for a shape it accepts: `ShapeDocumentRequested`, `ShapeDocumentShown` |
| `CMakeLists.txt` | its build target, `zengine-manual` |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
