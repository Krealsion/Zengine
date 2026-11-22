# Workshop law — arrangement

Register `WL-ARR`: gestures, the resize law, the two arranging scopes, the coarse step, and
Escape; the snap is its second file, [`arrangement-snap.md`](arrangement-snap.md). One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

Retired: WL-ARR-01, WL-ARR-03, WL-ARR-04, WL-ARR-05, WL-ARR-07, WL-ARR-08, WL-ARR-09, WL-ARR-10, WL-ARR-11, WL-ARR-12, WL-ARR-18

## WL-ARR-02 — `end_held_gestures()` is the one release owner

LAW — Every branch that can see a release calls `end_held_gestures()`, because a gesture begun under one mode may be released under another; what to tell the weaver is the caller's.

MEANS
- in a graphics window the release comes to Workshop's own area, held there; a hold the window says it ended there ends here too (`AreaHoldEnded`).

DOES NOT MEAN
- that a gesture may end anywhere else — `end_held_gestures()` is the one release owner.

WHY — `agents/decisions/one-press-one-gesture.md`

## WL-ARR-06 — Independent axes settle independently, refuse-never-clamp

LAW — A left or top pull authors place and size as one axis-local transaction, a gesture blocked on one axis still settles the other, and an unchanged axis is no proposal, so it writes nothing.

MEANS
- `author_pane_window` is the one gesture door; a refused height never leaves a moved top edge;
- a refused single-axis step cannot author a reactive place as a side effect;
- each axis keeps its word: an anchor's offset from its edge; a share place, a share or a fill's far edge the share reaching the hand's pixel; a moved centred place and `fit` become pixels, said.

DOES NOT MEAN
- that a wall clamps an axis — a blocked axis is refused whole and keeps its authored value.

WHY — `agents/decisions/anchors-and-axes.md`

## WL-ARR-13 — Escape is back, not cancel, and its last meaning puts the pane down

LAW — After every mode, overlay and draft has answered, a bare Escape where a list or nothing holds the keys sheds `Panes::selected` and the keyboard candidate, moving nothing else.

MEANS
- every immediate-commit gesture is reversible only by its inverse; there is no undo;
- a desk whose panes cover every usable cell still reaches selection = none;
- it is not a keymap action: a recovery gesture must not be authorable into a lockout.

WHY — `agents/decisions/escape-is-back.md`

## WL-ARR-14 — A pane that judges its own Escape is sent it

LAW — A focused external pane whose holder lists `PaneEscapeUnspent` among what it emits is sent a bare Escape no row claims, and keeps it unless it says it was unspent; the editors keep every Escape.

WHY — `agents/decisions/escape-is-back.md`

## WL-ARR-15 — A pane may say the Escape it was sent was unspent, and is put down

LAW — `PaneEscapeUnspent{pane}` from the role that offered it, echoing the number its Escape was sent under, puts that pane down while it is still selected, still where the keys go, and still last.

MEANS
- what is shed is `unselect_pane`'s two lines: nothing closes, and no arrangement moves;
- the default a pane gets is to drop its selection first, then say the word (`pane_escape`);
- zero, another Escape's number, a spent one, or a later gesture of its hand: the word moves nothing.

DOES NOT MEAN — a consumption protocol: it is Escape's own last meaning, asked for by the party
that knows whether Escape meant anything there.

WHY — `agents/decisions/escape-is-back.md`

## WL-ARR-16 — An Escape no holder could take crosses as nothing, and the last meaning answers

LAW — A bare Escape no pane row claims is not sent when the role's holder accepts no `PaneKey` or declares no `PaneEscapeUnspent`; nothing crosses, and the selection is shed as in command mode.

MEANS
- the accept-set and emit-set are the holder's declarations, read off the bus at the keystroke;
- a host that cannot answer which doors a holder has sends the key exactly as before;
- a pane that declared a row on Escape is sent the resolved id and owns it (WL-KEY-15).

WHY — `agents/decisions/escape-is-back.md`

## Do not assume

- That Escape with a pane selected does nothing, or closes the pane — it puts the selection
  down, last, and closes, moves, ranks and writes nothing (WL-ARR-13).
- That a focused pane's Escape reaches Workshop by itself — a pane that judges its own Escape
  keeps it until it says the Escape was unspent, and one that does not judge it is never sent it
  (WL-ARR-15, WL-ARR-16).

