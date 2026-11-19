# Workshop law — the Info body

Register `WL-INFO`: Info's pane-property view, `info/property_view.hpp` in the weave `info/pane.cpp`,
and the levels of a value view's rows. Cite laws by ID. Router: [`../workshop.md`](../workshop.md).
The separate value views are owned by `info/value_view.hpp`; see [inventory](../inventory.md).

Retired: WL-INFO-07, WL-INFO-09, WL-INFO-01, WL-INFO-02, WL-INFO-03, WL-INFO-04, WL-INFO-05, WL-INFO-08, WL-INFO-10, WL-INFO-11, WL-INFO-16

## WL-INFO-06 — A new room must not drop a live draft, and rows named anew must

LAW — A new room keeps the draft; a picture whose rows the host named anew ABANDONS it, saying so: carried onto another pane's or desk's row, it would write a weaver's text into another property.

MEANS
- draft identity is the host’s row identity and the drafted property’s label;
- a room, a moved value or a provider keeps the name; another pane, desk or set of settings does not, nor the row leaving or rejoining the desk;
- abandonment says so; a sent commit remains sent.

DOES NOT MEAN
- that a commit already sent is recalled — the host writes it only while its name holds.

WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-12 — An answer is read against the ask, the draft and the text that asked

LAW — Info sends one commit at a time; its answer closes only the draft that sent it while that draft holds exactly what it sent, replaces only its own sentence, and no draft's end says nothing was written.

MEANS
- a commit asked while one is unanswered is not sent, aloud; the draft and its edits stand;
- `draft_epoch` names the draft and the sent text what a write covers: later typing stays open;
- an inspect keeps its own newest record, so it never hides a commit.

DOES NOT MEAN
- that closing a draft retracts a write, or that a delivered unanswered commit ends.

WHY — `agents/decisions/a-paste-is-a-conversation.md`

## WL-INFO-13 — An ask Loom says ended is released, and nothing waits for ever

LAW — Info asks through Loom's door, keeping each ticket: nothing queued, or Loom's own `zen.AskEnded` naming that attempt, releases that record and says so, closing no draft; a forged notice settles nothing.

MEANS
- the draft and its text stand, and the next Return is a fresh commit, not a second one;
- a notice counts only with Loom's provenance, then the attempt, correlation, shape and role;
- "nothing queued" is the kept ticket branch, source-traced; an owner gone or silent ends the ask too.

DOES NOT MEAN
- that an ask its owner still holds is released: no timeout, retry, cancellation or polling.

WHY — `agents/decisions/an-undelivered-ask-is-released.md`

## WL-INFO-14 — The subject is a pane Info names, and nothing else moves it

LAW — Info names its pane-property subject; the host holds its rows. Selection, focus, Escape and switching views preserve it. Info may inspect itself.

