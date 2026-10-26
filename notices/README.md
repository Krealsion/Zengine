# Notices

**What Workshop said, kept for the run and shown in a pane: each saved file, each refusal, each
thing a command of yours did, the newest at the bottom.** Workshop says each thing once as it
happens; the notices record keeps the last hundred (a setting keeps more), and the Notices pane,
on a fresh desk at the room's bottom left, shows them. Turn the wheel over it, or press into it and
use `up` and `down`, to read older ones. Nothing here waits for you: a notice is a moment that
passed, and what is true now is [Attention](../attention/README.md)'s.

## Pages

- [`zengine.notices-pane`](docs/zengine.notices-pane.md): the pane's manual -- what it shows and
  its keys
- [`zengine.notices`](docs/zengine.notices.md): the record's manual -- what it keeps, for how long,
  its `keep` setting and the page a weave asks for

## What is in this folder

| | |
|---|---|
| `notices.cpp` | the record, built as a weave Workshop loads (`zengine-notices`) |
| `vocabulary.hpp` | the record's name (`zengine.notices`), its setting, its bounds and its messages |
| `notices-pane/` | the pane, built as a weave Workshop loads (`zengine-notices-pane`), and its names |
| `CMakeLists.txt` | how the record is built |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
