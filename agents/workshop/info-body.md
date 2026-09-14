# Workshop law — the Info body

Register `WL-INFO`: the Info pane's body, composed once, in the image that owns it. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

**Where this body lives.** It was `workshop/screen_info.cpp`'s `paint_info` and eighteen
helpers, painting a built-in panel into a region the host resolved. It is
`info-pane/pane.cpp`'s now: a loadable weave that is GRANTED a room in rows and columns and
SAYS rows into it. The composition did not change its meaning — the two headings, the fair
share, the windows, the omission markers, the fitted rows — and every law below is the same
law one image over. What did change is that no font metric, no rectangle and no canvas is
reachable from the party that composes, which is why several laws below got shorter.

## WL-INFO-01 — The Info body is composed once, by the pane, into the room it was granted

LAW — `say` is the whole Info body — the headings, both lists, their sharing, the controls and the notice — composed in one pass over the granted room; nothing else in the image publishes rows.

MEANS
- one pass builds the rows and records where each landed, so the press inverse cannot drift;
- a room too small ends the pass early rather than inventing rows to fill it.

PROVEN BY — `info-pane/pane.cpp` `say`, `finish`, `lead`; `info-pane/vocabulary.hpp`
`InfoPaneState`; `workshop/pane_vocabulary.hpp` `PaneRoom`, `PaneContent`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: the two headings and both lists are
the pane's rows, over the host's document"`, case `"INFO-WEAVE: a room too short for the body
invents none of it"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-02 — Nothing in the pane multiplies a font metric

LAW — The pane is granted ROWS and COLUMNS and counts in them; the fit from a face to a row count is the host's, made once against the pane's rectangle, and no metric crosses the seam at all.

MEANS
- 25 cells of body is 16 rows of an 18-pixel face and 25 rows of a cell medium, one grant;
- a pane that measured type would be a second answer to a question the host already answered.

PROVEN BY — `info-pane/pane.cpp` `rows_`, `columns_`, `granted_`;
`workshop/pane_vocabulary.hpp` `PaneRoom::rows`, `PaneRoom::columns`;
`workshop/weave_external.cpp` `refresh_external_rooms`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a room too short for the body invents
none of it"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-03 — The vertical window is `list_window`

LAW — `list_window`: a population that fits is shown whole, the focused row is always in the window, every omission is counted on its own side and spends a row; derived every publication, stored nowhere.

MEANS
- there is no scroll offset, no pane state field and no scroll gesture on either list;
- two packages carry this arithmetic and two is a convention, not a defect.

PROVEN BY — `info-pane/pane.cpp` `list_window`, `ListWindow`, `say_objects`,
`say_properties`; `workshop/screen_gestures.cpp` `list_window`, `omitted_text`;
`workshop/screen.hpp` `ListWindow`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: what the body cannot show, it counts
-- on the side it left it out"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-04 — The row maps are inverses, and a press names a row and not a cell

LAW — The row that must stay visible is the draft's, else the cursor's; `mark` records what each published row is and `placed` reads it back, so the pane answers a press with the row it composed.

MEANS
- a press crosses as the pane's OWN row and column, resolved to them by the host;
- a heading, a marker, a blank row or the space below the last control means nothing.

PROVEN BY — `info-pane/pane.cpp` `placed`, `Placed`, `composed_`, `lead`, `say_properties`;
`workshop/pane_vocabulary.hpp` `PanePressed::row`, `PanePressed::column`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a press on an object row selects it,
through the document's own door"`, case `"INFO-WEAVE: pressing Create is the SAME operation
the `n` key performs"`, case `"a press on an Info row while a notice stands names the row
painted there, and a full room keeps both controls under the notice"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-05 — A resting value is fitted and a live draft is windowed

LAW — A resting value is fitted with a mark where it was cut, because a committed value has no caret to say it moved; a live draft is windowed unmarked, and the window follows a caret that cannot cross.

MEANS
- at most one row is ever editing: a draft opens on the cursor's row and closes first;
- the caret itself does not cross the seam, which is this migration's own named loss.

