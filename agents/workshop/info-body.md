# Workshop law — the Info body

Register `WL-INFO`: the Info pane — its body, its subject, its draft. One law per heading; cite
by ID. Router: [`../workshop.md`](../workshop.md).

**Where this lives.** `info-pane/pane.cpp`, a weave granted a room that says rows into it: the
one inventory, and the pane a maker names there (`workshop/inspection_seam_vocabulary.hpp`). It
inspected the retired object document first; the same laws hold one subject over.

## WL-INFO-01 — The Info body is composed once, by the pane, into the room it was granted

LAW — `say` is the whole Info body — the headings, both lists, their sharing and the front sentence — composed in one pass over the granted room; nothing else in the image publishes rows.

MEANS
- one pass builds the rows and records where each landed, so the press inverse cannot drift.

PROVEN BY — `info-pane/pane.cpp` `say`, `finish`, `lead`; `info-pane/vocabulary.hpp`
`InfoPaneState`; `workshop/pane_vocabulary.hpp` `PaneRoom`, `PaneContent`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: the two headings and both lists are the
pane's rows, over the host's inventory and subject"`, case `"INFO-WEAVE: a room too short for the
body invents none of it"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-02 — Nothing in the pane multiplies a font metric

LAW — The pane is granted ROWS and COLUMNS and counts in them; the fit from a face to a row count is the host's, made once against the pane's rectangle, and no metric crosses the seam at all.

MEANS
- 25 cells of body is 16 rows of an 18-pixel face and 25 rows of a cell medium, one grant.

PROVEN BY — `info-pane/pane.cpp` `rows_`, `columns_`, `granted_`; `workshop/pane_vocabulary.hpp`
`PaneRoom::rows`, `PaneRoom::columns`; `workshop/weave_external.cpp` `refresh_external_rooms`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a room too short for the body invents none
of it"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-03 — The vertical window is `list_window`

LAW — `list_window`: a population that fits is shown whole, the focused row is always in the window, every omission is counted on its own side and spends a row; derived every publication, stored nowhere.

MEANS
- there is no scroll offset, no pane state field and no scroll gesture on either list.

PROVEN BY — `info-pane/pane.cpp` `list_window`, `ListWindow`, `say_panes`, `say_properties`;
`workshop/screen_gestures.cpp` `list_window`, `omitted_text`; `workshop/screen.hpp` `ListWindow`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: what the body cannot show, it counts -- on
the side it left it out"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-04 — The row maps are inverses, and a press names a row and not a cell

LAW — The row that must stay visible is the draft's, else the cursor's; `mark` records what each published row is and `placed` reads it back, so the pane answers a press with the row it composed.

MEANS
- a press crosses as the pane's OWN row and column, resolved to them by the host;
- a heading, a section, a marker or a blank row means nothing.

PROVEN BY — `info-pane/pane.cpp` `placed`, `Placed`, `composed_`, `lead`, `say_properties`;
`workshop/pane_vocabulary.hpp` `PanePressed::row`, `PanePressed::column`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a press on a pane row inspects it, through
the host's own door"`, case `"a press on an Info row while a notice stands names the row painted
there, and a full room keeps its last row under the notice"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-05 — A resting value is fitted and a live draft is windowed

LAW — A resting value is fitted with a mark where it was cut, because a committed value has no caret to say it moved; a live draft is windowed unmarked, and the window follows a caret that cannot cross.

MEANS
- at most one row is ever editing, and the caret itself does not cross the seam.

PROVEN BY — `info-pane/pane.cpp` `say_properties`, `begin_draft`, `close_draft`;
`workshop/pane_text.hpp` `fit`, `pad`; `component/text_box.hpp` `TextBox::visible`,
`TextBox::keep_caret_visible`; `tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a draft on
a value the maker owns is written to the desk"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-06 — A new room must not drop a live draft, and rows named anew must

LAW — A `PaneRoom` grant keeps the draft; a picture whose rows the host named anew ABANDONS it, saying so: carried onto another pane's or desk's row, it would write a maker's text into another property.

MEANS
- its subject is the host's name for the rows, then the label on the draft's row;
- a room, a moved value or a provider keeps the name; another pane, desk or row layout does not;
- the pane drops it unasked, so it says so, and that a commit it sent was already sent.

DOES NOT MEAN
- that a commit already sent is recalled — the host writes it only while its name holds.

PROVEN BY — `info-pane/pane.cpp` `on(PaneSubjectShown)`, `on(PaneRoom)`, `shows_draft_subject`,
`end_draft`, `close_draft`, `Draft::subject`, `draft_`; `workshop/inspection_seam_vocabulary.hpp`
`PaneSubjectShown::subject`; `tests/test_workshop_panes_info.cpp` case `"a picture that names
other rows abandons the Info draft and says so, even where they have the same property on the same
row, and writes nothing into either desk"`, case `"an Info commit queued behind a restore of the
desk from its file is refused, and a refused restore keeps the draft"`, case `"an Info draft
outlives a new room and its pane's window moving, and its commit is written"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-07 — `share_body_rows` is max-min fair sharing

