# Workshop law — files

Register `WL-FILES`: the browser over the machine, paths, marks and roots. One law per heading;
cite by ID. Router: [`../workshop.md`](../workshop.md).

⚠ THE BROWSER IS NOT THIS HOST'S CODE. It is `files/`, a loadable weave in the role
`zengine.files`, offering `project-files` and arriving by a plan row. This register stayed here
because what it states is a law both sides keep: most of it constrains the package, and the last
two entries constrain the host. Values are proved in `files_weave`, gestures in
`workshop_panes_project`.

Retired: WL-FILES-03, WL-FILES-06, WL-FILES-14, WL-FILES-15, WL-FILES-18, WL-FILES-19, WL-FILES-20

## WL-FILES-01 — The browser owns no source truth

LAW — The browser is its own package and its state its own shape; a row is a name, a kind, a linked flag and an openable flag and nothing else, and what it denotes is derived at activation.

MEANS
- no resolved path, recipe, artifact, build or editor state rides a row;
- the browser never judges contents — the Editor's door does;
- a same-shape reload keeps two fields: the location, and the cursor.

WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-02 — Four facts coincide at launch and are not the same fact

LAW — The project is what a project-relative spelling means; the origin is where this run began; the location is where somebody is looking; the operating system is what may be read, never modelled.

MEANS
- the project has one writer, `main`; browsing, marking and a foreign catalog never move it;
- origin is generated once, never persisted, never moved, and not renamed "the project";
- the pane cannot READ the project: it asks `zengine.project` and is answered a value.

WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-04 — A linked directory is marked and enterable

LAW — A directory row is `linked` when the entry leaves the tree: on Windows the host says reparse point; elsewhere following says directory and not following does not; never `is_symlink()`.

MEANS
- asked of the path, never of the listing's copy, which Windows documents as possibly stale;
- libstdc++ on Windows cannot see a reparse point unfollowed, which is why the host is asked;
- the mark says "leaves the tree", not symbolic link versus junction, and does not spread.

WHY — `agents/decisions/the-host-says-what-leaves-the-tree.md`

## WL-FILES-05 — A location mark is a destination and nothing else

LAW — One `LocationMarks` owner holds every stop, with three provenances as flags — origin, weaver marks, host roots — and a mark confers no authority, membership, trust or recipe base.

MEANS
- it lives inside the weave, so nothing in the host can read or write a weaver's places.

WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-07 — Roots are host-reported, asked at the gesture, and never on the paint path

LAW — `files/os.cpp` is the only place this repository asks an operating system for its roots, and nothing on the drawing path may ask for them: what the pane says about where it is is in memory.

MEANS
- host-reported roots are never "every reachable path": a UNC share is in no drive list; the places show the roots read with the listing (WL-FILES-12), never on a paint;
- the residual — no origin and no marks leaves the pane nowhere to jump — is named;
- the keyboard readiness test this law used to constrain is gone: every runtime pane takes it.

WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-08 — The weaver's marks are durable, ride the machine-local root, and refuse by row

LAW — The marks file is its own format, version <!-- value marks_persist::kFormatVersion -->1<!-- /value -->: the file's claims refuse it whole, an uncarriable row is skipped as a standing condition, and a refused file guards the first mark from overwriting it.

MEANS
- without the flag the first `m` would replace bytes this run could not read with an empty list;
- "unusable" is a spelling test, never an existence test: a gone directory is kept;
- WHERE the file lives is the host's answer; the file is the pane's.

WHY — `agents/decisions/the-marks-file-is-state.md`

## WL-FILES-09 — A durable spelling coming back in is a conversion too

LAW — Every write to `current_dir` and every persisted mark goes through `admit_location`, so "absolute, lexically normal, carriable" holds after the seed, an enter, a parent and a jump.

WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-10 — Filenames are `std::string` everywhere, so admission is a path law

LAW — Names are `u8string()` bytes: an exact UTF-8 name with no ASCII control character opens, beyond ASCII where paths are UTF-8; any other keeps its row, `?`-marked, and refuses; contents are the editor's.

DOES NOT MEAN
- that a file-type registry or extension list exists: a `.png` meets the refusal that knows why.

WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-11 — Asking for a path's bytes can throw, and one header is allowed to ask

