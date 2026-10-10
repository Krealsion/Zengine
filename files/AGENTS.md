# `files/` — Files: working on it

The router for `files/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

One part, the folder itself; the pure half keeps `namespace zengine::workshop`.

| path | what it is |
|---|---|
| `files.cpp` | the loadable weave `zengine-files`: office `zengine.files`, pane `project-files` |
| `os.cpp` | the two platform bodies: static library `zengine-files-os`, shared by weave and suite |
| `vocabulary.hpp` | the durable names: office, pane key, action ids, `FilesState` |
| `files.hpp` | the listing: `FileRow`, its order and bound |
| `marks.hpp` | the marks and the lexical parent of a location |
| `marks_persist.hpp` | the marks file |
| `filesystem_roots.hpp` | the filesystem roots, asked of the operating system |
| `CMakeLists.txt` | both targets; the weave only where the Loom can host loadable weaves |
| `docs/` | the how-to page and its image |

## Law

- [files](../agents/workshop/files.md) `WL-FILES` — the browser over the machine, paths, marks and
  roots; witnessed by `files_weave` (values), `workshop_panes` (files) and `workshop_files`

## Suites

- `files_weave` — the pure half in its own image: listing order and bound, name admission, marks
  and their file, roots, catalog refusals, and that the image links no host target (it reads
  `files/CMakeLists.txt` and `workshop/CMakeLists.txt`); `tests/test_files.cpp`

Its cases in shared suites: `workshop_panes` (`tests/test_workshop_panes_files.cpp`, the pane
through the real `zengine-files` image) and `workshop_files` (`tests/test_workshop_files.cpp`,
the doors it asks). Its marks file's shapes are counted by `workshop_shapes`
(`tests/census_headers.hpp`).

## Host side

- Loaded by a row in `workshop/default-load-plan.json` and `workshop/graphical-load-plan.json`;
  staged beside the host, and listed as a development pane weave, by `workshop/CMakeLists.txt`.
- Seam vocabulary: `workshop/pane_seam_vocabulary.hpp` (its asks, the marks file's name) and
  `workshop/open_seam_vocabulary.hpp` (`zengine.opening`, where an open goes).
- Doors: `ProjectDoor` (`zengine.project`) and `RecipesDoor` (`zengine.recipes`) in
  `workshop/pane_doors.hpp`, mounted in `workshop/workshop.cpp`, which resolves the marks path;
  the open is answered in `workshop/weave_opening.cpp`.
- The pure half includes `workshop/path_admission.hpp` and `workshop/persist.hpp`.
- A desk naming `zengine.workshop/project-files` is converted by `workshop/pane_migration.hpp`.

## Pages

- [Files](docs/files.md)

## Practices

- [Files](../docs/contributing/best-practices.md#files-files)
