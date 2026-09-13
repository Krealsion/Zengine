# Workshop law — files

Register `WL-FILES`: the browser over the machine, paths, marks and roots. One law per heading;
cite by ID. Router: [`../workshop.md`](../workshop.md).

⚠ THE BROWSER IS NOT THIS HOST'S CODE. It is `Zengine/files/`, a loadable weave in the office
`zengine.files`, offering `project-files` and arriving by a plan row. This register stayed here
because what it states is a law both sides keep: most of it constrains the package, and the last
two entries constrain the host. Values are proved in `files_weave`, gestures in `workshop_panes`.

## WL-FILES-01 — The browser owns no source truth

LAW — The browser is its own package and its state its own shape; a row is a name, a kind, a linked flag and an openable flag and nothing else, and what it denotes is derived at activation.

MEANS
- no resolved path, recipe, artifact, build or editor state rides a row;
- the browser never judges contents — the Editor's door does;
- a same-shape reload keeps two fields: the location, and the cursor.

PROVEN BY — `files/files.hpp` `FileRow`; `files/vocabulary.hpp` `FilesState`;
`files/files.cpp` `open`; `tests/test_files.cpp` case `"a FilesState is a shape a same-shape
reload keeps"`; `tests/test_workshop_panes_files.cpp` case `"FILES-WEAVE: Return on a source
opens it in the Editor, through the one door"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-02 — Four facts coincide at launch and are not the same fact

LAW — The project is what a project-relative spelling means; the origin is where this run began; the location is where somebody is looking; the operating system is what may be read, never modelled.

MEANS
- the project has one writer, `main`; browsing, marking and a foreign catalog never move it;
- origin is generated once, never persisted, never moved, and not renamed "the project";
- the pane cannot READ the project: it asks `zengine.project` and is answered a value.

PROVEN BY — `workshop/weave.hpp` `HostContext::project_dir`; `files/files.cpp`
`ask_project_root`; `files/marks.hpp` `LocationMarks::origin`, `LocationMarks::settled`;
`files/vocabulary.hpp` `FilesState::current_dir`; `tests/test_workshop_files.cpp` case
`"PANE-DOOR: the project door answers the weave that asked, and nobody else"`, case `"PROJ-2: an
external catalog is chosen live, and the project still owns relative sources"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-03 — Parent is lexical and stops at the filesystem, not at the project

LAW — `parent_path()` until the fixed point `p.parent_path() == p`, which POSIX `/`, a drive root and `//server/` all answer; `has_parent_path()` is not a root test, and nothing canonicalizes.

PROVEN BY — `files/marks.hpp` `parent_location`, `parent_path`, `at_filesystem_root`;
`files/files.cpp` `parent`; `tests/test_files.cpp` case `"PROJ-2: parent is lexical and stops
where a path stops, not where a project does"`; `tests/test_workshop_panes_files.cpp` case
`"FILES-WEAVE: Return walks into a directory, Backspace walks out"`.
WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-04 — A linked directory is marked and enterable

LAW — A directory row is `linked` when the entry leaves the tree: on Windows the host says reparse point; elsewhere following says directory and not following does not; never `is_symlink()`.

MEANS
- asked of the path, never of the listing's copy, which Windows documents as possibly stale;
- libstdc++ on Windows cannot see a reparse point unfollowed, which is why the host is asked;
- the mark says "leaves the tree", not symbolic link versus junction, and does not spread.

PROVEN BY — `files/os.cpp` `leaves_the_tree`, `GetFileAttributesW`, `symlink_status`;
`files/files.hpp` `FileRow::linked`; `files/files.cpp` `row_text`, `open`;
`tests/test_files.cpp` case `"EDIT-1: a listing shows what is there -- dotfiles and build trees
included"`.
UNWITNESSED — a dangling junction on libstdc++/Windows lists as a linked directory row, and
what `open` says when a maker enters it was not measured.
WHY — `agents/decisions/the-host-says-what-leaves-the-tree.md`

## WL-FILES-05 — A location mark is a destination and nothing else

LAW — One `LocationMarks` owner holds every stop, with three provenances as flags — origin, maker marks, host roots — and a mark confers no authority, membership, trust or recipe base.

MEANS
- it lives inside the weave, so nothing in the host can read or write a maker's places.

PROVEN BY — `files/marks.hpp` `LocationMarks`; `files/files.cpp` `mark`, `jump_mark`;
`tests/test_files.cpp` case `"PROJ-2: the marks owner is session truth, and Files is only its
first reader"`; `tests/test_workshop_panes_files.cpp` case `"FILES-WEAVE: a marked place is the
pane's own file, written by the pane"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-06 — The traversal set is built at the gesture and held nowhere

LAW — Origin, the maker's marks sorted bytewise, then `host_filesystem_roots()` asked fresh; one address is one stop, and there is no standing selected mark — the cycle starts where the browser is.

PROVEN BY — `files/marks.hpp` `LocationMarks::somewhere_to_go`, `LocationMarks::destinations`;
`files/os.cpp` `host_filesystem_roots`; `files/files.cpp` `jump_mark`; `tests/test_files.cpp`
case `"PROJ-2: one address is one traversal stop, however many ways it is known"`, case
`"PROJ-2: the host's filesystem roots are asked for, never invented"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-07 — Roots are host-reported, asked at the gesture, and never on the paint path

