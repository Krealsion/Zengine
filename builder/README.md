# The Builder

**The Builder builds a weave from its source and puts the result to work. A weaver (a person who
writes and runs weaves) chooses a recipe in Workshop's Builder pane and presses a key: the build
runs while Workshop keeps working, its output can be read in the pane, and the result can be
loaded into the running program, or swapped into a weave that is already running, which keeps
its state.**

A recipe says what to build and which file it makes, never which program to run: every build is
one run of CMake. The Builder is part of Workshop; nothing here is installed for other programs.

## Tutorials

1. [Build your first pane](docs/tutorials/build-your-first-pane/README.md): build a small pane from
   one source file, put it into your project, and open it
2. [Edit a running pane](docs/edit-a-running-pane.md): open a running pane's code, change it,
   rebuild, and watch the same pane change

## Manuals

- [The Builder pane](docs/zengine.builder-pane.md), `zengine.builder-pane`: every command, by its
  key: choosing, building, loading, promoting and reverting, reading what a build said
- [The Builder](docs/zengine.builder.md), `zengine.builder`: the office that builds, and the
  messages another weave sends it

## Pages

- [The recipe file](docs/recipes.md): what a recipe holds, where the recipe file comes from, where
  a build's file lands and how a build is judged

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
| `docs/` | these pages; each manual is compiled into the weave it describes |

Working on it: [AGENTS.md](AGENTS.md).
