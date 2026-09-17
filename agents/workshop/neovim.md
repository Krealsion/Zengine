# Workshop law — the Neovim-backed Editor

Register `WL-NVIM`: the Editor's office held with a Neovim, as a load plan choice beside the
standard Editor. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md). A
switch between the two is [`editor-switch.md`](editor-switch.md); an open is
[`opening.md`](opening.md); what crosses the seam is the protocol's law in
[`../panes.md`](../panes.md); Neovim's own behaviour is Neovim's, measured and never restated.

## WL-NVIM-01 — Neovim holds the Editor's office as the Editor, and every entry point reaches it

LAW — The Neovim-backed Editor holds `zengine.editor` with the Editor's pane key and answers the doors the standard Editor answers, so Files, the Builder and Edit Code reach it by reaching the office.

MEANS
- an open is loaded hidden, its claim unmoved, and shown at the publication, or Declined in words;
- the old door relays to the opening manager with the requester's right kept;
- while its pane holds the keys, `^s` writes the buffer and `^o` is Neovim's own jump back.

DOES NOT MEAN
- that Workshop knows it is Neovim: no host line names this weave or its artifact.

PROVEN BY — `neovim-editor/pane.cpp` `NeovimEditorWeave`, `on(PrepareSourceRequested)`,
`on_claim_published`, `on(OpenSourceRequested)`, `declare`, `on(PaneActionRequested)`;
`neovim-editor/vocabulary.hpp` `kEditorOffice`, `kEditorPane`, `kActionWrite`, `kActionJumpOlder`;
`workshop/keymap.hpp` `kActionCatalog`; `neovim/lua.hpp` `kModule`;
`tests/test_workshop_neovim.cpp` case `"an open through the office shows the file in Neovim, and
the save chord writes it"`, case `"after a switch to Neovim, an open through the office shows
another file in Neovim, beside the unsaved one"`; `tests/test_neovim_live.cpp` case `"preparing a
file hidden leaves the heard document the one Neovim shows"`;
`tests/test_workshop_panes_actions.cpp` case `"the join judges a declaration whole, in order, and
a refusal writes nothing"`.
WHY — `agents/decisions/neovim-holds-the-editor-office.md`

## WL-NVIM-02 — Neovim's screen crosses in the pane protocol's words

LAW — The pane is one status row over Neovim's screen in drawable ASCII; a bar or underline cursor is the caret, and the one range is Visual, else the menu's selected item, else the block cursor's cell.

MEANS
- the status row says saved or UNSAVED, the mode and the file, or a standing notice in its place;
- Neovim is given the room less that row and the caret's column, and resized with it;
- rows carry the document's generation, so a retired editor's rows never repaint this one.

DOES NOT MEAN
- syntax colours, search matches or a rectangle: the protocol has one range, the named next seam.

PROVEN BY — `neovim/projection.hpp` `project`, `ascii_of`; `neovim-editor/pane.cpp` `compose`,
`say`, `status_text`; `tests/test_neovim.cpp` case `"the projection: a block cursor is its cell, a
bar is a caret, rows carry their meaning"`, case `"the projection: Visual runs through the cursor
cell, blockwise is the cursor row, a menu item is the range"`, case `"every cell becomes one
drawable byte, and the document itself is never touched"`; `tests/test_workshop_neovim.cpp` case
`"standard to Neovim and back carries the unsaved document and its caret exactly, with Neovim
editing between"`.
WHY — `agents/decisions/neovim-holds-the-editor-office.md`

## WL-NVIM-03 — A document crosses to Neovim and back exactly, and what does not is said

LAW — Adoption loads the file, replaces its lines with no undo step, sets format, final newline and modified flag, and places caret and selection by keys, read back; export carries them home.

MEANS
- a caret past the final newline and a one-character backward selection are adjusted, and said;
- a linewise selection crosses as its lines, a blockwise one as its cursor, each said;
- a modified buffer's saved comparison is its file; a file not yet written counts as unsaved.

PROVEN BY — `neovim/document.hpp` `place`, `carry`, `neovim_text`, `file_bytes`;
`neovim-editor/pane.cpp` `on(EditorAdoptRequested)`, `judge_now`; `tests/test_workshop_neovim.cpp`
case `"standard to Neovim and back carries the unsaved document and its caret exactly, with Neovim
editing between"`, case `"a selection crosses to Neovim as Visual and comes back as the same
range, in its direction"`; `tests/test_neovim.cpp` case `"every placement a document allows
carries back exactly, or says it was adjusted"`.
WHY — `agents/decisions/neovim-holds-the-editor-office.md`

## WL-NVIM-04 — A switch away from Neovim names what it loses, resets and refuses

LAW — Other modified buffers and running terminal jobs are losses to consent to; undo, registers, marks, windows and clean buffers are resets; a prompt, a non-file buffer or an uncarriable format refuses.

MEANS
- an unfinished count is cancelled with Escape, and said among the resets;
- the boundary asks Neovim again, so losses that moved send the maker back to confirm.

PROVEN BY — `neovim-editor/pane.cpp` `judge_now`, `wait_unblocked`,
`on(EditorHandoffJudgeRequested)`, `on(EditorHandoffRequested)`; `tests/test_workshop_neovim.cpp`
case `"a switch away from Neovim names another modified buffer for consent, and carries the
current one"`, case `"an unfinished command in Neovim is reset by a switch and said, and a prompt
refuses the switch until it is answered"`.
WHY — `agents/decisions/neovim-holds-the-editor-office.md`

## WL-NVIM-05 — Waiting on Neovim is bounded, and a Neovim that cannot serve is said in words

LAW — The flow is never waited on; a question is asked within a bound beside Neovim's fast mode; a start answers later; a missing, too old, prompting or silent Neovim is said in its words and moves nothing.

