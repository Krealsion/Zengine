# Workshop law — attention

Register `WL-ATTN`: a thing that happened and a thing that is true are two surfaces. One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-ATTN-01 — An utterance and a condition are two surfaces

LAW — An utterance is a sentence about a moment that passed, held in one notice row; a condition is a fact that is true when it is read, held under a key or derived from a live owner.

MEANS
- an utterance is replaced by the next thing said and retracted no other way;
- a condition disappears because it resolved, never because something else was said.

PROVEN BY — `workshop/screen.hpp` `Session::notice`, `Session::conditions`, `kKeymapWallKey`;
`workshop/weave_run.cpp` `say`; `workshop/attention.hpp` `HeldConditions`, `Condition`;
`workshop/weave.hpp` `HostContext::standing_conditions`, `WorkshopWeave::prefs_bad_`;
`workshop/weave_handlers.cpp` `take_host_conditions`; `tests/test_workshop_panels.cpp` case
`"WUX-4: event sentences stay events, and a condition needs no sentence"`, case `"WUX-4: a held
condition stands until its owner retracts it"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-02 — `Session::notice` is the utterance row and nothing else

LAW — The standing truths (a refused keymap or prefs file, a shadowed legacy file, a pane's refused update, a waiting frontier) left the notice; `speak_startup_notes` joins only the event halves.

PROVEN BY — `workshop/weave_handlers.cpp` `speak_startup_notes`, `take_host_conditions`;
`workshop/weave_document.cpp` `say`; `workshop/weave.hpp` `HostContext::transition_note`;
`tests/test_workshop_panels.cpp` case `"WUX-4: event sentences stay events, and a condition needs
no sentence"`; `tests/test_workshop_persistence.cpp` case `"WUX-3: a refused prefs file is spoken,
stands, and is never overwritten"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-03 — `attention_conditions` is a pure projection

LAW — `attention_conditions` reads the held set and the derived owners, ranks, and owns nothing; it is the one population every consumer on this side of the seam spends.

MEANS
- what a maker has hidden is subtracted on the PANE's side and nowhere here;
- so the compact chip says what is true, and the pane says what this maker is looking at.

PROVEN BY — `workshop/screen_attention.cpp` `attention_conditions`;
`tests/test_workshop_panels.cpp` case `"WUX-4: a held condition stands until its owner retracts
it"`, case `"WUX-4: the compact line is ranked by truth, and says how many it is not saying"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-04 — A derived condition stays derived

LAW — A derived condition is never copied into the held set; its owner's next truth clears it with no retraction call, and a pane's refusal is that truth until content this host ACCEPTED replaces it.

MEANS
- a room grant turns over the rows, `heard` and `awaiting`; the refusal is about the content;
- so a maker cannot un-say it by widening their window, and a re-offer does not either.

DOES NOT MEAN
- that the reason follows the room — it quotes the grant the refused content was judged against.

PROVEN BY — `workshop/panel.hpp` `ExternalPane`, `ExternalPane::refusal`,
`ExternalPane::refusal_why`, `ProjectFrontier`, `ExternalPane::clear_refusal`;
`workshop/screen_pane_state.cpp` `pane_state_of`; `workshop/screen_compose.cpp` `paint`;
`workshop/weave_external.cpp` `refresh_external_rooms`; `workshop/weave_seam.cpp` `judge_content`;
`workshop/screen_attention.cpp` `attention_conditions`; `workshop/attention.hpp` `HeldConditions`;
`workshop/weave.hpp` `HostContext::frontier`; `workshop/weave_run.cpp` `frontier_now`;
`tests/test_workshop_panels.cpp` case `"WUX-4: a derived condition enters and leaves attention
with its subject"`, case `"WUX-4: the project frontier is a condition while it waits and nothing
after"`; `tests/test_workshop_panes_seam.cpp` case `"a refusal stands until ACCEPTED CONTENT
replaces it, a new room included"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-05 — Three pane states earn ambient attention and four do not

LAW — `refused`, `waiting` and `off-room` are conditions; `closed` is the maker's choice, `unresolved` is already counted on the Layouts row, `covered` has something visible, and `open` is nothing.

PROVEN BY — `workshop/screen_attention.cpp` `attention_conditions`;
`workshop/screen_pane_state.cpp` `pane_state_of`; `tests/test_workshop_panels.cpp` case `"WUX-4:
not every true pane state deserves ambient attention"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-06 — The compact channel is the `kSlotScore` slot, and empty is the retraction

LAW — The loudest CURRENT condition plus an honest `(+N more)` is published as `SurfaceText` on every repaint before the canvas, because the SDL medium composes it into the picture; no band row was taken.

MEANS
- it is the medium's own furniture and Workshop cannot be pointed at it in either medium;
- a maker who hides a row in the pane still sees it here, because it is still true.

PROVEN BY — `workshop/weave_run.cpp` `kSlotScore`; `surface/vocabulary.hpp` `kSlotScore`,
`SurfaceText`; `workshop/screen_attention.cpp` `attention_compact`;
`tests/test_workshop_panels.cpp` case `"WUX-4: a healthy Workshop says nothing on the attention
slot at all"`, case `"WUX-4: the compact line is ranked by truth, and says how many it is not
saying"`; `tests/test_surface.cpp` case `"WUX-4: the attention chip is a region in the picture,
and empty draws nothing"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-07 — Ranking is `ranks_before`: loudness, then key

