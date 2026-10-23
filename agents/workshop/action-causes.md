# Workshop law — what a press may cause, the modes, and whether a control applies

Register `WL-CAUSE`: what a pane says of its actions beside their keys -- the shapes a press may
send, the mode its rows are in force for and the other modes' causes, and whether a control it
draws applies now -- and what holds that true. The keys are [`keyboard.md`](keyboard.md)
(WL-KEY-15) and the drawn controls [`pane-controls.md`](pane-controls.md). One law per heading;
cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-CAUSE-01 — A row says what its press may cause, and Workshop holds that with the row

LAW — A row of `v3::PaneActions` or `v2::AppActions` names each shape a press may send beside the pane protocol; Workshop judges them with the keys, refuses the declaration whole, or holds them with its rows.

MEANS
- a send that waits on an answer is the press's; one that waits on another gesture, a menu's choice, is not;
- a subject is empty (the pane's own state), `on <prefix>` (the part pressed) or `to <office>/<pane>`;
- an earlier version is widened with none, and Workshop resolves nothing: a reader joins the rows with the desk.

DOES NOT MEAN
- that a field of the message is declared: which recipe a build names is the pane's state at the press;
- that a cause is a promise: "may" -- a press in another state sends less, or nothing.

WHY — `agents/decisions/a-press-says-what-it-may-cause.md`

## WL-CAUSE-02 — A declaration names its mode, the mode a row enters, and what the others cause

LAW — A declaration names the `mode` its rows are in force for, each row the mode it `enters`, and in `elsewhere` what each other mode may cause, never its rows; Workshop judges and holds them.

MEANS
- a reader tells "another mode can, after this row" from "nothing here can";
- only the rows in force are dispatched: another mode's causes are no keys;
- a pane declaring its rows by mode from one function says `elsewhere` from it too (`causes_elsewhere`).

DOES NOT MEAN
- that a pane with one mode must name it: an empty mode with nothing elsewhere is one mode.

WHY — `agents/decisions/a-press-says-what-it-may-cause.md`

## WL-CAUSE-03 — A part says whether its operation applies now, and its reading carries it

LAW — A part says `available` as the pane draws the face, `[label]` true, `(label)` false; it rides the drawing's record (`PartFace`), as the window reads it (`AreaPart`), to each `PanePart`.

MEANS
- a part a record says nothing of, and a part Workshop names itself, reads available;
- availability is part of what the area shows: a change of it moves the fingerprint and stamp.

DOES NOT MEAN
- that available is permission: the operation asks again when the press arrives (WL-HAND-01).

WHY — `agents/decisions/a-press-says-what-it-may-cause.md`

## WL-CAUSE-04 — A shipped pane's declaration is held true by its census, a press and its manual

LAW — The census holds each row's causes and mode entered, a press of each action sends nothing beside the pane protocol its row does not declare, and each manual command's Answers line links its causes.

MEANS
- every row each office's census pins is pressed, on a pane of its own in the state its suite drives;
- a declared cause no driven state sent is listed beside the build, never failed: "may";
- the manual check reads only which shapes a line links, never its words.

WHY — `agents/decisions/a-press-says-what-it-may-cause.md`

## Do not assume

- That a row with no causes causes nothing: an earlier version's says nothing either way
  (WL-CAUSE-01).
- That "nowhere in force" is "nowhere": another mode's causes are in `elsewhere` (WL-CAUSE-02).
- That parentheses are the fact: the part says `available` (WL-CAUSE-03).

## Proven by

### WL-CAUSE-01
`workshop/pane_vocabulary.hpp` ActionCause v3::PaneActionRow v3::PaneActions kMaxActionCauses
`workshop/desktop_seam_vocabulary.hpp` v2::AppActionRow v2::AppActions
`workshop/pane_causes.hpp` action_cause_problem action_causes_problem pane_declaration_problem
cause_of on_part to_pane action_row_of
`workshop/keymap.hpp` AppRow::causes
`workshop/weave_seam.cpp` on(PaneActions) declare_pane_actions
`workshop/weave_desktop.cpp` on(AppActions) app_rows_of
`tests/test_workshop_panes_actions.cpp` "WL-CAUSE-01, WL-CAUSE-02: what a declaration says beside
its keys is judged by its form -- each cause, the mode in force, the other modes and the mode a row
enters" "WL-CAUSE-01, WL-CAUSE-02: Workshop holds a declaration's causes, its mode and what the
other modes cause with its rows, widens an earlier version with none, and refuses a malformed one
whole, the rows in force standing"

### WL-CAUSE-02
`workshop/pane_vocabulary.hpp` ModeCauses kMaxPaneModes kMaxModeCauses kMaxPaneModeLen
`workshop/pane_causes.hpp` pane_mode_problem pane_declaration_problem causes_elsewhere
`workshop/panes.hpp` RuntimePane::mode RuntimePane::elsewhere
`builder/builder-pane/pane.cpp` rows_of mode declare
`workshop/desktop-pane/pane.cpp` keys_rows keys_mode declare_keys
`tests/test_workshop_panes_actions.cpp` "WL-CAUSE-01, WL-CAUSE-02: what a declaration says beside
its keys is judged by its form -- each cause, the mode in force, the other modes and the mode a row
enters"
`tests/test_workshop_panes_builder.cpp` "the action census: the rows the Builder declares in each
mode, with what each press causes and the mode it enters, and the desk's, are tests/actions.txt"

### WL-CAUSE-03
`graphics/vocabulary.hpp` PartFace AreaPart
`workshop/pane_view.hpp` PanePart PaneView PaneViewRequested
`workshop/pane_drawing.hpp` part_available rows_drawing
`workshop/pane_canvas_drawing.hpp` canvas_sketch
`workshop/pane_parts.hpp` row_parts
`workshop/weave_desk_read.cpp` on(PaneViewRequested) shown_words_and_parts shown_fingerprint
`builder/builder-pane/pane.cpp` BuilderMeaning::available
`tests/test_workshop_panes_builder.cpp` "WL-CAUSE-03: a control the Builder draws in brackets reads
available in its reading, and one drawn in parentheses reads unavailable -- the pane's word, not the
drawing"
`tests/test_workshop_panes_canvas.cpp` "a weaver keys a described view's button by naming its id in
their keymap file: the key says the button's intent, the keymap says what it causes, and its part is
the action's"

### WL-CAUSE-04
`tests/census.hpp` pane_protocol SendsOf PressWitness PressDriverOf press_each_state
`tests/test_workshop_panes_builder.cpp` list_unreached witness_presses "the action census: the rows the Builder declares in each
mode, with what each press causes and the mode it enters, and the desk's, are tests/actions.txt"
"the press witness: each action the Builder declares, pressed in each mode it is driven into, sends
nothing beside the pane protocol that its row does not declare" "the press witness: each row the
desktop declares, pressed, sends nothing beside the pane protocol that the row does not declare --
the application's, the Pane Manager's and the Manual's" "the press witness: every other office in
the census, each row its panes declare pressed in each mode its suite drives, sends nothing beside
the pane protocol that the row does not declare"
`tests/check_manual.cmake` zen_manual_census zen_manual_answers
