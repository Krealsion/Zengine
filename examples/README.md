# Examples

**Small, complete programs made of weaves, to read, run and change. Each shows one thing a weave
can be: a game whose parts are swapped while it plays, a pane in Workshop, a replacement for a
part Workshop ships, or a weave that drives a running Workshop from another host.**

None of them is part of the installed package. Tally, Guard, the numbered presenter and the tower
defense game are each one source file that builds against an installed Zengine and Loom, as
your own weave would.

## Pages

- [Snake](snake/README.md): the separate weaves the game is made of, and the keys that play it
  and swap them while it runs
- [Tower Defense, made from inside Workshop](tower-defense/README.md): play the game, build it
  into your own project, and replay how it was made

## What is in this folder

| | |
|---|---|
| [`snake/`](snake/README.md) | a snake game whose world, score, controls and clock are separate weaves |
| [`tally-pane/`](tally-pane/tally.cpp) | a Workshop pane with one count that survives a reload of its code ([walkthrough](../builder/docs/edit-a-running-pane.md)) |
| [`guard-pane/`](guard-pane/guard.cpp) | a Workshop pane that blocks while the right mouse button is held on it ([walkthrough](../workshop/docs/panes.md#the-second-button--the-panes-first)) |
| [`numbered-presenter/`](numbered-presenter/presenter.cpp) | another way to show a pane's menu: numbered rows, a digit to choose ([walkthrough](../workshop/docs/panes.md#replacing-the-menu-presenter)) |
| [`tower-defense/`](tower-defense/README.md) | a tower defense game in a Workshop pane, and a script that replays how it was made |
| [`workshop-probe/`](workshop-probe/probe.hpp) | a weave another Loom host loads to press a key in a running Workshop and save a picture of it ([guide](../external-host/docs/external-host.md#4-link-a-host-to-it)) |
| [`workshop-recipe-fixture/`](workshop-recipe-fixture/CMakeLists.txt) | a tiny project to write a build recipe for ([guide](../external-host/docs/elh-recipe-journey.md)) |

Working on it: [AGENTS.md](AGENTS.md).