LAW — `persist/path_admission.hpp` alone asks for a path's bytes: a path is admitted as a value (carried or not, plus its spelling), a filename as a name with an exact flag, the launch capture as the host's.

MEANS
- `exact` and the name's text, never the path again, are what `open` and `use_recipes` ask through `openable`;
- a second `generic_string()` anywhere is a second way for the process to die;
- the header is shared by include path and no link edge, so both halves keep one custody law.

WHY — `agents/decisions/a-refusal-outlives-its-reason.md`

## WL-FILES-12 — A listing is not a per-paint population

LAW — A listing is recomputed at the room grant, an enter, a parent, `files.refresh`, and on a finished build; no watcher, no timer, no poll, and staleness between those moments is named, not solved.

MEANS
- the room grant is the pane's own beat: the walk happens there, never inside anything that draws;
- a walk nobody asked for, a grant or a build, keeps the selected entry by name (`relist`).

WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-13 — Bounds and order

LAW — `kMaxListedEntries` stops the walk and the header says `stopped counting`, never an unreached total; directories first, then files, bytewise over admitted name bytes within each class.

MEANS
- a layout's `listed-entries` stops it sooner, and the header says the same;
- the order takes no locale, no natural sort, no extension grouping, no configuration.

WHY — `agents/decisions/four-facts-that-coincide.md`

## WL-FILES-16 — A pane's rows are its mode's, and an id is one operation

LAW — The pane declares only its mode's rows, one id per operation, with the ids a weaver's keymap names, re-declared as the mode changes; a key they do not claim reaches it as a key.

MEANS
- only the rows in force collide, so each mode's Return is an id of its own;
- an id not in the rows in force is no act; one still there (up, down, cancel) keeps its meaning;
- while the line has the keys only two rows are declared, so Backspace still deletes.

WHY — `agents/decisions/files-is-a-weave.md`

## WL-FILES-17 — Two doors, and what crosses them is a value

