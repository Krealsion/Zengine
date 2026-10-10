# Message drafts

**A small C++ library for a typed message that is still being written. A program fills its fields
one at a time, may leave it unfinished, keeps it as a named preset in a file, and checks it against
the message's declared shape before using it.**

A refused edit leaves the draft as it was, and an unset field, `false` and empty text stay three
different things. A preset, or one field picked out of a draft, can also be carried as an ordinary
value. A draft never sends anything and records no destination: sending is a separate act.

It is header-only. A program links `zengine::message-draft` and writes
`#include "message-draft/draft.hpp"`, `#include "message-draft/library.hpp"` or
`#include "message-draft/transfer.hpp"`.

## Pages

- [Reusable message and value drafts](docs/message-drafts.md): the reference; editing a draft,
  saving and reopening presets, and carrying a preset or a field as data

## What is in this folder

| | |
|---|---|
| `draft.hpp` | a draft and the form rows it shows: its fields, paths and what is missing |
| `library.hpp` | named presets, saved to bytes or a file and opened again |
| `transfer.hpp` | a preset or a single field wrapped as a complete value |
| `CMakeLists.txt` | its build target, `zengine::message-draft` |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