LAW — `files/os.cpp` is the only place this repository asks an operating system for its roots, and nothing on the drawing path may ask for them: what the pane says about where it is is in memory.

MEANS
- host-reported roots are never "every reachable path": a UNC share is in no drive list;
- the residual — no origin and no marks leaves the pane nowhere to jump — is named;
- the keyboard readiness test this law used to constrain is gone: every runtime pane takes it.

PROVEN BY — `files/os.cpp` `host_filesystem_roots`, `GetLogicalDrives`; `files/marks.hpp`
`LocationMarks::provenance`; `files/files.cpp` `say_where`, `where`; `tests/test_files.cpp` case
`"PROJ-2: the host's filesystem roots are asked for, never invented"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-08 — Maker marks are durable, ride the machine-local root, and refuse by row

LAW — The marks file is its own format, version 1: the file's claims refuse it whole, an uncarriable row is skipped as a standing condition, and a refused file guards the first mark from overwriting it.

MEANS
- without the flag the first `m` would replace bytes this run could not read with an empty list;
- "unusable" is a spelling test, never an existence test: a gone directory is kept;
- WHERE the file lives is the host's answer; the file is the pane's.

PROVEN BY — `files/marks_persist.hpp` `kFormatVersion`, `from_text`, `WorkshopMark`;
`workshop/pane_seam_vocabulary.hpp` `kDefaultMarksFileName`; `files/files.cpp` `load_marks`,
`save_marks`, `marks_refused_`; `tests/test_files.cpp` case `"PROJ-2: a persisted mark is
admitted, never re-based, and never quietly dropped"`; `tests/test_workshop_panes_files.cpp`
case `"FILES-WEAVE: a marked place is the pane's own file, written by the pane"`.
WHY — `agents/decisions/the-marks-file-is-state.md`

## WL-FILES-09 — A durable spelling coming back in is a conversion too

LAW — Every write to `current_dir` and every persisted mark goes through `admit_location`, so "absolute, lexically normal, carriable" holds after the seed, an enter, a parent and a jump.

PROVEN BY — `workshop/path_admission.hpp` `admit_location`; `files/files.cpp` `admit_location`;
`files/marks.hpp` `admit_location`; `tests/test_files.cpp` case `"PROJ-2: a location is one
absolute spelling, admitted the same way every time"`.
WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-10 — Filenames are `std::string` everywhere, so admission is a path law

LAW — Names are `u8string()` bytes: a printable-ASCII name is exact and openable, any other keeps its row as a `?`-marked projection and refuses activation; what is inside a file is the editor's question.

DOES NOT MEAN
- that a file-type registry or extension list exists: a `.png` meets the refusal that knows why.

PROVEN BY — `files/files.hpp` `printable_ascii_name`, `FileRow::openable`, `shown_name`;
`workshop/path_admission.hpp` `admit_filename`; `files/files.cpp` `row_text`, `open`;
`tests/test_files.cpp` case `"EDIT-1: a name outside printable ASCII keeps its row,
marked, and cannot be opened"`.
WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-11 — Asking for a path's bytes can throw, and one header is allowed to ask

LAW — `workshop/path_admission.hpp` alone asks for a path's bytes: a path is admitted as a value (carried or not, plus its spelling), a filename as a name with an exact flag, the launch capture as the host's.

MEANS
- `exact`, never the bytes, is what `open` and `use_recipes` ask through `openable`;
- a second `generic_string()` anywhere is a second way for the process to die;
- the header is shared by include path and no link edge, so both halves keep one custody law.

PROVEN BY — `workshop/path_admission.hpp` `AdmittedPath::carried`, `AdmittedName::exact`,
`admit_path`, `admit_filename`, `launch_project_dir`, `u8string`; `files/files.cpp` `open`,
`use_recipes`; `tests/test_files.cpp` case `"QR-12: an ordinary path and an ordinary name are
carried exactly as they were"`, case `"QR-12: a name this platform will not spell is one inert
row, not the end of it"`.
WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-12 — A listing is not a per-paint population

LAW — A listing is recomputed at the room grant, an enter, a parent, `files.refresh`, and on a finished build; no watcher, no timer, no poll, and staleness between those moments is named, not solved.

MEANS
- the room grant is the pane's own beat: the walk happens there, never inside anything that draws.