LAW — The four facts the browser read off `HostContext` cross as asks to roles and answers back: where this run began, which file the recipes come from, one authored row, one source opened (the Editor's).

MEANS
- every field on the wire is Text, Int, Bool or a List: no reference of any kind crosses;
- a door answers an ROLE, refuses speech with no author, and answers the asker alone;
- knowledge is not authority: hearing where the project is permits reading nothing under it.

WHY — `agents/decisions/files-is-a-weave.md`

## Do not assume

- That the Files pane cannot leave the project, that a linked directory is refused, or that its
  location is project-relative — none holds; what did not move is the project anchor
  (WL-FILES-02, WL-FILES-04).
- That a marked place is part of the project, trusted, or buildable (WL-FILES-05).
- That entering a dangling junction on libstdc++/Windows has been measured — it has not
  (WL-FILES-04, UNWITNESSED).
- That this host can present the browser, or knows anything about it — it holds two doors and
  the plan row that loads it (WL-FILES-17).
- That a pane's declared rows are fixed for its life (WL-FILES-16).

## Proven by

### WL-FILES-01
`files/files.hpp` FileRow
`files/vocabulary.hpp` FilesState
`files/files.cpp` open on(ProjectRoot)
`tests/test_files.cpp` "a FilesState is a shape a same-shape reload keeps"
`tests/test_workshop_panes_files.cpp` "Return on a source opens it in the Editor, through the one
door" "Files reloaded in place keeps the place it shows and the row the weaver was on"

### WL-FILES-02
`workshop/weave.hpp` HostContext::project_dir
`files/files.cpp` ask_project_root
`files/marks.hpp` LocationMarks::origin LocationMarks::settled
`files/vocabulary.hpp` FilesState::current_dir
`tests/test_workshop_files.cpp` "the project door answers the weave that asked, and nobody else" "an
external catalog is chosen live, and the project still owns relative sources"

### WL-FILES-04
`files/os.cpp` leaves_the_tree GetFileAttributesW symlink_status
`files/files.hpp` FileRow::linked
`files/files.cpp` open
`files/browser_view.hpp` row_text
`tests/test_workshop_panes_files.cpp` "a linked directory is marked, entered through its own
spelling, and left the way it came"
UNWITNESSED — a dangling junction on libstdc++/Windows lists as a linked directory row, and what
`open` says when a weaver enters it was not measured.

### WL-FILES-05
`files/marks.hpp` LocationMarks
`files/files.cpp` mark jump_mark
`tests/test_files.cpp` "the marks owner is session truth, and Files is only its first reader"
`tests/test_workshop_panes_files.cpp` "a marked place is the pane's own file, written by the pane"

### WL-FILES-07
`files/os.cpp` host_filesystem_roots GetLogicalDrives
`files/marks.hpp` LocationMarks::provenance
`files/files.cpp` say_where where
`tests/test_files.cpp` "the host's filesystem roots are asked for, never invented"

### WL-FILES-08
`files/marks_persist.hpp` kFormatVersion from_text WorkshopMark
`workshop/pane_seam_vocabulary.hpp` kDefaultMarksFileName
`files/files.cpp` load_marks save_marks marks_refused_
`tests/test_files.cpp` "a persisted mark is admitted, never re-based, and never quietly dropped"
`tests/test_workshop_panes_files.cpp` "a marked place is the pane's own file, written by the pane"
"a marks file this run could not read keeps its bytes when the weaver marks a place"

### WL-FILES-09
`persist/path_admission.hpp` admit_location
`files/files.cpp` admit_location
`files/marks.hpp` admit_location
`tests/test_files.cpp` "a location is one absolute spelling, admitted the same way every time"

### WL-FILES-10
`files/files.hpp` carried_name narrow_paths_are_utf8 FileRow::openable shown_name
`persist/path_admission.hpp` admit_filename
`files/files.cpp` open
`files/browser_view.hpp` row_text
`tests/test_files.cpp` "a name beyond ASCII is carried exactly and opens, drawn as the screen spells
it"

### WL-FILES-11
`persist/path_admission.hpp` AdmittedPath::carried AdmittedName::exact admit_path admit_filename
launch_project_dir u8string
`files/files.cpp` open use_recipes
`tests/test_files.cpp` "an ordinary path and an ordinary name are carried exactly as they were" "a
name this platform will not spell is one inert row, not the end of it"

### WL-FILES-12
`files/files.cpp` refresh relist
`files/vocabulary.hpp` kActionRefresh
`workshop/docs/limitations.md` listing
`tests/test_workshop_panes_files.cpp` "the pane lists the place this run began, asked of the host"
"with pane titles hidden, a first press on the row painted gamma selects gamma once every delivery
it caused has settled, and a later press opens gamma"

### WL-FILES-13
`files/files.hpp` kMaxListedEntries row_before
`tests/test_files.cpp` "a listing is directories first, then files, bytewise inside each" "a
directory that cannot be listed is a refusal, not an empty listing"

### WL-FILES-16
`files/files.cpp` declare action_rows answers
`files/vocabulary.hpp` kActionOpen kActionChoose kActionCommitField
`workshop/keymap_join.hpp` join_pane_rows
`tests/test_workshop_panes_files.cpp` "the pane declares its rows with the ids a weaver's keymap
already knows" "the authoring line takes raw keys, and Escape abandons it whole" "an id Files does
not declare in the mode it is in is no act: an unknown one, a browsing id while the authoring line
is open, and a second cancel resolved in the same poll leave the notice standing through a new room,
and a declared id that moves nothing still spends it" "an id Files resolved in one mode is no act in
the next" "each Files mode's Return is its own id, and a keymap moves each alone"

### WL-FILES-17
`workshop/project_seam_vocabulary.hpp` ProjectRootRequested ProjectRoot RecipeUseRequested
RecipeAuthorRequested RecipeOutcome
`workshop/open_seam_vocabulary.hpp` OpenSourceRequested SourceOpened
`workshop/pane_seam_vocabulary.hpp` kEditorRole
`workshop/pane_doors.hpp` ProjectDoor
`editor/editor-pane/pane.cpp` on(OpenSourceRequested)
`tests/test_workshop_files.cpp` "the project door answers the weave that asked, and nobody else" "a
door answers a role, and refuses speech with no author" "the project door spends this host's one
recipe writer and re-words nothing"
`tests/test_workshop_panes_editor.cpp` "the door refuses speech with no author, and answers nobody"
