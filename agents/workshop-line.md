# Agent law — The line between Workshop and Zengine

Routed detail behind [`AGENTS.md`](../AGENTS.md), for a change that moves a file across the line,
includes a header of the other side, or edits `tests/census.txt`.

## Two sides in one repository

Workshop is built as a stranger to Zengine: one side of a line that runs through this repository,
held so that Workshop could leave it and build against the installed package alone, as Zengine
builds against Loom's. **Workshop's side** is `workshop/` less the menu presenter
(`workshop/menu-presenter/`), `builder/`, `launcher/` and `external-host/`. **Zengine's** is every
library, every other pane, the menu presenter and the app host. A header the package installs
(`cmake/ZengineInstall.cmake`) is the package's whichever folder holds it, so the words both
sides speak are Zengine's even where they stand in a Workshop folder. The suites under `tests/`
straddle by design and stand on neither side.

## The census holds it

`tests/census.txt` declares the sides, a `side` row a folder, the longest that holds a file
deciding, and pins every reach across the line, a `line` row a file and a header:

- a file of Zengine's side, in a code folder of `tests/folders.txt`, including a header of
  Workshop's side;
- a file of Workshop's side including a header of Zengine's side that the package does not
  install.

`census` (`tests/check_census.cmake`, [VM-CHECK-17](verification/checks.md)) is red when the tree
holds a reach no row pins, naming the file and line, or a row the tree no longer holds. Every
`line` row is marked to only fall: a change that adds a reach, or raises a row, is red against the
base the change is measured from, so the line moves one way. A change that closes a reach removes
its row in the same change, and a file that moves across the line takes its rows with it as a
visible edit.

## What it does not see

A link line or a target named across the line, a source-tree path in Workshop's CMake, and a test
that builds both sides: the census reads includes alone. Zengine configured and built with none
of Workshop's folders beside it is the line's second witness, and does not run yet.