LAW — `share_body_rows` is max-min fair: each list gets what it needs, spare stays spare, what both cannot have is shared equally with an unneeded half going to the other; 50/50 is a consequence.

MEANS
- growing the pane never shrinks either list.

PROVEN BY — `info-pane/pane.cpp` `share_body_rows`, `BodyShare`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: what the body cannot show, it counts -- on
the side it left it out"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-08 — The headings are reserved before either list is offered anything

LAW — The two headings and a front sentence are subtracted from the granted rows before the share is computed; a bound that grows when it is exceeded is not a bound, and `finish` truncates the rest.

MEANS
- the press inverse is measured from the same lead, so a sentence cannot move a press off its row.

PROVEN BY — `info-pane/pane.cpp` `say`, `lead`, `finish`, `front_sentence`; `workshop/weave.hpp`
`WorkshopWeave::judge_content`; `tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a room too
short for the body invents none of it"`, case `"a press on an Info row while a notice stands names
the row painted there, and a full room keeps its last row under the notice"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-09 — A pane row names the pane, and a press on it asks the host to inspect it

LAW — A pane row is its name and what the inventory says of it, cut at the granted columns; a press on one, or Return on the list cursor, ASKS the host to make it the subject unless a draft is live.

MEANS
- `*` marks the subject, `>` the list cursor where the keys are, `?` a cursor holding nothing;
- the list cursor is an identity: a pane that left the list is said, and Return inspects nothing;
- another subject takes a draft's rows, so a live draft refuses the press before asking.

PROVEN BY — `info-pane/pane.cpp` `say_panes`, `ask_inspect`, `inspect_cursor`,
`find_list_cursor`, `hold_list`, `on(PanePressed)`, `press_placed`, `kFinishTheEdit`;
`workshop/pane_text.hpp` `drawable`; `tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a
press on a pane row inspects it, through the host's own door"`, case `"INFO-WEAVE: a live draft
holds another subject back, and the reason is the maker's"`, case `"a press on a pane while an
Info draft is live is refused, keeping the draft, its text, the subject and the desk through a new
room, and inspecting resumes once the draft ends"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-10 — An empty list says it is empty, whatever its share

LAW — An empty list says so in its own words, and neither sentence is behind the share: an empty list is offered nothing, so a guarded row would be missing in the one state the sentence exists for.

MEANS
- a panel that merely goes blank is indistinguishable from a tool that has broken;
- the granted room is still the wall: `finish` truncates and cannot be talked past.

PROVEN BY — `info-pane/pane.cpp` `say_panes`, `say_properties`, `finish`, `kNoSubject`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: with nothing inspected, the properties say
so and say what to do next"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-11 — The desk names this pane, and an office that does not offer leaves the row

LAW — `default_setup` authors this pane's reference, so a run whose plan loads no Info office keeps the row, reads it unresolved and counts it on the band; silence would not tell an absent office from a loss.

MEANS
- the desk's row is the INTENT and an office's offer is what resolves it;
- a plan row whose artifact is missing is the realization's to say (`agents/realization.md`).

PROVEN BY — `workshop/setup.hpp` `default_setup`, `unresolved_panes`; `workshop/panel.hpp`
`kInfoPaneProvider`, `kInfoPaneKey`; `workshop/screen_layouts.cpp` `setup_rest_text`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a Workshop with no Info OFFICE keeps the
row and says so"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-12 — An answer is read against the ask, the draft and the text that asked

LAW — Info sends one commit at a time; its answer closes only the draft that sent it while that draft holds exactly what it sent, replaces only its own sentence, and no draft's end says nothing was written.

MEANS
- a commit asked while one is unanswered is not sent, aloud; the draft and its edits stand;
- `draft_epoch` names the draft and the sent text what a write covers: later typing stays open;
- an inspect keeps its own newest record, so it never hides a commit.

DOES NOT MEAN
- that closing a draft retracts a write, or that a delivered unanswered commit ends.

PROVEN BY — `info-pane/pane.cpp` `on(PaneSubjectActed)`, `act`, `answered_commit`, `ask_commit`,
`end_draft`, `Asked`, `SentCommit`, `SentCommit::draft`, `SentCommit::text`,
`SentCommit::promise`, `acting_`, `committing_`, `kCommitNotSent`; `component/text_box.hpp`
`TextBox::draft_epoch`; `tests/test_workshop_panes_info.cpp` case `"a commit and a cancel resolved
in one poll: the cancel says the commit was already sent, the answer's account takes that
sentence's place, and no row says nothing was written over a write"`, case `"an inspect asked
before an Info draft opened, and answered while it is open, closes nothing"`, case `"a second Info
commit in the same poll as the first is not sent: the pane says so, keeps the text typed between
them, sends that text once the first is answered, and a cancel after the write and a refused retry
claims no write away"`, case `"text typed after an Info commit was sent outlives that commit's
answer: the draft stays open with its history, the write is told apart from the unsent text, a
refusal is not said of the newer text, and the newer text commits normally"`.
WHY — `agents/decisions/a-paste-is-a-conversation.md`

