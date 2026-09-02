# Workshop law — files

Register `WL-FILES`: the browser over the machine, paths, marks and roots. One law per heading;
cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-FILES-01 — The browser owns no source truth

LAW — The browser's machinery is its own file and its state lives on the panels; a row is a name, a kind, a linked flag and an openable flag and nothing else, and what it denotes is derived at activation.

MEANS
- no resolved path, recipe, artifact, build or editor state rides a row;
- the browser never judges contents — the editor's door does; none of it reaches a maker's file.

PROVEN BY — `workshop/files.hpp` `FilesPane`, `current_dir`; `workshop/panel.hpp` `files`;
`tests/test_workshop_files.cpp` case `"EDIT-1: opening a row hands the path to the ONE editor
door"`, case `"EDIT-1: the browser does not judge contents -- the editor's door does"`, case
`"EDIT-1: nothing about the browser is written to a maker's files"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-02 — Four facts coincide at launch and are not the same fact

LAW — The project is what a project-relative spelling means; the origin is where this run began; the location is where somebody is looking; the operating system is what may be read, never modelled.

MEANS
- the project has one writer, `main`; browsing, marking and a foreign catalog never move it;
- origin is generated once, never persisted, never moved, and not renamed "the project";
- the location is one absolute, lexically normal, generic-slash string, and it is not persisted.

PROVEN BY — `workshop/weave.hpp` `project_dir`; `workshop/marks.hpp` `LocationMarks`, `origin`;
`workshop/files.hpp` `current_dir`; `tests/test_workshop_files.cpp` case `"PROJ-2: origin is
generated for the run, is a mark, and is not the project"`, case `"PROJ-2: marks survive a
restart, and the browsing location deliberately does not"`, case `"PROJ-2: a location is one
absolute spelling, admitted the same way every time"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-03 — Parent is lexical and stops at the filesystem, not at the project

LAW — `parent_path()` until the fixed point `p.parent_path() == p`, which POSIX `/`, a drive root and `//server/` all answer; `has_parent_path()` is not a root test, and nothing canonicalizes.

MEANS
- going up from a linked directory returns the maker to where they walked in.

PROVEN BY — `workshop/files.hpp` `parent_path`, `has_parent_path`;
`tests/test_workshop_files.cpp` case `"PROJ-2: parent is lexical and stops where a path stops,
not where a project does"`, case `"PROJ-2: parent walks straight past the project and stops at
the filesystem"`, case `"EDIT-1: entering a directory walks in, and parent walks back to where
you were"`.
WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-04 — A linked directory is marked and enterable

LAW — A directory row is `linked` when following it says directory and not following it does not (`symlink_status()`), never `is_symlink()`, which a Windows junction answers false.

PROVEN BY — `workshop/files.hpp` `symlink_status`; `tests/test_workshop_files.cpp` case
`"PROJ-2: a linked directory is marked, entered, and left again LEXICALLY"`.
WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-05 — A location mark is a destination and nothing else

LAW — `Session::marks` is the one owner, outside `FilesPane`, with three provenances as flags — origin, maker marks, host roots — and a mark confers no authority, membership, trust or recipe base.

MEANS
- Files is the first consumer, not the owner: a fact inside a pane is `close_panel`'s to destroy.

PROVEN BY — `workshop/marks.hpp` `LocationMarks`; `workshop/screen.hpp` `marks`;
`tests/test_workshop_files.cpp` case `"PROJ-2: the marks owner is session truth, and Files is
only its first reader"`, case `"PROJ-2: one address is one traversal stop, however many ways it
is known"`, case `"PROJ-2: a maker marks a place, leaves, and comes back to it"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-06 — The traversal set is built at the gesture and held nowhere

LAW — Origin, the maker's marks sorted bytewise, then `host_filesystem_roots()` asked fresh; one address is one stop, and there is no standing selected mark — the cycle starts where the browser is.

PROVEN BY — `workshop/marks.hpp` `somewhere_to_go`; `workshop/filesystem_roots.hpp`
`host_filesystem_roots`; `workshop/weave.hpp` `host_filesystem_roots`;
`tests/test_workshop_files.cpp` case `"PROJ-2: traversal is cyclic, has no standing selection,
and both directions work"`, case `"PROJ-2: the host's filesystem roots are asked for, never
invented"`, case `"PROJ-2: a run with no origin can still reach a place it remembers"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-07 — Roots are host-reported, asked at the gesture, and never on the paint path

LAW — One header is the only place this repository asks an operating system for its roots, and the keyboard-readiness test may not ask for them: its whole test is in memory.

MEANS
- the test is `listing.known || !current_dir.empty() || marks.somewhere_to_go()`, all in memory;
- host-reported roots are never "every reachable path": a UNC share is in no drive list;
- the residual — no origin and no marks declines the keyboard — is named, not solved.