MEANS
- a warm-up that never answers keeps the switch pending and cancellable; its candidate goes too;
- with no Neovim the pane says so, an open is refused in words, and the quit is permitted;
- the beat is ordered in the Timer's own words, and a beat the Timer refuses is said on the pane.

PROVEN BY — `neovim/host.hpp` `Host::call_now`, `Host::start`; `neovim-editor/pane.cpp`
`on(EditorWarmRequested)`, `settle_warm`, `lua_now`, `ensure_running`, `ensure_beat`,
`on(TimerResolution)`; `tests/test_workshop_neovim.cpp` case `"the Neovim editor orders its beat
in words the Timer reads, and a refused beat is said on its pane"`;
`tests/test_workshop_neovim.cpp` case `"a Neovim choice whose program is not there refuses the
switch in words, and the standard Editor keeps editing"`, case `"a Neovim older than 0.11 refuses
the switch naming its version"`, case `"a Neovim that stops at a prompt while starting refuses the
switch in its words"`, case `"a Neovim that never answers keeps the switch pending and
cancellable, and the candidate goes with the cancel"`, case `"the Neovim editor holding the office
with no Neovim says so on its pane, refuses an open in words, and permits the quit"`.
WHY — `agents/decisions/neovim-holds-the-editor-office.md`

## WL-NVIM-06 — Neovim belongs to one incarnation, and each way it ends is its own

LAW — Hiding the pane ends nothing; a switch away ends Neovim once its successor serves; `:qa` leaves the office held with no document, said so; the quit asks Neovim; a reload is refused while it runs.

MEANS
- the quit is refused naming the unsaved buffers, or the running terminal jobs;
- a document handed to a Neovim that ended before it served is carried back as it was handed.

PROVEN BY — `neovim-editor/pane.cpp` `snapshot`, `on(PaneQuitRequested)`, `absorb`, `judge_now`;
`tests/test_workshop_neovim.cpp` case `"the orderly quit is refused while Neovim holds unsaved
changes, naming the file, and permitted once written"`, case `"Neovim ended from inside leaves the
office held with no document, said so, and switching back carries nothing"`, case `"a reload of
the Neovim editor is refused while its Neovim runs, said on its pane, and Neovim keeps running"`.
UNWITNESSED — the handed document carried back when Neovim ends before it serves
WHY — `agents/decisions/neovim-holds-the-editor-office.md`

## WL-NVIM-07 — The clipboard crosses through the Skin, both ways

LAW — A copy Neovim makes to `+` or `*` is published as the process's clipboard; a paste Neovim asks for is asked of the Skin and answered to the request Neovim is waiting on.

MEANS
- a linewise copy is its lines and one newline; text ending in a newline pastes linewise;
- the provider is installed only where the maker's own configuration chose none.

PROVEN BY — `neovim/lua.hpp` `kModule`; `neovim-editor/pane.cpp` `absorb`, `on(ClipboardText)`,
`join_lines`, `paste_lines`; `tests/test_workshop_neovim.cpp` case `"a copy in Neovim reaches the
Skin, and a paste in Neovim asks the Skin"`.
WHY — `agents/decisions/neovim-holds-the-editor-office.md`

## WL-NVIM-08 — From a Loom with no Workshop, Neovim listens for a second terminal

LAW — `NeovimStartRequested` starts Neovim headless and listening and answers the address and the line that attaches to it; the status and the stop answer as `zen.Result` or `zen.Refused`.

MEANS
- the stop is refused while a buffer holds unsaved changes, naming them, unless `discard` is set;
- `loom-host` keeps its console: Neovim's own interface runs in the second terminal.

PROVEN BY — `neovim-editor/pane.cpp` `on(NeovimStartRequested)`, `on(NeovimStopRequested)`,
`on(NeovimStatusRequested)`; `neovim/launch.hpp` `launch_spec`; `tests/test_workshop_neovim.cpp`
case `"from a Loom with no Workshop, Neovim listens for a second terminal, says where, and stops
only when nothing is lost"`.
WHY — `agents/decisions/neovim-holds-the-editor-office.md`

## WL-NVIM-09 — Which Neovim and which configuration, judged before a start

LAW — `ZENGINE_NEOVIM` names the program and `ZENGINE_NEOVIM_PROFILE` the configuration -- `clean`, `user`, or an init file resolved against this process's own directory -- judged before a Neovim is started.

MEANS
- a value that is neither word and names no file is refused, naming where it looked;
- a running Neovim's pane says the profile in a word beside its mode; an answer says it whole;
- nothing else names either -- no shape, poke or plan row -- so the environment is the one owner.

DOES NOT MEAN — that a relative init path follows Neovim's own working directory, which is the
project's: it follows the directory this process was started in, where the maker set the variable.

PROVEN BY — `neovim/launch.hpp` `check_profile`, `ProfileChoice`, `profile_tag`,
`profile_words`, `choice_from_environment`, `kProfileVariable`, `kProgramVariable`;
`neovim-editor/pane.cpp` `start`, `status_text`, `started_words`;
`tests/test_workshop_neovim.cpp` case `"a profile that is neither clean nor user and names no
init file refuses the switch before starting Neovim"`, case `"the profile a maker names is the
configuration that runs and the pane says which one it is"`.
WHY — `agents/decisions/neovim-holds-the-editor-office.md`

## Do not assume

- That the gated cases ran on a lane with no Neovim: they are behind the `neovim` gate, and the
  always cases use a fake that fails at start on purpose.
- That a Neovim ended before it served hands its document back only by a witness: WL-NVIM-06's
  carried-back document is UNWITNESSED.
- That a live Neovim session survives a reload of this image: a reload is refused instead.