LAW — `attention_rank` is the one place this application claims one role is more urgent than another: total, with an unknown role last.

PROVEN BY — `workshop/attention.hpp` `ranks_before`, `attention_rank`;
`tests/test_workshop_panels.cpp` case `"WUX-4: the compact line is ranked by truth, and says how
many it is not saying"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-08 — Dismissal is the PANE's, scoped to the statement and not to the key

LAW — The Attention pane remembers the key and a stamp of the statement it hid, so a condition whose content moves is visible again; a dismissal outlives no condition.

MEANS
- never persisted; it crosses a same-shape reload in the pane's state and reaches no file;
- dismiss is not resolve: the host still holds the condition, derives it and says it.

PROVEN BY — `attention-pane/vocabulary.hpp` `Dismissal`, `AttentionPaneState::dismissed`;
`workshop/attention.hpp` `Condition::stamp`; `tests/test_workshop_panes_attention.cpp` case
`"ATTN-WEAVE: dismissal hides a presentation and changes nothing that is true"`, case
`"ATTN-WEAVE: a dismissed condition comes back when it materially changes"`, case
`"ATTN-WEAVE: a dismissal does not outlive the condition it was about"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-09 — RETIRED: the view was a mode in the picker's place

LAW — The view is a pane, so it takes the keyboard under `KeyContext::kPane` when pressed into; the mode, its four rows and the global chord are gone, and three ids are the pane's own.

PROVEN BY — `tests/test_workshop_panes_attention.cpp` case `"ATTN-WEAVE: the pane declares the
three ids a maker's keymap file already names"`, case `"ATTN-WEAVE: the pane's keys act only
after the maker has pressed into it"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-10 — A condition names an action and holds no power

LAW — A condition names an action by its catalog id, which the HOST resolves into words against the effective keymap; the id never crosses, and no severity opens or arranges anything.

MEANS
- the pane is handed a sentence and could not press an action if it were given one;
- nothing puts this pane on a screen but a maker choosing it from the picker.

PROVEN BY — `workshop/attention.hpp` `Condition::action`; `workshop/keymap.hpp` `ActionRow`;
`workshop/screen_attention.cpp` `standing_conditions`; `tests/test_workshop_panels.cpp` case
`"WUX-4: a condition names an action and what crosses is the maker's own gesture"`, case
`"WUX-4: an alert condition opens nothing"`; `tests/test_workshop_panes_attention.cpp` case
`"ATTN-WEAVE: the action a condition names arrives as words and not as a name"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-11 — The condition path holds no timer, no callback and no history

LAW — `workshop/attention.hpp` includes exactly `surface/vocabulary.hpp`; nothing in the path schedules, calls back, records or persists, and displaying a condition implies no history.

MEANS
- the wire form is the SEAM's; the internal type still crosses nothing;
- an observer can see what the host says, and that grants nobody observation authority.

PROVEN BY — `workshop/attention.hpp` `HeldConditions`; `tests/test_workshop_panels.cpp` case
`"WUX-4: the condition path carries no timer, no callback and no history"`, case `"WUX-4:
showing a condition writes no history"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-12 — What is true right now crosses as a publication, and only when it changes

LAW — The host says every current condition as `StandingConditions`, `to_any`, ranked, with the action resolved into words; it compares against its own last utterance and stays silent when they are equal.

MEANS
- silence is what makes it terminate: content ends in a repaint, so saying it always would loop;
- what the host remembers is its own last utterance, never a second copy of the truth.

DOES NOT MEAN
- that a condition is addressed to anybody: which weave presents it is a load plan's answer.

PROVEN BY — `workshop/attention_seam_vocabulary.hpp` `StandingCondition`,
`StandingCondition::suggestion`, `StandingConditions`; `workshop/screen_attention.cpp`
`standing_conditions`, `same_conditions`; `workshop/weave_run.cpp`
`WorkshopWeave::say_conditions`; `workshop/weave.hpp` `WorkshopWeave::said_conditions_`,
`WorkshopWeave::conditions_said_`; `tests/test_workshop_panels.cpp` case `"WUX-4: what is
true is said across the seam, in the host's own order and words"`, case `"WUX-4: nothing new
is nothing said, which is what stops the seam looping"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`
