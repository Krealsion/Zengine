# Persist

**The file doors every durable file shares: a whole file read within a bound, a file written
without putting the last good copy at risk, and what a write came to, in words a weaver can act
on.**

- **A bounded read.** A read names the most bytes the file may hold, and a larger file is refused by
  its size before a byte of it is read, so a file never chooses the cost of refusing it. A missing
  file, or one that cannot be opened, is an ordinary refusal.
- **A safe write.** A write goes to a candidate beside the file (`<file>.saving`) and replaces the
  file only once the candidate is whole. A write that fails part way, as when the disk fills,
  removes the candidate and is refused with words that say the file is unchanged, so the last good
  copy is still there to read.
- **Folders.** A per-user root, such as the folder the session is kept in, is made on its first
  write. A project's folder never is, so a project file into a missing folder is refused out loud.
- **What a write came to.** Every outcome is a `Written`: accepted, or refused with its reason. The
  reason is the point, so there is no bare `false`.

The folder also takes a path from the platform into the one way this application spells a path,
or refuses it in words, and reads plain JSON a weaver writes by hand.

A program includes the header it needs (`persist/file.hpp`, `persist/written.hpp`,
`persist/path_admission.hpp`, `persist/json.hpp`) and links `zengine::persist`.

## Parts

| file | what it holds |
|---|---|
| `file.hpp` | the bounded read (`read_file`), the safe write (`write_file`, `write_file_making_room`, `pending_path`), a relative spelling resolved (`resolved_against`), and `json::`, which reads a JSON object's members as text before a shape can judge them; installed |
| `written.hpp` | `Written`, what a write came to; installed |
| `path_admission.hpp` | `admit_path`, `admit_location`, `admit_filename` and `launch_project_dir`: a path or a file name from the platform taken into the application's one path vocabulary, or refused in words; installed |
| `json.hpp` | a plain JSON document read whole into a tree of nodes and written back (`json::parse`, `json::to_text`), bounded in depth and in values |
| `CMakeLists.txt` | `zengine-persist`, a header-only target aliased `zengine::persist` |

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`persist/` adds to it.

**Law.** The rules this folder keeps are Workshop's, in its registers under `agents/workshop/`, and
the comment on each declaration names the law it keeps in a sentence:

- the bounded read and the safe write, WL-DOC-15 ([the document file](../agents/workshop/document-file.md));
- a root made on its first write and a project folder never invented, WL-SESSION-18, and the one
  door that writes the session, WL-SESSION-13 ([the session](../agents/workshop/session.md));
- the path doors: `path_admission.hpp` alone asks for a path's bytes, WL-FILES-11; a spelling coming
  back in goes through `admit_location`, WL-FILES-09; a file name's bytes, WL-FILES-10
  ([Files](../agents/workshop/files.md));
- a relative spelling is the project's file, WL-EDIT-06 ([the editor](../agents/workshop/editor.md))
  and WL-PROJ-02, and the project is the launch directory, WL-PROJ-01
  ([the project](../agents/workshop/project.md)).

Those registers are routed by [Workshop's router](../workshop/AGENTS.md).

**Suites.** The folder has no CTest entry of its own. `workshop_persistence`
(`tests/test_workshop_persistence.cpp`) writes and reads every file Workshop keeps through it, and
`theme` (`tests/test_theme.cpp`) a theme's files. Each runs through the official lane,
`tests/verify.cmake`.

**The host's side.** Workshop's files (`workshop/*_persist.hpp`, the session's, the layouts'), the
editors' and Files' reads and writes, and the theme reader (`theme/theme_persist.hpp`) read and
write through it. `cmake/ZengineInstall.cmake` installs `file.hpp`, `written.hpp` and
`path_admission.hpp`.