## Proven by

### WL-ARR-02
`workshop/weave_pointer.cpp` end_held_gestures
`tests/test_workshop_screen.cpp` "a release ends a pane gesture whatever mode sees it"
`workshop/weave_handlers.cpp` on(AreaHoldEnded)
`tests/test_workshop_desk.cpp` "a press the graphics window gives Workshop's own area is the press it is, held to its own
release, and one it gives a pane's area is the pane's, Workshop told where it landed" "a press Workshop holds in its own
area of the window ends when the window says it ended the hold, and not before: an area Workshop
places while it holds is the window's to judge"

### WL-ARR-06
`workshop/setup.hpp` author_pane_window check_pane_place_coord PaneAxisProposal share_reaching share_placing
WindowWritten
`desk-gestures/gestures.cpp` pane_window_proposal
`desk-gestures/gestures.hpp` PaneWindowProposal
`workshop/screen_gestures.cpp` pane_window_axes
`workshop/screen.hpp` pane_window_at
`workshop/weave_arrange.cpp` arrange_place arrange_resize arrange_window write_window
`tests/test_workshop_screen.cpp` "a refused anchored resize writes neither the place nor the size"
"a move blocked at the left wall still follows the hand down" "a move past two walls at once writes
nothing" "a refused nudge does not author a reactive place" "a corner resize blocked on one axis
still resizes the other" "a hand pulling a share's edge writes the share whose edge is the hand's
pixel, at every pixel of the widest room" "a fill pulled at its far edge, with nothing sharing its
frame, writes the share whose edge is the hand's pixel, and says so" "a hand moves an anchored pane
in its word, a centred one into pixels, and pulls a fitted size into pixels, each said"

### WL-ARR-13
`workshop/weave_external.cpp` unselect_pane
`workshop/screen_arrange.cpp` default_row_context
`tests/test_workshop_host.cpp` "Escape clears the ordinary selection last, and the inspected subject
stands" "every more-specific Escape meaning answers first, and deselection waits" "a desk with no
unoccupied cell still reaches selection = none"

### WL-ARR-14
`workshop/weave_external.cpp` unselect_pane
`workshop/screen_arrange.cpp` default_row_context keyboard_context
`tests/test_workshop_panes_input.cpp` "a focused external pane keeps Escape; a press on a pane that
takes no text, then Escape, puts the selection down"
`tests/test_workshop_panes_editor.cpp` "Escape means nothing in the Editor -- no mode closes, no
text moves"

### WL-ARR-15
`pane/vocabulary.hpp` PaneEscapeUnspent
`workshop/weave.hpp` WorkshopWeave::gestures_ WorkshopWeave::Hand::escape_sent
WorkshopWeave::GestureSent WorkshopWeave::gesture_asks_
`workshop/weave_external.cpp` on(PaneEscapeUnspent) WorkshopWeave::external_key
`pane/escape.hpp` answer unspent bare
`terminal/pane.cpp` composing_nothing
`tests/test_workshop_panes_input.cpp` "a pane that takes keys keeps Escape until it says the Escape
was unspent" "a word about an Escape moves nothing when it is stale or anonymous or about another
pane" "an answer to an Escape that is over cannot borrow the next Escape's identity" "only an Escape
is sent under a number, and a pane may answer the one it holds"
`tests/test_workshop_panes_terminal.cpp` "Escape sheds the list then the line then the pane itself
and moves nothing else" "an unspent Escape the weaver has already typed past moves nothing" "a
second Escape does not lend its identity to the first Escape's answer"

### WL-ARR-16
`workshop/weave_external.cpp` external_key holder_emits_on
`workshop/weave_handlers.cpp` on(KeyPressed)
`workshop/weave.hpp` HostContext::holder_accepts HostContext::holder_emits
`tests/test_workshop_panes_input.cpp` "a pane that never mentions Escape is put down by it, and is
sent nothing" "every shipped pane that holds the keys and selects nothing hands a bare Escape back"
`tests/test_workshop_inventory_info.cpp` "escape: Loaded and Powers let go of the row a weaver chose
first, and are put down next"
`tests/test_workshop_panes_attention.cpp` "a pane whose holder has no door for a key is put down by
Escape, and nothing is sent"
