# Workshop law — the Neovim-backed Editor

Register `WL-NVIM`: the Editor's role held with a Neovim, as a load plan choice beside the
standard Editor. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md). A
switch between the two is [`editor-switch.md`](editor-switch.md); an open is
[`opening.md`](opening.md); what crosses the seam is the protocol's law in
[`../panes.md`](../panes.md); what it carries out and takes in, and a change Neovim holds, is
[`neovim-transfers.md`](neovim-transfers.md); Neovim's own behaviour is Neovim's, measured and
never restated.

Retired: WL-NVIM-01, WL-NVIM-02, WL-NVIM-07, WL-NVIM-09

## WL-NVIM-03 — A document crosses to Neovim and back exactly, and what does not is said

LAW — Adoption loads the file, replaces its lines with no undo step, sets format, final newline and modified flag, and places caret and selection by keys, read back; export carries them home.

MEANS
- a caret past the final newline and a one-character backward selection are adjusted, and said;
- a linewise selection crosses as its lines, a blockwise one as its cursor, each said;
- a modified buffer's saved comparison is its file; a file not yet written counts as unsaved.

WHY — `agents/decisions/neovim-holds-the-editor-role.md`

## WL-NVIM-04 — A switch away from Neovim names what it loses, resets and refuses

LAW — Other modified buffers and running terminal jobs are losses to consent to; undo, registers, marks, windows and clean buffers are resets; a prompt, a non-file buffer or an uncarriable format refuses.

MEANS
- an unfinished count is cancelled with Escape, and said among the resets;
- the boundary asks Neovim again, so losses that moved send the weaver back to confirm.

WHY — `agents/decisions/neovim-holds-the-editor-role.md`

## WL-NVIM-05 — Neovim is never waited on, and a Neovim that cannot serve is said in words

LAW — No handler waits on Neovim: a question is answered on the wake that hears it, outstanding at its bound; a start answers later; a missing, too old, prompting or silent Neovim is said in its words.

MEANS
- a bus ask the Editor answers from Neovim is answered when Neovim answers, and the bus delivers meanwhile;
- a warm-up that never answers keeps the switch pending and cancellable; with no Neovim the pane says so, an open is refused in words, and the quit is permitted;
- the pane is a source of the host's one wait: it claims Neovim's pipes and its next bound, each begun at the first time the wait hands after it was set, and is woken then.

DOES NOT MEAN
- that a press or a cursor races its question: a press on the highlight reaches Neovim once it has said what it selected, a location's cursor once it has shown the file;
- that Neovim keeps the wait's time: it is a real process on the machine's, and ending it waits on the machine's.

WHY — `agents/decisions/neovim-holds-the-editor-role.md`

## WL-NVIM-06 — Neovim belongs to one incarnation, and each way it ends is its own

LAW — Hiding the pane ends nothing; a switch away ends Neovim once its successor serves; `:qa` leaves the role held with no document, said so; the quit asks Neovim; a reload is refused while it runs.

MEANS
- the quit is refused naming the unsaved buffers, or the running terminal jobs;
- a document handed to a Neovim that ended before it served is carried back as it was handed.

WHY — `agents/decisions/neovim-holds-the-editor-role.md`

## WL-NVIM-08 — From a Loom with no Workshop, Neovim listens for a second terminal

LAW — `NeovimStartRequested` starts Neovim headless and listening and answers the address and the line that attaches to it; the status and the stop answer as `zen.Result` or `zen.Refused`.

MEANS
- the stop is refused while a buffer holds unsaved changes, naming them, unless `discard` is set;
- `loom-host` keeps its console: Neovim's own interface runs in the second terminal.

WHY — `agents/decisions/neovim-holds-the-editor-role.md`

## Do not assume

- That the gated cases ran on a lane with no Neovim: they are behind the `neovim` gate, and the
  always cases use a fake that fails at start on purpose.
- That a Neovim ended before it served hands its document back only by a witness: WL-NVIM-06's
  carried-back document is UNWITNESSED.
- That a live Neovim session survives a reload of this image: a reload is refused instead.

## Proven by

### WL-NVIM-03
`editor/neovim/document.hpp` place carry neovim_text file_bytes
`editor/neovim-editor/opening_switch.hpp` on(EditorAdoptRequested) adopt_placed
`tests/test_workshop_neovim.cpp` "standard to Neovim and back carries the unsaved document and its
caret exactly, with Neovim editing between" "a selection crosses to Neovim as Visual and comes back
as the same range, in its direction"
`tests/test_neovim.cpp` "every placement a document allows carries back exactly, or says it was
adjusted"

### WL-NVIM-04
`editor/neovim-editor/opening_switch.hpp` judge judged_from until_unblocked
on(EditorHandoffJudgeRequested) on(EditorHandoffRequested)
`tests/test_workshop_neovim.cpp` "a switch away from Neovim names another modified buffer for
consent, and carries the current one" "an unfinished command in Neovim is reset by a switch and
said, and a prompt refuses the switch until it is answered"

### WL-NVIM-05
`editor/neovim/host.hpp` Host::ask_later Host::start Host::pump Host::due_ms Host::hand Bound
`editor/neovim/child.hpp` Child::say_last
`editor/neovim-editor/pane.cpp` ask ask_rpc hear when_ready ensure_running settle_readies settle_wait
on(Waited)
`editor/neovim-editor/opening_switch.hpp` on(EditorWarmRequested) settle_warm
`editor/neovim-editor/transfers.hpp` press_at settle_location
`tests/test_workshop_neovim.cpp` "a Neovim that never answers a question holds no handler: the rest
of the bus keeps delivering, the pane claims the question's bound in the host's wait, and the ask is
answered in words at it" "a Neovim started after the pane heard no time for a while is given its
whole start, from the next time the wait hands" "a question asked after the pane heard no time for
a while is given its whole bound, from the next time the wait hands" "the Neovim
editor is a source of the host's wait: it claims what it reads as its role, orders the Timer
nothing, and reads when the wait wakes it" "a Neovim choice whose program is not there refuses the switch in
words, and the standard Editor keeps editing" "a Neovim older than 0.11 refuses the switch naming
its version" "a Neovim that stops at a prompt while starting refuses the switch in its words" "a
Neovim that never answers keeps the switch pending and cancellable, and the candidate goes with the
cancel" "the Neovim editor holding the role with no Neovim says so on its pane, refuses an open in
words, and permits the quit"
`tests/test_neovim.cpp` "a start that never answers fails at its bound on the time the owner is
handed, from the first time handed after it, never before" "a question a peer holds is said
outstanding at the first pump its bound has passed on the time handed, its ending heard late, once"

### WL-NVIM-06
`editor/neovim-editor/pane.cpp` snapshot on(PaneQuitRequested) answer_quit absorb
`editor/neovim-editor/opening_switch.hpp` judge
`tests/test_workshop_neovim.cpp` "the orderly quit is refused while Neovim holds unsaved changes,
naming the file, and permitted once written" "Neovim ended from inside leaves the role held with
no document, said so, and switching back carries nothing" "a reload of the Neovim editor is refused
while its Neovim runs, said on its pane, and Neovim keeps running"
UNWITNESSED — the handed document carried back when Neovim ends before it serves

### WL-NVIM-08
`editor/neovim-editor/opening_switch.hpp` on(NeovimStartRequested) on(NeovimStopRequested)
on(NeovimStatusRequested)
`editor/neovim/launch.hpp` launch_spec
`tests/test_workshop_neovim.cpp` "from a Loom with no Workshop, Neovim listens for a second
terminal, says where, and stops only when nothing is lost"
