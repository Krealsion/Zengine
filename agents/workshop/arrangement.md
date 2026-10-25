# Workshop law — arrangement

Register `WL-ARR`: gestures, the resize law, the two arranging scopes, the coarse step, and
Escape; the snap is its second file, [`arrangement-snap.md`](arrangement-snap.md). One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-ARR-01 — One press claims one gesture until release

LAW — `PaneGesture` holds an identity, an edge, the press's size and its snap's lines — no rectangle, no live position — so every motion proposes `base + (pointer - press)` and nothing crossed moves it.

MEANS
- crossing another pane, the Terminal or a reorder changes nothing about who is being moved;
- outside arrangement an addressed pane behind another claims no press and no address auto-raises;
- each motion is interpreted; unchanged geometry, address, gesture and notice need no new picture.

WHY — `agents/decisions/one-press-one-gesture.md`

## WL-ARR-02 — `end_held_gestures()` is the one release owner

LAW — Every branch that can see a release calls `end_held_gestures()`, because a gesture begun under one mode may be released under another; what to tell the weaver is the caller's.

MEANS
- in a graphics window the release comes to Workshop's own area, held there; a hold the window says it ended there ends here too (`AreaHoldEnded`).

DOES NOT MEAN
- that a gesture may end anywhere else — `end_held_gestures()` is the one release owner.

WHY — `agents/decisions/one-press-one-gesture.md`

## WL-ARR-03 — `forget_removed_selection()` clears on membership, never on presentation

LAW — A pane that becomes covered, off-room or unresolved stays addressed; a reference leaving the setup clears the address and its gesture, and closes the one-pane scope.

MEANS
- the desk stays open, its subject being the desk;
- it runs inside `apply_setup`, the one door membership changes through.

WHY — `agents/decisions/one-press-one-gesture.md`

## WL-ARR-04 — A resize begins from the resolved window

LAW — A first edit measures from `managed_bounds().resolved`, the unclipped ask captured at the press; `rect` is the visible intersection and owns painting, occupancy, coverage and handles.

MEANS
- a default pane resolving to 89 cells with four on screen answers one rightward step by five.

WHY — `agents/decisions/anchors-and-axes.md`

## WL-ARR-05 — Every resize edge preserves its opposite anchor

LAW — The pulled edge follows the hand and the opposite edge holds still; a corner holds the corner across from it; right and bottom pulls leave a default place reactive by not writing it.

WHY — `agents/decisions/anchors-and-axes.md`

## WL-ARR-06 — Independent axes settle independently, refuse-never-clamp

LAW — A left or top pull authors place and size as one axis-local transaction, a gesture blocked on one axis still settles the other, and an unchanged axis is no proposal, so it writes nothing.

MEANS
- `author_pane_window` is the one gesture door; a refused height never leaves a moved top edge;
- a refused single-axis step cannot author a reactive place as a side effect;
- each axis keeps its word: an anchor's offset from its edge, a share (or a fill pulled at its far edge) the share reaching the hand's pixel; a moved centred place and `fit` become pixels, said.

DOES NOT MEAN
- that a wall clamps an axis — a blocked axis is refused whole and keeps its authored value.

WHY — `agents/decisions/anchors-and-axes.md`

## WL-ARR-07 — Arrangement is two scopes and one vocabulary

LAW — The one-pane scope is bound to exactly its pane — admission before binding through `arrange_geometry_ready` — and the desk opens with no pane addressed, every pane answering the pointer.

MEANS
- `PaneArrange{open, desk, pane, resetting}` replaced the selector with submodes;
- in the one-pane scope a press elsewhere is consumed with the sentence naming the state;
- on the desk a press takes hold and makes that pane the keyboard's target, topmost first.

WHY — `agents/decisions/two-arranging-scopes.md`

## WL-ARR-08 — The arranging keys are one vocabulary in both scopes

LAW — Arrows place one cell, shift+arrows pull the extent one cell anchored at the place, `=`/`-` the coarse step, `f`/`b`/`r`/`l` order, `d` remove, `0` the reset prompt, esc leave.

MEANS
- the desk adds `tab`/`shift+tab` over `arrangeable()` — every setup row — and Return;
- a hand and a key author the same setup values, each axis in its word; escape unwinds one level and rolls nothing back.

WHY — `agents/decisions/two-arranging-scopes.md`

## WL-ARR-09 — Arranging a pane is choosing it, and the rings are its statement

LAW — `enter_arrange_pane` writes `Panes::selected` from the addressed reference after admission and nothing else; the state's visible statement is the rings, the legend and the notice.