PROVEN BY — `files/files.cpp` `refresh`; `files/vocabulary.hpp` `kActionRefresh`;
`docs/workshop/limitations.md` `listing`; `tests/test_workshop_panes_files.cpp` case
`"FILES-WEAVE: the pane lists the place this run began, asked of the host"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-13 — Bounds and order

LAW — `kMaxListedEntries` stops the walk and the header says `stopped counting`, never an unreached total; directories first, then files, bytewise over admitted name bytes within each class.

MEANS
- no locale, no natural sort, no extension grouping, no configuration.

PROVEN BY — `files/files.hpp` `kMaxListedEntries`, `row_before`; `tests/test_files.cpp` case
`"EDIT-1: a listing is directories first, then files, bytewise inside each"`, case `"EDIT-1: a
directory that cannot be listed is a refusal, not an empty listing"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-14 — A failed listing has no rows, and an unclassifiable entry is a file

LAW — A directory that is missing, unreadable or replaced lists as a refusal and no rows, never a partial listing; an entry whose kind cannot be asked keeps its row as a file the editor's door refuses.

MEANS
- every failure is an ordinary refusal through the iterator's `error_code` forms: nothing throws.

PROVEN BY — `files/files.hpp` `enumerate_directory`, `Listing`; `tests/test_files.cpp` case
`"EDIT-1: a directory that cannot be listed is a refusal, not an empty listing"`.
WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-15 — `pick buildable` hands over a place, and Files still knows no recipe

LAW — `a` is an ordinary declared row; the gesture hands the location and the listing's names to a chooser in the pane's own room, and a row learns nothing about what it names.

MEANS
- the one thing added to the listing is an existence probe per directory row, at the gesture;
- the chooser and the line are rows in the pane's room: no popup, no panel, no second surface;
- what a maker types is a DRAFT; the host composes, checks and installs it (WL-AUTH-01).

PROVEN BY — `files/vocabulary.hpp` `kActionPickBuildable`; `files/files.cpp` `pick_buildable`,
`chooser_choose`, `authoring_commit`; `workshop/pane_seam_vocabulary.hpp`
`RecipeAuthorRequested`; `tests/test_workshop_panes_files.cpp` case `"FILES-WEAVE: a maker
authors a recipe row in-pane, and the host writes it"`.
WHY — `agents/decisions/a-maker-authors-the-two-files.md`

## WL-FILES-16 — A pane is one keyboard context, so its rows are what is true now

LAW — The pane declares its actions with the ids a maker's keymap already knows and re-declares them when its mode changes; a key the declaration does not claim reaches it as a key.

MEANS
- a pane's rows join into ONE map, and the collision law refuses two rows on one gesture;
- so there is no second commit id: Return is `files.open`, and what it means is the pane's;
- while the line has the keyboard only two rows are declared, so Backspace still deletes.

PROVEN BY — `files/files.cpp` `declare`; `files/vocabulary.hpp` `kActionOpen`, `kActionCancel`;
`workshop/keymap.hpp` `join_pane_rows`; `tests/test_workshop_panes_files.cpp` case
`"FILES-WEAVE: the pane declares its rows with the ids a maker's keymap already knows"`, case
`"FILES-WEAVE: the authoring line takes raw keys, and Escape abandons it whole"`.
WHY — `agents/decisions/files-is-a-weave.md`

## WL-FILES-17 — Three doors, and what crosses them is a value

LAW — The four facts the browser read off `HostContext` cross as asks to offices and answers back: where this run began, which file the recipes come from, one authored row, one source opened (the Editor's).

MEANS
- every field on the wire is Text, Int, Bool or a List: no reference of any kind crosses;
- a door answers an OFFICE, refuses speech with no author, and answers the asker alone;
- knowledge is not authority: hearing where the project is permits reading nothing under it.

PROVEN BY — `workshop/pane_seam_vocabulary.hpp` `ProjectRootRequested`, `ProjectRoot`,
`RecipeUseRequested`, `RecipeAuthorRequested`, `RecipeOutcome`, `OpenSourceRequested`,
`SourceOpened`, `kEditorRole`; `workshop/pane_doors.hpp` `ProjectDoor`, `RecipesDoor`;
`editor-pane/pane.cpp` `on(OpenSourceRequested)`; `tests/test_workshop_files.cpp` case
`"PANE-DOOR: the project door answers the weave that asked, and nobody else"`, case `"PANE-DOOR:
a door answers an office, and refuses speech with no author"`, case `"PANE-DOOR: the recipes
door spends this host's one writer and re-words nothing"`;
`tests/test_workshop_panes_editor.cpp` case `"EDIT-W8: the door refuses speech with no author,
and answers nobody"`.
WHY — `agents/decisions/files-is-a-weave.md`

## Do not assume

- That the Files pane cannot leave the project, that a linked directory is refused, or that its
  location is project-relative — none holds; what did not move is the project anchor
  (WL-FILES-02, WL-FILES-03, WL-FILES-04).
- That a marked place is part of the project, trusted, or buildable (WL-FILES-05).
- That entering a dangling junction on libstdc++/Windows has been measured — it has not
  (WL-FILES-04, UNWITNESSED).
- That this host can present the browser, or knows anything about it — it holds three doors and
  the plan row that loads it (WL-FILES-17).
- That a pane's declared rows are fixed for its life (WL-FILES-16).