PROVEN BY — `workshop/filesystem_roots.hpp` `host_filesystem_roots`, `GetLogicalDrives`;
`workshop/screen.hpp` `files_has_keyboard`; `workshop/marks.hpp` `somewhere_to_go`;
`tests/test_workshop_files.cpp` case `"PROJ-2: the host's filesystem roots are asked for, never
invented"`, case `"EDIT-1: with no origin the pane refuses in words and guesses nothing"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-08 — Maker marks are durable, ride the machine-local root, and refuse by row

LAW — The marks file is its own format, version 1: the file's claims refuse it whole, an uncarriable row is skipped as a standing condition, and a refused file guards the first mark from overwriting it.

MEANS
- without the flag the first `m` would replace bytes this run could not read with an empty list;
- "unusable" is a spelling test, never an existence test: a marked directory that is gone is kept;
- a mark is an absolute path, so it describes this machine's disks — state, not configuration.

PROVEN BY — `workshop/marks_persist.hpp` `kFormatVersion`, `from_text`; `workshop/weave.hpp`
`marks_refused_`, `load_marks`; `tests/test_workshop_files.cpp` case `"PROJ-2: a persisted mark
is admitted, never re-based, and never quietly dropped"`, case `"PROJ-2: a marks file this run
could not read is never overwritten"`, case `"PROJ-2: marks survive a restart, and the browsing
location deliberately does not"`.
WHY — `agents/decisions/the-marks-file-is-state.md`

## WL-FILES-09 — A durable spelling coming back in is a conversion too

LAW — Every write to `current_dir` and every persisted mark goes through `admit_location`, so "absolute, lexically normal, carriable" holds after the seed, an enter, a parent and a jump.

PROVEN BY — `workshop/path_admission.hpp` `admit_location`; `workshop/files.hpp`
`admit_location`; `workshop/marks.hpp` `admit_location`; `tests/test_workshop_files.cpp` case
`"PROJ-2: a location is one absolute spelling, admitted the same way every time"`, case
`"PROJ-2: origin is the ADMITTED spelling of the launch location, not the raw one"`.
WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-10 — Filenames are `std::string` everywhere, so admission is a path law

LAW — Names are `u8string()` bytes: a printable-ASCII name is exact and openable, any other keeps its row as a `?`-marked projection and refuses activation; what is inside a file is the editor's question.

DOES NOT MEAN
- that a file-type registry or extension list exists: a `.png` meets the refusal that knows why.

PROVEN BY — `workshop/files.hpp` `printable_ascii_name`, `openable`, `admit_filename`;
`workshop/path_admission.hpp` `admit_filename`; `tests/test_workshop_files.cpp` case `"EDIT-1: a
name outside printable ASCII keeps its row, marked, and cannot be opened"`, case `"EDIT-1: the
browser does not judge contents -- the editor's door does"`, case `"EDIT-1: a name the path
custody cannot carry refuses at the browser, in its words"`.
WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-11 — Asking for a path's bytes can throw, and one header is allowed to ask

LAW — One header alone asks for a path's bytes: a path is admitted as a value — carried or not, plus its spelling — a filename as a name with an exact flag, and the launch capture is the host's.

MEANS
- `exact`, never the bytes, is what `files_open` and `files_use_recipes` ask through `openable`;
- a refused name's `?` projection is printable ASCII and would otherwise read as openable;
- a second `generic_string()` anywhere is a second way for the process to die.

PROVEN BY — `workshop/path_admission.hpp` `carried`, `exact`, `admit_path`, `admit_filename`,
`launch_project_dir`, `u8string`; `workshop/weave.hpp` `files_open`, `files_use_recipes`;
`tests/test_workshop_files.cpp` case `"QR-12: an ordinary path and an ordinary name are carried
exactly as they were"`, case `"QR-12: a name this platform will not spell is one inert row, not
the end of it"`, case `"QR-12: a name this platform will not spell refuses at the browser's
door"`.
WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-12 — A listing is not a per-paint population

LAW — A listing is recomputed at open, enter, parent, `files.refresh`, and on a finished build gated on `build_news`; no watcher, no timer, no poll, and staleness between those moments is named, not solved.

PROVEN BY — `workshop/weave.hpp` `build_news`; `workshop/keymap.hpp` `files.refresh`;
`docs/workshop/limitations.md` `listing`; `tests/test_workshop_files.cpp` case `"EDIT-1: the
listing is a snapshot -- painting does not re-walk the directory"`, case `"EDIT-1: reopening the
pane takes a fresh listing"`, case `"EDIT-1: a finished build gives the browser a fresh listing,
and nothing else does"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-13 — Bounds and order

LAW — `kMaxListedEntries` stops the walk and the header says `stopped counting`, never an unreached total; directories first, then files, bytewise over admitted name bytes within each class.

MEANS
- no locale, no natural sort, no extension grouping, no configuration;
- a directory that cannot be listed is a refusal, not an empty listing.

PROVEN BY — `workshop/files.hpp` `kMaxListedEntries`; `tests/test_workshop_files.cpp` case
`"EDIT-1: a bound claims what it read, and never a total it never reached"`, case `"EDIT-1: a
listing is directories first, then files, bytewise inside each"`, case `"EDIT-1: a directory
that cannot be listed is a refusal, not an empty listing"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## Do not assume

- That the Files pane cannot leave the project, that a linked directory is refused, or that its
  location is project-relative — none holds; what did not move is the project anchor
  (WL-FILES-02, WL-FILES-03, WL-FILES-04).
- That a marked place is part of the project, trusted, or buildable — it says one thing:
  somebody may want to come back here (WL-FILES-05).