MEANS
- rings: accent in the one-pane scope; over the desk muted, with accent on the target;
- `arrange_status()` carries the pane's state word, so an invisible pane is recoverable by ear.

WHY — `agents/decisions/two-arranging-scopes.md`

## WL-ARR-10 — The coarse step is the fine step with a bigger delta

LAW — The coarse step resizes the addressed pane by the coarse count on both axes through the ordinary resize seam: one proposal at the bottom-right edge, into the one gesture door, and no second owner.

MEANS
- it meets the identical per-axis settlement a shifted arrow meets, and there is no second owner;
- it cannot move the pane it resizes, moves no other pane, and measures no content;
- the coarse count is `workshop.arrange-step`, `kCoarseStepCells` until a weaver sets it.

WHY — `agents/decisions/why-the-coarse-step-is-four.md`

## WL-ARR-11 — Four is the default, and the default is pinned

LAW — A `static_assert` holds `kStackRows + kCoarseStepCells - 2*kChromeCells - 1 >= 8`, so at the defaults one grow gives a stack pane's body the eight rows the Compose form needs.

DOES NOT MEAN
- that a weaver's settings keep it: a smaller `workshop.arrange-step` or `workshop.stack-rows` is theirs to choose.

WHY — `agents/decisions/why-the-coarse-step-is-four.md`

## WL-ARR-12 — The coarse step is ordinary action vocabulary

LAW — `manage.grow`/`manage.shrink` are catalog rows declared in both scopes, so one override moves both; the legend and the effective keymap say them, and no pane's chrome paints them.

WHY — `agents/decisions/why-the-coarse-step-is-four.md`

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

LAW — `PaneEscapeUnspent{pane}` from the office that offered it, echoing the number its Escape was sent under, puts that pane down while it is still selected, still where the keys go, and still last.

MEANS
- what is shed is `unselect_pane`'s two lines: nothing closes, and no arrangement moves;
- the default a pane gets is to drop its selection first, then say the word (`pane_escape`);
- zero, another Escape's number, a spent one, or a later gesture of its hand: the word moves nothing.

DOES NOT MEAN — a consumption protocol: it is Escape's own last meaning, asked for by the party
that knows whether Escape meant anything there.

WHY — `agents/decisions/escape-is-back.md`

## WL-ARR-16 — An Escape no holder could take crosses as nothing, and the last meaning answers

LAW — A bare Escape no pane row claims is not sent when the office's holder accepts no `PaneKey` or declares no `PaneEscapeUnspent`; nothing crosses, and the selection is shed as in command mode.

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

### WL-ARR-01
`workshop/screen.hpp` PaneGesture Session::pane_drag kPaneEdgeBandPx
`workshop/screen_arrange.cpp` pane_edge_at
`workshop/weave_arrange.cpp` take_pane_hold arrange_motion commit_drag
`workshop/weave_pointer.cpp` pointer_moved
`tests/test_workshop_panes_window.cpp` "one press claims one gesture, and crossing anything does not
move it" "outside arrangement, an addressed pane behind another clicks through nothing" "unchanged
arrangement motion retains its gesture without another picture" "repeated refused arrangement motion
retains the refusal and can recover" "queued arrangement motions retain order through release"

### WL-ARR-02
`workshop/weave_pointer.cpp` end_held_gestures
`tests/test_workshop_screen.cpp` "a release ends a pane gesture whatever mode sees it"
`workshop/weave_handlers.cpp` on(AreaHoldEnded)
`tests/test_workshop_desk.cpp` "a press the graphics window gives Workshop's own area is the press it is, held to its own
release, and one it gives a pane's area is the pane's, Workshop told where it landed" "a press Workshop holds in its own
area of the window ends when the window says it ended the hold, and not before: an area Workshop
places while it holds is the window's to judge"

### WL-ARR-03
`workshop/weave_arrange.cpp` forget_removed_selection
`workshop/weave_session.cpp` apply_setup
`workshop/screen.hpp` PaneArrange
`tests/test_workshop_screen.cpp` "a removed target leaves no stale selection, submode or heading"
"removing the pane being arranged ends the arrangement about it"
`tests/test_workshop_panes_window.cpp` "clearing the selected pane clears its gesture safely"

### WL-ARR-04
`workshop/weave_arrange.cpp` managed_bounds managed_window_base
`workshop/screen.hpp` PaneBounds PaneBounds::rect
`workshop/screen_gestures.cpp` pane_window_proposal
`tests/test_workshop_screen.cpp` "a clipped default resize begins from the full resolved size"