MEANS
- a role asks Info to inspect a pane (the Pane Manager's `inspect in Info`); Info asks the host and answers the role, and a live draft holds the ask back, refused in Info's words;
- row identity covers the pane, the desk, whether it holds the row, and the settings rows; changing any renames the rows;
- an absent pane is refused without movement; an arriving inspector is answered; nothing is published about a subject until one is named.

DOES NOT MEAN
- that two inspectors each hold a subject: there is one slot, and a role that asks moves it.

WHY — `agents/decisions/an-inspector-names-its-subject.md`

## WL-INFO-15 — A property edit is the owner's write, through a door the desk already has

LAW — A commit names the rows, the row and the text; the host judges the name first, writes through the row's own setter — the setup's gesture, reset or settings door — and reseats.

MEANS
- a refusal is the owner's own sentence (a unit it does not read, a value out of range, what a setting takes);
- the band says what was written to which pane, read fresh from the row;
- an answer reaches only the incarnation that asked: a replaced Info is told nothing of it.

DOES NOT MEAN
- inventory-entry edits: live saves use owner identity/revision; a copy first adds an entry.

WHY — `agents/decisions/an-inspector-names-its-subject.md`

## Proven by

### WL-INFO-06
`info/property_view.hpp` shown shows_draft_subject end_draft close_draft Draft::subject draft_
`info/pane.cpp` on(PaneSubjectShown)
`workshop/inspection_seam_vocabulary.hpp` PaneSubjectShown::subject
`tests/test_workshop_panes_info.cpp` "a picture that names other rows abandons the Info draft and
says so, even where they have the same property on the same row, and writes nothing into either
desk" "an Info commit queued behind a restore of the desk from its file is refused, and a refused
restore keeps the draft" "an Info draft outlives a new room and its pane's window moving, and its
commit is written"

### WL-INFO-12
`info/property_view.hpp` acted act answered_commit ask_commit end_draft Asked SentCommit
SentCommit::draft SentCommit::text SentCommit::promise acting_ committing_ kCommitNotSent
`info/pane.cpp` on(PaneSubjectActed)
`component/text_box.hpp` TextBox::draft_epoch
`tests/test_workshop_panes_info.cpp` "a commit and a cancel resolved in one poll: the cancel says
the commit was already sent, the answer's account takes that sentence's place, and no row says
nothing was written over a write" "an inspect asked before an Info draft opened, and answered while
it is open, closes nothing" "a second Info commit in the same poll as the first is not sent: the
pane says so, keeps the text typed between them, sends that text once the first is answered, and a
cancel after the write and a refused retry claims no write away" "text typed after an Info commit
was sent outlives that commit's answer: the draft stays open with its history, the write is told
apart from the unsent text, a refusal is not said of the newer text, and the newer text commits
normally"

### WL-INFO-13
`info/property_view.hpp` refused refused_ask undelivered_commit ask_inspect ask_commit
Asked::attempt kCommitNotQueued kCommitUndelivered
`info/pane.cpp` on(AskEnded)
`tests/test_workshop_panes_info.cpp` "an Info commit Loom refuses at dispatch is released: the draft
and its text stand, the next Return is written, and a cancel's promise is replaced" "an Info commit
a doorless role refuses at dispatch: the draft stands, the next commit tries again, and it is
written once the door is back" "an ending notice anyone could send, naming the Info pane's
outstanding commit exactly, settles nothing" "an Info inspect Loom refuses at dispatch releases its
own record, answers the role that asked, and the next ask inspects"

### WL-INFO-14
`info/property_view.hpp` inspect_asked ask_inspect settle_relay relay_ kFinishTheEdit
`info/pane.cpp` on(InspectPaneRequested)
`workshop/inspection_seam_vocabulary.hpp` InspectPaneRequested PaneSubjectRequested PaneSubjectShown
`workshop/screen.hpp` InspectedPane pane_subject_rows pane_subject_shown
`workshop/screen_pane_subject.cpp` pane_subject_shown pane_subject_rows
`workshop/layouts/run.hpp` SetupState::put_live
`workshop/weave_inspection.cpp` refresh_inspected on(InspectPaneRequested) on(PaneSubjectRequested)
publish_pane_subject
`workshop/live_desk.hpp` put_live
`tests/test_workshop_panes_info.cpp` "a role's ask inspects a pane through the host's own door,
and the role is answered what the host said" "two inspects asked in one burst: the older role is
told a newer took its place, the newer is answered what the host said, and the subject is the
newer's" "a live draft holds another subject back, and the
reason is the weaver's" "an inspect asked while an Info draft is live is refused, keeping the
draft, its text, the subject and the desk through a new room, and inspecting resumes once the draft
ends" "the subject is Info's to name: the keys leaving, Escape and a press elsewhere leave it
standing" "Info may inspect itself, and an edit to its own place is written by the desk, reseats it
and keeps the subject" "Info's property cursor survives its own reload, the subject stands, and
Return opens a draft where the cursor stood" "a subject naming a pane in neither this build's
vocabulary nor this desk is refused in words with nothing moved, and an inspector that arrives is
answered the picture as it is now"
`tests/test_workshop_panes_settings.cpp` "the subject is named anew when its pane's row leaves the
desk, and again when it comes back"

### WL-INFO-15
`workshop/inspection_seam_vocabulary.hpp` PaneCommitRequested PaneSubjectActed
`workshop/weave_inspection.cpp` on(PaneCommitRequested)
`workshop/weave.hpp` kPaneCommitSubjectGone
`workshop/screen_pane_subject.cpp` write_pane_axis
`workshop/screen.hpp` pending_edit
`tests/test_workshop_panes_info.cpp` "a draft on a value the weaver owns is written to the desk" "an
Info commit queued behind another desk put live, or another and back, is refused: neither desk is
written and the pane says why" "an Info commit whose image was replaced before its answer is not the
successor's: the successor holds no draft and says nothing of it, the subject stands, and the write
shows as the owner's rows"
