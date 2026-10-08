# Workshop law — attention

Register `WL-ATTN`: a thing that happened and a thing that is true are two surfaces. One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-ATTN-01 — An utterance and a condition are two surfaces

LAW — An utterance is a sentence about a moment that passed, held in one notice row; a condition is a fact that is true when it is read, held under a key or derived from a live owner.

MEANS
- an utterance is replaced by the next thing said and retracted no other way;
- a condition disappears because it resolved, never because something else was said.

PROVEN BY — `workshop/screen.hpp` `Session::notice`, `Session::conditions`, `kKeymapWallKey`;
`workshop/weave_run.cpp` `say`; `workshop/attention.hpp` `HeldConditions`, `Condition`,
`unavailable_tool`; `workshop/weave.hpp` `HostContext::standing_conditions`,
`WorkshopWeave::prefs_bad_`; `workshop/weave_handlers.cpp` `take_host_conditions`;
`tests/test_workshop_host.cpp` case `"event sentences stay events, and a condition needs no
sentence"`, case `"a held condition stands until its owner retracts it"`, case `"an
unavailable tool is named by its artifact on the host's own condition row, which no tool
paints"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-02 — `Session::notice` is the utterance row and nothing else

LAW — The standing truths (a refused keymap or prefs file, a shadowed legacy file, a pane's refused update, a waiting frontier) left the notice; `speak_startup_notes` joins only the event halves.

PROVEN BY — `workshop/weave_handlers.cpp` `speak_startup_notes`, `take_host_conditions`;
`workshop/weave_document.cpp` `say`; `workshop/weave.hpp` `HostContext::transition_note`;
`tests/test_workshop_host.cpp` case `"event sentences stay events, and a condition needs no
sentence"`; `tests/test_workshop_persistence.cpp` case `"a refused prefs file is spoken,
stands, and is never overwritten"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-03 — `attention_conditions` is a pure projection

LAW — `attention_conditions` reads the held set and the derived owners, ranks, and owns nothing; it is the one population every consumer on this side of the seam spends.

MEANS
- what a weaver has hidden is subtracted on the PANE's side and nowhere here;
- so the glance says what is true, and the list says what this weaver is looking at.

PROVEN BY — `workshop/screen_attention.cpp` `attention_conditions`;
`tests/test_workshop_host.cpp` case `"a held condition stands until its owner retracts it"`,
case `"the glance is ranked by truth, and says how many it is not saying"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-04 — A derived condition stays derived

LAW — A derived condition is never copied into the held set; its owner's next truth clears it with no retraction call, and a pane's refusal is that truth until content this host ACCEPTED replaces it.

MEANS
- a room grant turns over the rows, `heard` and `awaiting`; the refusal is about the content;
- so a weaver cannot un-say it by widening their window, and a re-offer does not either;
- a canvas picture refused for what it holds is one, its last admitted picture marked `(update refused)`; at its corner the mark is Workshop's own chrome, which the pane view waits behind.

DOES NOT MEAN
- that the reason follows the room — it quotes the grant the refused content was judged against;
- that a picture refused as late -- for a holder, a room or a number since replaced -- is said: its pane alone is answered.

PROVEN BY — `workshop/panes.hpp` `ExternalPane`, `ExternalPane::refusal`,
`ExternalPane::refusal_why`, `ProjectFrontier`, `ExternalPane::clear_refusal`;
`workshop/screen_pane_state.cpp` `pane_state_of`; `workshop/screen_compose.cpp` `paint`;
`workshop/weave_external.cpp` `refresh_external_rooms`; `workshop/weave_seam.cpp` `judge_content`;
`workshop/screen_attention.cpp` `attention_conditions`; `workshop/attention.hpp` `HeldConditions`;
`workshop/weave.hpp` `HostContext::frontier`; `workshop/weave_run.cpp` `frontier_now`;
`workshop/weave_canvas.cpp` `admit_canvas_content`, `WorkshopWeave::canvas_press`,
`WorkshopWeave::on_refused_mark`;
`workshop/screen_external.cpp` `paint_external`, `refused_mark_cover`;
`workshop/weave_inspection.cpp` `WorkshopWeave::visible_body`;
`workshop/weave_operation.cpp` `WorkshopWeave::drop_on_canvas`, `WorkshopWeave::release_value_drag`;
`workshop/screen.hpp` `kExternalPictureRefused`, `kExternalRefusedMark`;
`tests/test_workshop_host.cpp` case `"a derived condition enters and leaves attention with its
subject"`, case `"the project frontier is a condition while it waits and nothing after"`;
`tests/test_workshop_panes_seam.cpp` case `"a refusal stands until ACCEPTED CONTENT
replaces it, a new room included"`; `tests/test_workshop_panes_canvas.cpp` case `"a canvas picture
refused for what it holds is said in its pane and in Attention until a picture of the pane is
admitted, and one that came late is answered to its pane alone"`; `tests/test_workshop_desk.cpp` case
`"with pane titles hidden, a refused picture's mark is Workshop's own: the pane view waits while it
covers the picture, a press on it reaches no provider and a right press opens Workshop's menu, and
the picture beside it takes a press as before, a press held or not"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-05 — One pane state earns ambient attention and four do not

LAW — `off-room` is a condition; `closed` is the weaver's choice, `unresolved` is already counted on the Layouts row, `covered` has something visible, and `open` is nothing.

PROVEN BY — `workshop/screen_attention.cpp` `attention_conditions`;
`workshop/screen_pane_state.cpp` `pane_state_of`; `tests/test_workshop_host.cpp` case `"not
every true pane state deserves ambient attention"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-06 — The glance is the Attention pane's first row