### WL-ARR-05
`workshop/screen_gestures.cpp` pane_window_proposal
`workshop/screen.hpp` pane_edge::kBottomRight PaneWindowProposal
`workshop/setup.hpp` author_pane_window
`workshop/weave_arrange.cpp` arrange_resize arrange_window write_window
`tests/test_workshop_screen.cpp` "every edge resizes pixel-fine and preserves its opposite anchor"
"the reported top-edge defect is dead -- the bottom edge holds still" "a right or bottom resize
leaves a default place reactive"

### WL-ARR-06
`workshop/setup.hpp` author_pane_window check_pane_place_coord PaneAxisProposal share_reaching
WindowWritten
`workshop/screen_gestures.cpp` pane_window_proposal pane_window_axes
`workshop/screen.hpp` PaneWindowProposal pane_window_at
`workshop/weave_arrange.cpp` arrange_place arrange_resize arrange_window write_window
`tests/test_workshop_screen.cpp` "a refused anchored resize writes neither the place nor the size"
"a move blocked at the left wall still follows the hand down" "a move past two walls at once writes
nothing" "a refused nudge does not author a reactive place" "a corner resize blocked on one axis
still resizes the other" "a hand pulling a share's edge writes the share whose edge is the hand's
pixel, at every pixel of the widest room" "a fill pulled at its far edge, with nothing sharing its
frame, writes the share whose edge is the hand's pixel, and says so" "a hand moves an anchored pane
in its word, a centred one into pixels, and pulls a fitted size into pixels, each said"

### WL-ARR-07
`workshop/screen.hpp` PaneArrange Session::arrange
`workshop/weave_arrange.cpp` enter_arrange_pane take_pane_hold arrange_geometry_ready
open_arrange_desk arrange_press
`tests/test_workshop_panes_window.cpp` "the one-pane scope is bound -- another pane cannot be drawn
into it" "the desk manipulates panes directly, and a press is its own targeting"
`tests/test_workshop_host.cpp` "contextual Arrange admission precedes binding"

### WL-ARR-08
`workshop/keymap.hpp` kActionCatalog KeyContext::kArrangePane KeyContext::kArrangeDesk
KeyContext::kArrangeReset
`workshop/weave_arrange.cpp` arrange_key arrangeable arrange_step arrange_nudge arrange_grow
`tests/test_workshop_panes_window.cpp` "the keyboard alone reaches every window operation" "a hand
and a key author the same setup values" "escape unwinds one level and rolls nothing back"
`tests/test_workshop_panes_info.cpp` "a pane its words leave too small to see, or off the room, is
still listed, reached by tab, inspected by Info and given its size back there"

### WL-ARR-09
`workshop/weave_arrange.cpp` enter_arrange_pane arrange_status
`workshop/screen_reveal.cpp` paint_pane_affordances
`workshop/screen.hpp` pane_edge_name
`workshop/screen_arrange.cpp` pane_edge_cell
`tests/test_workshop_screen.cpp` "the arrangement's visible statement is the ring on the pane
itself" "contextual Arrange lifts the pane it addressed, not the one in front"
`tests/test_workshop_panes_window.cpp` "stepping names the pane, its state and its authored window
in words"

### WL-ARR-10
`workshop/weave_arrange.cpp` arrange_grow arrange_key
`workshop/screen.hpp` kCoarseStepCells pane_edge::kBottomRight Session::arrange_step_cells
`workshop/screen_gestures.cpp` pane_window_proposal
`workshop/setup.hpp` author_pane_window
`tests/test_workshop_panes_window.cpp` "the coarse step is the resize seam with a bigger delta" "a
coarse shrink meets the same per-axis refusal a fine one does"
`tests/test_workshop_screen.cpp` "the coarse step is `workshop.arrange-step` cells, read from
Workshop's settings file"

### WL-ARR-11
`workshop/screen.hpp` kCoarseStepCells kStackRows kChromeCells kMinArrangeStepCells
kMaxArrangeStepCells
`tests/test_workshop_panes_input.cpp` "one coarse grow gives the DEFAULT Compose pane a usable form"

### WL-ARR-12
`workshop/keymap.hpp` manage.grow manage.shrink
`tests/test_workshop_panes_window.cpp` "the coarse step is ordinary action vocabulary, not pane
chrome"
`tests/test_workshop_screen.cpp` "no ordinary pane spends a row teaching a key the keymap already
owns"

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
`workshop/pane_vocabulary.hpp` PaneEscapeUnspent
`workshop/weave.hpp` WorkshopWeave::gestures_ WorkshopWeave::Hand::escape_sent
WorkshopWeave::GestureSent WorkshopWeave::gesture_asks_
`workshop/weave_external.cpp` on(PaneEscapeUnspent) WorkshopWeave::external_key
`workshop/pane_escape.hpp` answer unspent bare
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
