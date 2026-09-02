# Workshop law — attention

Register `WL-ATTN`: a thing that happened and a thing that is true are two surfaces. One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-ATTN-01 — An utterance and a condition are two surfaces

LAW — An utterance is `Session::notice`, written by `say(text, bad)` about a moment that passed; a condition is true when read, held in `Session::conditions` or derived from a live owner.

MEANS
- an utterance is replaced by the next thing said and retracted no other way;
- a condition disappears because it resolved, never because something else was said.

PROVEN BY — `workshop/screen.hpp` `notice`, `say`, `conditions`; `workshop/attention.hpp`
`HeldConditions`, `Condition`; `tests/test_workshop_panels.cpp` case `"WUX-4: event sentences
stay events, and a condition needs no sentence"`, case `"WUX-4: a held condition stands until its
owner retracts it"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-02 — `Session::notice` is the utterance row and nothing else

LAW — The standing truths (a refused keymap or prefs file, a shadowed legacy file, a pane's refused update, a waiting frontier) left the notice; `speak_startup_notes` joins only the event halves.

PROVEN BY — `workshop/weave.hpp` `speak_startup_notes`, `say`; `tests/test_workshop_panels.cpp`
case `"WUX-4: event sentences stay events, and a condition needs no sentence"`;
`tests/test_workshop_persistence.cpp` case `"WUX-3: a refused prefs file is spoken, stands, and
is never overwritten"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-03 — `attention_conditions` is a pure projection

LAW — `attention_conditions` reads the held set and the derived owners, ranks, and owns nothing; `attention_shown` is that list less this session's dismissals, the one population every consumer spends.

PROVEN BY — `workshop/attention.hpp` `attention_conditions`; `workshop/screen.hpp`
`attention_shown`; `tests/test_workshop_panels.cpp` case `"WUX-4: the view shows every current
condition in its owner's own words"`, case `"WUX-4: the view never publishes more rows than its
region can show"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-04 — A derived condition stays derived

LAW — `ExternalPane::refusal`, `pane_state_of` and `ProjectFrontier` are correct by construction and never copied into `HeldConditions`; a pane's next valid content clears its refusal with no retraction call.

PROVEN BY — `workshop/panel.hpp` `refusal_why`; `workshop/screen.hpp` `pane_state_of`;
`workshop/attention.hpp` `ProjectFrontier`, `HeldConditions`; `tests/test_workshop_panels.cpp`
case `"WUX-4: a derived condition enters and leaves attention with its subject"`, case `"WUX-4:
the project frontier is a condition while it waits and nothing after"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-05 — Three pane states earn ambient attention and four do not

LAW — `refused`, `waiting` and `off-room` are conditions; `closed` is the maker's choice, `unresolved` is already counted on the Layouts row, `covered` has something visible, and `open` is nothing.

PROVEN BY — `workshop/attention.hpp` `attention_conditions`; `workshop/screen.hpp`
`pane_state_of`; `tests/test_workshop_panels.cpp` case `"WUX-4: not every true pane state deserves
ambient attention"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-06 — The compact channel is the `kSlotScore` slot, and empty is the retraction

LAW — The loudest condition plus an honest `(+N more)` is published as `SurfaceText` on every repaint before the canvas, because the SDL medium composes it into the picture; no band row was taken.

PROVEN BY — `workshop/weave.hpp` `kSlotScore`; `surface/vocabulary.hpp` `kSlotScore`,
`SurfaceText`; `tests/test_workshop_panels.cpp` case `"WUX-4: a healthy Workshop says nothing on
the attention slot at all"`, case `"WUX-4: the compact line is ranked by truth, and says how
many it is not saying"`; `tests/test_surface.cpp` case `"WUX-4: the attention chip is a region
in the picture, and empty draws nothing"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-07 — Ranking is `ranks_before`: loudness, then key

LAW — `attention_rank` is the one place this application claims one role is more urgent than another: total, with an unknown role last.

PROVEN BY — `workshop/attention.hpp` `ranks_before`, `attention_rank`;
`tests/test_workshop_panels.cpp` case `"WUX-4: the compact line is ranked by truth, and says how
many it is not saying"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-08 — Dismissal is scoped to the statement, not the key

LAW — `AttentionView::dismissed` holds `{key, stamp}`, and `Condition::stamp()` is an opaque token over compact, detail, role and action, so a condition whose content moves is visible again unasked.

MEANS
- session-only, never persisted; dismiss is not resolve and changes no underlying truth.

PROVEN BY — `workshop/attention.hpp` `dismissed`, `stamp`; `workshop/screen.hpp`
`AttentionView`; `tests/test_workshop_panels.cpp` case `"WUX-4: dismissal hides a presentation and
changes nothing that is true"`, case `"WUX-4: a dismissed condition comes back when it materially
changes"`, case `"WUX-4: dismiss is not resolve, resolve is not dismiss"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-09 — `KeyContext::kAttention` is a mode in the picker's place

LAW — It sits below the Terminal and the arrangement scopes and above a focused pane and a live draft, not keys-modal because its gestures are catalog rows; `workshop.attention` is `kNoText` on `^a`.

PROVEN BY — `workshop/keymap.hpp` `kAttention`, `workshop.attention`, `kNoText`;
`workshop/screen.hpp` `keyboard_context_beneath_menu`; `tests/test_workshop_panels.cpp` case
`"WUX-4: the view's gestures are the keymap's, and every help surface says so"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-10 — A condition names an action and holds no power

LAW — `Condition::action` is an `ActionRow::id` or nothing, painted through the effective keymap; nothing may open the view but a maker's gesture, and no severity or count reaches `toggle_attention`.

PROVEN BY — `workshop/attention.hpp` `action`; `workshop/weave.hpp` `toggle_attention`;
`tests/test_workshop_panels.cpp` case `"WUX-4: a condition names an action and cannot execute
one"`, case `"WUX-4: an alert condition opens nothing"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-11 — The condition path touches neither the Recorder nor the Logger

LAW — `workshop/attention.hpp` includes exactly `surface/vocabulary.hpp`, so a condition has no wire form and cannot be observed, recorded, selected or persisted; displaying one implies no history.

PROVEN BY — `workshop/attention.hpp` `HeldConditions`; `tests/test_workshop_panels.cpp` case
`"WUX-4: the condition path carries no timer, no callback and no history"`, case `"WUX-4:
showing a condition writes no history"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`
