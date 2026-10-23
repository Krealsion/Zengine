# Workshop law — the Info body

Register `WL-INFO`: Info's pane-property view, `info/property_view.hpp` in the weave `info/pane.cpp`.
Cite laws by ID. Router: [`../workshop.md`](../workshop.md). The separate value views are owned by
`info/value_view.hpp`; see [inventory](../inventory.md).

Retired: WL-INFO-07, WL-INFO-09.

## WL-INFO-01 — The Info body is composed once, by the pane, into the room it was granted

LAW — `compose` makes Info’s body: one composition of one pane’s properties under one heading in the pane-property view, or a value view’s own rows while the default pane holds a value.

MEANS
- one pass composes rows and their press map;
- the heading is `PANE <name>` for the subject an office named (`InspectPaneRequested`, as the Pane Manager's `inspect in Info` asks it), and `PROPERTIES` before one is named;
- the desk's panes are the Pane Manager's to list: no row here lists them.

WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-02 — Nothing in the pane multiplies a font metric

LAW — The pane counts in ROWS and COLUMNS: the rows and columns its area's room holds in the window's text measure; the fit from a face to a row count is made once, never by its own arithmetic.

MEANS
- 25 cells of body is 16 rows of an 18-pixel face and 25 rows of a cell medium, one grant.

WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-03 — The vertical window is `list_window`

LAW — `list_window`: a population that fits is shown whole, the focused row is always in the window, every omission is counted on its own side and spends a row; derived every publication, stored nowhere.

MEANS
- there is no scroll offset, no pane state field and no scroll gesture on the list.

WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-04 — The row maps are inverses, and a press names a row and not a cell

LAW — The row that must stay visible is the draft's, else the cursor's; `mark` records what each published row is and `placed` reads it back, so the pane answers a press with the row it composed.

MEANS
- a press is read back to the pane's OWN row and column, in the room it was aimed at;
- a heading, a section, a marker or a blank row means nothing.

WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-05 — A resting value is fitted and a live draft is windowed

LAW — A resting value is fitted with a mark where it was cut, because a committed value has no caret to say it moved; a live draft is windowed unmarked, and the window follows its caret.

MEANS
- at most one row is ever editing, and its caret and selection stand in it, moving no character.

WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-06 — A new room must not drop a live draft, and rows named anew must

LAW — A new room keeps the draft; a picture whose rows the host named anew ABANDONS it, saying so: carried onto another pane's or desk's row, it would write a weaver's text into another property.

MEANS
- draft identity is the host’s row identity and the drafted property’s label;
- a room, a moved value or a provider keeps the name; another pane, desk or set of settings does not, nor the row leaving or rejoining the desk;
- abandonment says so; a sent commit remains sent.

DOES NOT MEAN
- that a commit already sent is recalled — the host writes it only while its name holds.

WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-08 — The heading is reserved before the list is offered anything

LAW — The heading and a front sentence are subtracted from the granted rows before the list is windowed; a bound that grows when it is exceeded is not a bound, and `finish` truncates the rest.

MEANS
- the press inverse is measured from the same lead, so a sentence cannot move a press off its row.

WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-10 — An empty list says it is empty, whatever its room

LAW — With nothing inspected the list says so in its own words, behind no budget: the list is offered nothing then, so a guarded row would be missing in the one state the sentence exists for.

MEANS
- blankness cannot distinguish empty from broken, so the sentence says where a subject is named: the Pane Manager's `inspect in Info`;
- the granted room is still the wall: `finish` truncates and cannot be talked past.

WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-11 — The desk names this pane, and an office that does not offer leaves the row

LAW — `default_setup` authors this pane's reference, so a run whose plan loads no Info office keeps the row, reads it unresolved and counts it on the band; silence would not tell an absent office from a loss.

MEANS
- the desk's row is the INTENT and an office's offer is what resolves it;
- a plan row whose artifact is missing is the realization's to say (`agents/realization.md`).

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

## WL-INFO-13 — An ask Loom says never arrived is released, and silence still waits

LAW — Info keeps each ask's ticket: nothing queued, or Loom's own `zen.DispatchRefused` naming that attempt, releases that record and says so, closing no draft; a forged notice settles nothing.

MEANS
- the draft and its text stand, and the next Return is a fresh commit, not a second one;
- a notice counts only with Loom's provenance, then the attempt, correlation, shape and office;
- "nothing queued" is the kept ticket branch, source-traced.

DOES NOT MEAN
- that a delivered, unanswered ask is released: no timeout, retry, cancellation or polling.

WHY — `agents/decisions/an-undelivered-ask-is-released.md`

## WL-INFO-14 — The subject is a pane Info names, and nothing else moves it

LAW — Info names its pane-property subject; the host holds its rows. Selection, focus, Escape and switching views preserve it. Info may inspect itself.

MEANS
- an office asks Info to inspect a pane (the Pane Manager's `inspect in Info`); Info asks the host and answers the office, and a live draft holds the ask back, refused in Info's words;
- row identity covers the pane, the desk, whether it holds the row, and the settings rows; changing any renames the rows;
- an absent pane is refused without movement; an arriving inspector is answered; nothing is published about a subject until one is named.

DOES NOT MEAN
- that two inspectors each hold a subject: there is one slot, and an office that asks moves it.

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

### WL-INFO-01
`info/property_view.hpp` compose finish lead
`info/pane.cpp` say say_properties
`info/vocabulary.hpp` InfoPaneState
`workshop/pane_drawing.hpp` rows_drawing
`tests/test_workshop_panes_info.cpp` "the heading and the subject's properties are the pane's rows,
over the host's subject, and the panes are the Pane Manager's to list" "a room too short for the
body invents none of it"

### WL-INFO-02
`info/property_view.hpp` rows_ columns_ granted_ room
`info/pane.cpp` fit_room
`workshop/pane_drawing.hpp` RowsRoom RowsRoom::rows RowsRoom::columns
`graphics/vocabulary.hpp` AreaRoom
`tests/test_workshop_panes_info.cpp` "a room too short for the body invents none of it"

### WL-INFO-03
`info/property_view.hpp` list_window ListWindow say_properties
`workshop/screen_gestures.cpp` list_window omitted_text
`workshop/screen.hpp` ListWindow
`tests/test_workshop_panes_info.cpp` "what the body cannot show, it counts -- on the side it left it
out"

### WL-INFO-04
`info/property_view.hpp` placed Placed composed_ lead say_properties press press_placed
`info/pane.cpp` press
`workshop/pane_drawing.hpp` window_cell
`tests/test_workshop_panes_info.cpp` "a press on an Info row while a notice stands names the row
painted there, and a full room keeps its last row under the notice"

### WL-INFO-05
`info/property_view.hpp` say_properties begin_draft close_draft
`workshop/pane_text.hpp` fit pad
`component/text_box.hpp` TextBox::visible TextBox::keep_caret_visible
`tests/test_workshop_panes_info.cpp` "a draft on a value the weaver owns is written to the desk"
"Info draws its properties on its canvas on the medium's own ground, and a draft's caret and
selection stand in its row where the weaver types, moving no character, and go with it"

### WL-INFO-06
`info/property_view.hpp` shown shows_draft_subject end_draft close_draft Draft::subject draft_
`info/pane.cpp` on(PaneSubjectShown)
`workshop/inspection_seam_vocabulary.hpp` PaneSubjectShown::subject
`tests/test_workshop_panes_info.cpp` "a picture that names other rows abandons the Info draft and
says so, even where they have the same property on the same row, and writes nothing into either
desk" "an Info commit queued behind a restore of the desk from its file is refused, and a refused
restore keeps the draft" "an Info draft outlives a new room and its pane's window moving, and its
commit is written"

### WL-INFO-08
`info/property_view.hpp` compose lead finish front_sentence
`tests/test_workshop_panes_info.cpp` "a room too short for the body invents none of it" "a press on
an Info row while a notice stands names the row painted there, and a full room keeps its last row
under the notice"

### WL-INFO-10
`info/property_view.hpp` say_properties finish kNoSubject kNoSubjectRow
`tests/test_workshop_panes_info.cpp` "with nothing inspected, the properties say so and say what to
do next"

### WL-INFO-11
`workshop/setup.hpp` default_setup
`workshop/setup_reconcile.hpp` unresolved_panes
`workshop/panes.hpp` kInfoPaneProvider kInfoPaneKey
`workshop/screen_layouts.cpp` setup_rest_text
`tests/test_workshop_panes_info.cpp` "a Workshop with no Info OFFICE keeps the row and says so"

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
`info/pane.cpp` on(DispatchRefused)
`tests/test_workshop_panes_info.cpp` "an Info commit Loom refuses at dispatch is released: the draft
and its text stand, the next Return is written, and a cancel's promise is replaced" "an Info commit
a doorless office refuses at dispatch: the draft stands, the next commit tries again, and it is
written once the door is back" "a refusal notice anyone could send, naming the Info pane's
outstanding commit exactly, settles nothing" "an Info inspect Loom refuses at dispatch releases its
own record, answers the office that asked, and the next ask inspects"

### WL-INFO-14
`info/property_view.hpp` inspect_asked ask_inspect settle_relay relay_ kFinishTheEdit
`info/pane.cpp` on(InspectPaneRequested)
`workshop/inspection_seam_vocabulary.hpp` InspectPaneRequested PaneSubjectRequested PaneSubjectShown
`workshop/screen.hpp` InspectedPane pane_subject_rows pane_subject_shown
`workshop/screen_pane_subject.cpp` pane_subject_shown pane_subject_rows
`workshop/layout_shelf.hpp` SetupState::put_live
`workshop/weave_inspection.cpp` refresh_inspected on(InspectPaneRequested) on(PaneSubjectRequested)
publish_pane_subject
`tests/test_workshop_panes_info.cpp` "an office's ask inspects a pane through the host's own door,
and the office is answered what the host said" "two inspects asked in one burst: the older office is
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
`workshop/screen_pane_subject.cpp` write_pane_axis write_pane_setting
`tests/test_workshop_panes_info.cpp` "a draft on a value the weaver owns is written to the desk" "an
Info commit queued behind another desk put live, or another and back, is refused: neither desk is
written and the pane says why" "an Info commit whose image was replaced before its answer is not the
successor's: the successor holds no draft and says nothing of it, the subject stands, and the write
shows as the owner's rows"