LAW — The loudest condition the host says is true, with an honest `(+N more)`, leads the Attention pane in that condition's role; Workshop puts nothing on Surface's attention slot.

MEANS
- a weaver who hides a row still reads it in the glance, because it is still true;
- a pane one row tall is the glance alone; nothing true is said in words, never an empty box.

DOES NOT MEAN
- that Surface's slot is gone: a Skin draws it, in the picture and the title, for whoever uses it.

PROVEN BY — `workshop/attention_seam_vocabulary.hpp` `attention_glance`;
`attention-pane/pane.cpp` `say_view`; `workshop/weave_run.cpp` `WorkshopWeave::repaint`;
`tests/test_workshop_host.cpp` case `"Workshop puts nothing on the attention slot, healthy or
not: what is true is said to whoever presents it"`, case `"the glance is ranked by truth, and
says how many it is not saying"`; `tests/test_workshop_panes_attention.cpp` case `"the Attention
pane leads with the glance: the loudest condition that is true and how many more, hidden ones
counted"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-07 — Ranking is `ranks_before`: loudness, then key

LAW — `attention_rank` is the one place this application claims one role is more urgent than another: total, with an unknown role last.

PROVEN BY — `workshop/attention.hpp` `ranks_before`, `attention_rank`;
`tests/test_workshop_host.cpp` case `"the glance is ranked by truth, and says how many it is not
saying"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-08 — Dismissal is the PANE's, scoped to the statement and not to the key

LAW — The Attention pane remembers the key and a stamp of the statement it hid, so a condition whose content moves is visible again; a dismissal outlives no condition.

MEANS
- never persisted; it crosses a same-shape reload in the pane's state and reaches no file;
- dismiss is not resolve: the host still holds the condition, derives it and says it;
- so a list whose every current condition is hidden says they are hidden, never the all-clear.

PROVEN BY — `attention-pane/vocabulary.hpp` `Dismissal`, `AttentionPaneState::dismissed`;
`attention-pane/pane.cpp` `say_view`, `visible`, `known_`;
`workshop/attention.hpp` `Condition::stamp`; `tests/test_workshop_panes_attention.cpp` case
`"dismissal hides a presentation and changes nothing that is true"`, case
`"a dismissed condition comes back when it materially changes"`, case
`"a dismissal does not outlive the condition it was about"`, case `"an Attention
pane whose every current condition is hidden says they are hidden and still true, through a spent
notice and a new room, and says nothing needs attention only when nothing is true"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-09 — The Attention view is a pane, keyed only once pressed into

LAW — The view is a pane, so it takes the keyboard under `KeyContext::kPane` when pressed into; the mode, its four rows and the global chord are gone, and three ids are the pane's own.

PROVEN BY — `tests/test_workshop_panes_attention.cpp` case `"the pane declares the three ids a
weaver's keymap file already names"`, case `"the Attention pane's keys act only after
the weaver has pressed into it"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-10 — A condition names an action and holds no power

LAW — A condition names an action by its catalog id, which the HOST resolves into words against the effective keymap; the id never crosses, and no severity opens or arranges anything.

MEANS
- the pane is handed a sentence and could not press an action if it were given one;
- nothing puts this pane on a screen but a weaver launching it from the Pane Manager.

PROVEN BY — `workshop/attention.hpp` `Condition::action`; `workshop/keymap.hpp` `ActionRow`;
`workshop/screen_attention.cpp` `standing_conditions`; `tests/test_workshop_host.cpp` case
`"a condition names an action and what crosses is the weaver's own gesture"`, case
`"an alert condition opens nothing"`; `tests/test_workshop_panes_attention.cpp` case
`"the action a condition names arrives as words and not as a name"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`

## WL-ATTN-11 — The condition path holds no timer, no callback and no history

LAW — `workshop/attention.hpp` includes exactly `surface/vocabulary.hpp`; nothing in the path schedules, calls back, records or persists, and displaying a condition implies no history.

MEANS
- the wire form is the SEAM's; the internal type still crosses nothing;
- an observer can see what the host says, and that grants nobody observation authority.

PROVEN BY — `workshop/attention.hpp` `HeldConditions`; `tests/test_workshop_host.cpp` case
`"the condition path carries no timer, no callback and no history"`, case `"showing a
condition writes no history"`.
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
`WorkshopWeave::conditions_said_`; `tests/test_workshop_host.cpp` case `"what is true is said
across the seam, in the host's own order and words"`, case `"nothing new is nothing
said, which is what stops the seam looping"`.
WHY — `agents/decisions/a-condition-has-a-lifetime.md`
