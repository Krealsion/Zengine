# `persist/` — Persist: working on it

The router for `persist/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `file.hpp` | `read_file`, `write_file`, `write_file_making_room`, `pending_path`, `resolved_against`, `json::` |
| `json.hpp` | plain JSON, read into a tree of nodes and written back: `json::parse`, `json::to_text`, bounded in depth and values |
| `written.hpp` | `Written` |
| `path_admission.hpp` | `admit_path`, `admit_location`, `admit_filename`, `launch_project_dir`: a filesystem path or name taken into the one path vocabulary, or refused in words |
| `CMakeLists.txt` | `zengine-persist`: INTERFACE |

## Law

No register of its own. The safe write and the bounded read are Workshop's document-file law,
`agents/workshop/document-file.md`, with the session's (`agents/workshop/session.md`); both are
routed by [workshop/AGENTS.md](../workshop/AGENTS.md).

## Suites

None of its own: `workshop_persistence` (`tests/test_workshop_persistence.cpp`) writes and reads
every file Workshop keeps through it, and `theme` (`tests/test_theme.cpp`) a theme's files.

## Host side

Workshop's files (`workshop/*_persist.hpp`, the session's, the layouts'), the Editors' and Files'
reads and writes, and the theme reader (`theme/theme_persist.hpp`) read and write through it.

## Pages

- [Persist](README.md), the front page

## Practices

None of its own yet.