PROVEN BY — `info-pane/pane.cpp` `say_properties`, `begin_draft`, `close_draft`;
`workshop/pane_text.hpp` `fit`, `pad`; `component/text_box.hpp` `TextBox::visible`,
`TextBox::keep_caret_visible`; `tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a draft
on a value the maker owns is written to the document"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-06 — A new room must not drop a live draft, and a new document must

LAW — A `PaneRoom` grant keeps the draft; a `DocumentShown` that stops showing its property ABANDONS it, saying so: a draft carried onto another object's row writes a maker's text into another property.

MEANS
- its property is the object AND the label on the draft's row: another object's row is not it;
- the pane drops it unasked, so it says so, and whether a commit it sent may still be written.

DOES NOT MEAN
- that a commit already sent is recalled — the host commits against the rows it holds then.

PROVEN BY — `info-pane/pane.cpp` `on(DocumentShown)`, `on(PaneRoom)`, `shows_draft_subject`,
`end_draft`, `close_draft`, `Draft::object`, `draft_`;
`workshop/document_seam_vocabulary.hpp` `DocumentShown::properties`, `DocumentShown::selected`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a draft opens on the cursor's row,
declares two ids and no more, and commits through the document"`, case `"a picture that selects
another object abandons the Info draft and says so, even where that object has the same property
on the same row, and writes nothing into either object"`, case `"a press on an object while an
Info draft is live is refused in the controls' words, keeping the draft, its text, the selection
and the document through a new room, and selecting resumes once the draft ends"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-07 — `share_body_rows` is max-min fair sharing

LAW — `share_body_rows` is max-min fair: each list gets what it needs, spare stays spare, what both cannot have is shared equally with an unneeded half going to the other; 50/50 is a consequence.

MEANS
- growing the pane never shrinks either list;
- the host keeps its own copy because the Pane Manager spends the same share.

PROVEN BY — `info-pane/pane.cpp` `share_body_rows`, `BodyShare`;
`workshop/screen_info.cpp` `share_body_rows`; `workshop/screen.hpp` `BodyShare`,
`list_demand`; `tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: what the body cannot
show, it counts -- on the side it left it out"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-08 — The headings are reserved before either list is offered anything

LAW — The headings, the controls and a notice row are subtracted from the granted rows before the share is computed; a bound that grows when it is exceeded is not a bound, and `finish` truncates the rest.

MEANS
- the press inverse is measured from the same lead, so a notice cannot move a press off its row.

PROVEN BY — `info-pane/pane.cpp` `say`, `lead`, `finish`, `kActionCount`;
`workshop/weave.hpp` `WorkshopWeave::judge_content`; `tests/test_workshop_panes_info.cpp` case
`"INFO-WEAVE: a room too short for the body invents none of it"`, case `"INFO-WEAVE: the two
controls are the last rows of the body, and say their own availability in characters"`, case
`"a press on an Info row while a notice stands names the row painted there, and a full room
keeps both controls under the notice"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-09 — An object row is fitted whole, and a press on it asks the document

LAW — An object row puts the identity before the name, cut at the granted columns; a press on one ASKS the host to select unless a draft is live, and learns the answer as a picture, not by moving it.

MEANS
- a name longer than the column is marked, and the document still holds all of it;
- an undrawable byte is replaced rather than sent, because a publication is judged whole;
- a select changes the rows a draft is typed into, so a live draft refuses it first (WL-CTRL-03).

PROVEN BY — `info-pane/pane.cpp` `say_objects`, `ask_select`, `on(PanePressed)`,
`press_placed`, `kFinishTheEdit`; `workshop/pane_text.hpp` `drawable`;
`workshop/document_seam_vocabulary.hpp` `kDocumentSelect`; `tests/test_workshop_panes_info.cpp`
case `"INFO-WEAVE: a press on an object row selects it, through the document's own door"`, case
`"INFO-WEAVE: an object name a canvas cannot draw is still shown"`, case `"a press on an object
while an Info draft is live is refused in the controls' words, keeping the draft, its text, the
selection and the document through a new room, and selecting resumes once the draft ends"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-10 — An empty list says it is empty, whatever its share