## WL-INFO-13 — An ask Loom says never arrived is released, and silence still waits

LAW — Info keeps each ask's ticket: nothing queued, or Loom's own `zen.DispatchRefused` naming that attempt, releases that record and says so, closing no draft; a forged notice settles nothing.

MEANS
- the draft and its text stand, and the next Return is a fresh commit, not a second one;
- a notice counts only with Loom's provenance, then the attempt, correlation, shape and office.

DOES NOT MEAN
- that a delivered, unanswered ask is released: no timeout, retry, cancellation or polling.

PROVEN BY — `info-pane/pane.cpp` `on(DispatchRefused)`, `refused_ask`, `undelivered_commit`,
`ask_inspect`, `ask_commit`, `Asked::attempt`, `kCommitNotQueued`, `kCommitUndelivered`;
`tests/test_workshop_panes_info.cpp` case `"an Info commit Loom refuses at dispatch is released:
the draft and its text stand, the next Return is written, and a cancel's promise is replaced"`,
case `"an Info commit nothing could queue is released at once: the draft stands, the next commit
tries again, and it is written once the door is back"`, case `"a refusal notice anyone could send,
naming the Info pane's outstanding commit exactly, settles nothing"`, case `"an Info inspect Loom
refuses at dispatch releases its own record, and the next press inspects"`.
WHY — `agents/decisions/an-undelivered-ask-is-released.md`

## WL-INFO-14 — The subject is a pane Info names, and nothing else moves it

LAW — Info's subject is a pane named by Info's own ask and held by the host, whose rows they are; the selection, the keys, a press elsewhere and Escape never move it, and Info may name itself.

MEANS
- the host names the rows for one pane, desk and row layout, and anew when one of them moves;
- a pane nobody has is refused in words, nothing moved; an arriving inspector is answered;
- nothing is published about a subject until one is named.

DOES NOT MEAN
- that two inspectors each hold a subject: there is one slot, and an office that asks moves it.

PROVEN BY — `workshop/inspection_seam_vocabulary.hpp` `InspectPaneRequested`,
`PaneSubjectRequested`, `PaneSubjectShown`; `workshop/screen.hpp` `InspectedPane`,
`pane_subject_rows`, `pane_subject_shown`; `workshop/screen_pane_subject.cpp` `inspected_region`,
`pane_subject_shown`, `pane_subject_rows`; `workshop/setup.hpp` `SetupState::put_live`;
`workshop/weave_inspection.cpp` `refresh_inspected`, `on(InspectPaneRequested)`,
`on(PaneSubjectRequested)`, `publish_pane_subject`; `tests/test_workshop_panes_info.cpp` case
`"INFO-WEAVE: the subject is Info's to name: the keys leaving, Escape and a press elsewhere leave
it standing"`, case `"INFO-WEAVE: Info may inspect itself, and an edit to its own place is written
by the desk, reseats it and keeps the subject"`, case `"a subject naming a pane in neither this
build's vocabulary nor this desk is refused in words with nothing moved, and an inspector that
arrives is answered the picture as it is now"`.
WHY — `agents/decisions/an-inspector-names-its-subject.md`

## WL-INFO-15 — A property edit is the owner's write, through a door the desk already has

LAW — A commit names the rows, the row and the text; the host judges the name first, writes through the row's own setter — the setup's gesture or reset door, a definition's region door — and reseats.

MEANS
- a refusal is the owner's own sentence (a unit it does not read, a pane with no room);
- the band says what was written to which pane, read fresh from the row;
- an answer reaches only the incarnation that asked: a replaced Info is told nothing of it.

DOES NOT MEAN
- a general property editor: the rows are the ones the host's Pane Manager showed.

PROVEN BY — `workshop/inspection_seam_vocabulary.hpp` `PaneCommitRequested`, `PaneSubjectActed`;
`workshop/weave_inspection.cpp` `on(PaneCommitRequested)`; `workshop/weave.hpp`
`kPaneCommitSubjectGone`; `workshop/screen_pane_subject.cpp` `write_pane_axis`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a draft on a value the maker owns is
written to the desk"`, case `"an Info commit queued behind another desk put live, or another and
back, is refused: neither desk is written and the pane says why"`, case `"an Info commit whose
image was replaced before its answer is not the successor's: the successor holds no draft and says
nothing of it, the subject stands, and the write shows as the owner's rows"`.
WHY — `agents/decisions/an-inspector-names-its-subject.md`
