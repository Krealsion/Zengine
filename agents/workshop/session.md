# Workshop law — the session

Register `WL-SESSION`: the durable files, the session, the room and the window. One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-SESSION-01 — Workshop writes the desk, the room and the window, and reads them back

LAW — The maker-facing files live in three ownership domains: project (`--document`, `--setup`, `--pane`), configuration (`--keymap`, `--prefs`), state (`--session`, `--marks`); plans ship beside the binary.

MEANS
- project files follow the launch directory; configuration follows the maker; state, the machine;
- a mark is an absolute path, so the marks file is state, and its own file, not a prefs field.

DOES NOT MEAN
- that the document is read at launch: it is not; the desk, the window and the preferences are.

PROVEN BY — `workshop/user_paths.hpp` `resolve_durable_path`; `workshop/weave.hpp`
`HostContext::document_path`, `HostContext::setup_path`, `HostContext::session_path`,
`HostContext::marks_path`, `HostContext::pane_path`, `HostContext::keymap_path`,
`HostContext::prefs_path`; `workshop/workshop.cpp` `Arguments`;
`tests/test_workshop_persistence.cpp` case `"WUX-3: the two Windows roots are the platform's own
conventions"`, case `"WUX-3: the two XDG roots, and their home fallbacks"`, case `"WUX-3: the host
resolves the maker's files through the one precedence"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-02 — The precedence is pinned and has one spelling

LAW — The precedence has one spelling: an explicit path the maker typed, then `--isolated`, then the per-user default; `--isolated` resolves the per-user defaults to the designed empty-path absence.

MEANS
- `--isolated` is the flag every witness harness and executor live run must carry;
- an environment with no resolvable root is the same absence, said once on the banner.

PROVEN BY — `workshop/user_paths.hpp` `resolve_durable_path`; `workshop/workshop.cpp`
`Arguments::session`; `tests/test_workshop_persistence.cpp` case `"WUX-3: one precedence --
explicit path, then isolation, then the default"`, case `"WUX-3: the host resolves the maker's
files through the one precedence"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-03 — The legacy transition is one rule and converges by existence

LAW — `import_legacy_file`: a per-user default whose file does not exist, beside a pre-root-flip local file that does, imports the local bytes once, safe-written and unjudged, and never fires again.

MEANS
- an existing user-root file always wins; the legacy file is never deleted, moved or rewritten.

PROVEN BY — `workshop/user_paths.hpp` `import_legacy_file`, `LegacyImport`;
`workshop/workshop.cpp` `import_legacy_file`; `workshop/weave.hpp` `HostContext::transition_note`;
`tests/test_workshop_persistence.cpp` case `"WUX-3: a legacy-only file is imported once, and the
original is left in place"`, case `"WUX-3: an existing user-root file always wins over a legacy
file"`, case `"WUX-3: repeated launches converge -- the import can never fire twice"`, case
`"WUX-3: no legacy file, no destination -- the import does nothing, silently"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-04 — One representation of a desk, two files

LAW — The session's written shape nests the setup's written shape as a field, and one function turns a written setup into a live one, whichever file it came from; an automatic save never lands on `--setup`.

MEANS
- a desk cannot be legal in one file and illegal in the other;
- the three files are three formats, and each refuses the others.

PROVEN BY — `workshop/session_persist.hpp` `WorkshopSession`, `setup_in`;
`workshop/setup_persist.hpp` `WorkshopSetup`, `setup_in`; `workshop/weave.hpp`
`HostContext::session_path`; `tests/test_workshop_persistence.cpp` case `"WUX-0 F: an automatic
save never touches the file a maker named"`, case `"WUX-0 F: a restored session never touches the
file a maker named, either"`, case `"WUX-0 F: the three files are three formats, and each refuses
the others"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-13 — One door writes the session, and only on an orderly close

LAW — The quit key, the interrupt chord and the medium's close request all reach one quit, which saves the session before it stops the bus; no autosave, no dirty tracking, no background writer.

MEANS
- crash durability is not claimed: `write_file` does not fsync; a killed run loses its session.

PROVEN BY — `workshop/weave_run.cpp` `quit`; `workshop/weave_session.cpp` `save_last_session`;
`workshop/weave_handlers.cpp` `on(SurfaceCloseRequested)`; `workshop/weave.hpp`
`HostContext::session_path`; `workshop/persist.hpp` `write_file`; `surface/vocabulary.hpp`
`SurfaceCloseRequested`; `workshop/session_persist.hpp` `save_file`;
`tests/test_workshop_persistence.cpp` case `"WUX-0 B: the second session replaces the first, room
and desk both"`, case `"WUX-0: a write that fails leaves the last good session where it was"`;
`tests/test_workshop_document.cpp` case `"the native close request reaches the quit policy `q`
already had"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-15 — A session this run could not read is never written over

LAW — A session this run could not read is never written over: the save checks the refusal first; a declined viewport is not a refusal; the standing consequence is a condition, true all run, with an action.

PROVEN BY — `workshop/weave.hpp` `WorkshopWeave::session_refused_`; `workshop/weave_session.cpp`
`save_last_session`, `kSessionWallKey`; `tests/test_workshop_persistence.cpp` case `"MIG-0/SC-13:
a session this run could not read is never written over"`, case `"MIG-0/SC-13: the file survives
the run that could not read it, and opens later"`.
WHY — `agents/decisions/three-ownership-domains.md`

## WL-SESSION-18 — Roots are made on the first write, and a project directory is never invented

LAW — The per-user roots are created on the first write and never on a read, so a run that persists nothing leaves no trace; a project file into a missing directory is refused, not given one.

MEANS
- the session's close, the prefs toggle and the legacy import write through the making door;
- `--document`, `--setup` and `--pane` paths do not: a missing directory there is a maker's typo.

PROVEN BY — `workshop/persist.hpp` `write_file_making_room`;
`tests/test_workshop_persistence.cpp` case `"WUX-0 C: a first launch is not an error, and needs no
file to exist"`, case `"a save into a place that does not exist refuses before it writes
anything"`.
WHY — `agents/decisions/three-ownership-domains.md`

## Do not assume

- That a session save can be trusted after a crash: it is written on an orderly close and
  nowhere else (WL-SESSION-13).
- That the last session and a named setup are the same thing saved twice: two promises, two
  files (WL-SESSION-04).
