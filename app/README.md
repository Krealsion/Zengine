# Apps

**An app is a view and the flow that answers it, made in Workshop and run on its own: `zengine-app`
opens its window, draws its view there, takes its keys and presses, and runs its flow, with no
Workshop running.** What may run in it is decided at its own console, in Loom's `authority`
words, and kept in the app's folder.

## Pages

- [Running an app on its own](docs/app.md): an app's folder, `app.json`, starting it, its console,
  and what runs the view and the flow.

![The tally app in its own OS window, typed into and counting](docs/images/tally-app.png)

## What is in this folder

| | |
|---|---|
| `main.cpp` | `zengine-app`, the program: an app's folder, its console, its turns of the bus |
| `host.hpp` | the app host: what the program composes over its bus, and the console's words |
| `weave.cpp` | `zengine-app-weave`, the app's own weave: runs its flows and views, arranges its window |
| `files.hpp` | reading `app.json` and the app's files |
| `vocabulary.hpp` | the offices, the place door's asks, and `app.json`'s shapes |
| `CMakeLists.txt` | how the folder builds, and the artifacts staged beside the program |
| `docs/` | the page above and its picture |

An app written by hand: [`examples/tally-app/`](../examples/tally-app/README.md).

Working on it: [AGENTS.md](AGENTS.md).