LAW — An empty list says so in its own words, and neither sentence is behind the share: an empty list is offered nothing, so a guarded row would be missing in the one state the sentence exists for.

MEANS
- a panel that merely goes blank is indistinguishable from a tool that has broken;
- the granted room is still the wall: `finish` truncates and cannot be talked past.

PROVEN BY — `info-pane/pane.cpp` `say_objects`, `say_properties`, `finish`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: an empty document says it is empty and
says what to do next"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-11 — The desk names this pane, and an office that does not offer leaves the row

LAW — `default_setup` authors this pane's reference, so a run whose plan loads no Info office keeps the row, reads it unresolved and counts it on the band; silence would not tell an absent office from a loss.

MEANS
- the desk's row is the INTENT and an office's offer is what resolves it;
- ⚠ a MISSING ARTIFACT is a different failure: a plan is all-or-nothing, so the host exits;
- `1 unresolved` is on the band for the frames before that refusal arrives.

PROVEN BY — `workshop/setup.hpp` `default_setup`, `unresolved_panes`; `workshop/panel.hpp`
`kInfoPaneProvider`, `kInfoPaneKey`; `workshop/screen_layouts.cpp` `setup_rest_text`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a Workshop with no Info OFFICE keeps
the row and says so"`, case `"INFO-WEAVE: the pane arrives by a plan row and resolves a row the
desk already had"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-12 — An answer is read against the act, the draft and the text that asked

LAW — Info sends one commit at a time; its answer closes only the draft that sent it while that draft holds exactly what it sent, replaces only its own sentence, and no draft's end says nothing was written.

MEANS
- a commit asked while one is unanswered is not sent, aloud; the draft and its edits stand;
- `draft_epoch` names the draft and the sent text what a write covers: later typing stays open;
- a select, a create or a delete keeps its own newest record, so none of them hides a commit.

DOES NOT MEAN
- that closing a draft retracts a write, or that the pane knows what object a late commit reached;
- that an answer replaces a later act's sentence, or that an unanswered commit is ever given up.

PROVEN BY — `info-pane/pane.cpp` `on(DocumentActed)`, `act`, `answered_commit`, `ask`,
`end_draft`, `Asked`, `SentCommit`, `SentCommit::draft`, `SentCommit::text`,
`SentCommit::promise`, `acting_`, `committing_`, `kCommitNotSent`; `component/text_box.hpp`
`TextBox::draft_epoch`; `tests/test_workshop_panes_info.cpp` case `"a commit and a cancel
resolved in one poll: the cancel says the commit was already sent, the answer's account takes that
sentence's place, and no row says nothing was written over a write"`, case `"an Info commit
answered after a newer draft opened on the same field closes, alters and marks nothing on that
draft, and a sentence a later act said stands"`, case `"an Info draft ended while an earlier
draft's commit is still unanswered says the commit was already sent, even when an act between them
asked the document something else, and that commit's account replaces the sentence"`, case `"a
select asked before an Info draft opened, and answered while it is open, closes nothing"`, case
`"a second Info commit in the same poll as the first is not sent: the pane says so, keeps the text
typed between them, sends that text once the first is answered, and a cancel after the write and a
refused retry claims no write away"`, case `"Return twice over an unchanged Info draft sends one
commit, says the second was not sent while the first is unanswered, and the first's acceptance
closes the draft and retires that sentence"`, case `"text typed after an Info commit was sent
outlives that commit's answer: the draft stays open with its history, the write is told apart from
the unsent text, a refusal is not said of the newer text, and the newer text commits normally"`,
case `"an Info commit's answer settles the sentence that said a second commit was not sent, and
leaves a sentence a later act said standing"`, case `"a commit from a newer Info draft is not sent
while an earlier draft's commit is unanswered, even with a select asked between them, and the
earlier commit's account replaces that sentence without closing or altering the newer draft"`.
WHY — `agents/decisions/a-paste-is-a-conversation.md`
