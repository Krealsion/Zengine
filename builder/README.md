# The Builder

**The Builder builds a weave from its source and puts the result to work. A weaver (a person who
writes and runs weaves) chooses a recipe in Workshop's Builder pane and presses a key: the build
runs while Workshop keeps working, its output can be read in the pane, and the result can be
loaded into the running program, or swapped into a weave that is already running, which keeps
its state.**

A recipe says what to build and which file it makes, never which program to run: every build is
one run of CMake. The Builder is part of Workshop; nothing here is installed for other programs.

## Pages

- [Workshop's Builder](docs/builder.md): what the Builder pane does today: its keys, the recipe
  catalog, load after build, promote and revert
- [Edit a running pane](docs/edit-a-running-pane.md): a walkthrough: open a running pane's code,
  change it, rebuild, and watch the same pane change
- [The Builder package](docs/builder-reference.md): the reference: what a recipe is, how a build
  is started and followed, and how success is judged

## What is in this folder

| | |
|---|---|
| `builder-pane/` | the Builder pane, where a weaver chooses, builds and loads |
| `recipe.hpp` | what a recipe is: a name, the file it makes, and how CMake makes it |
| `generate.hpp` | a recipe turned into one CMake run, and the small project one source file gets |
| `run.hpp` | starting a build process and looking at it without waiting on it |
| `runner.hpp` | the runner, the one part that starts builds |
| `weave.hpp` | the Builder tool: which recipes there are, and how the current build is going |
| `vocabulary.hpp` | the messages of a build: what is asked, what it said, and how it ended |
| `CMakeLists.txt` | the build |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
