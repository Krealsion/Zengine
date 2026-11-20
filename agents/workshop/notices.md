# Workshop law — what Workshop said

Register `WL-NOTE`: each thing Workshop says, published as said; the notices record that keeps it,
its bound and a page of it; the Notices pane that walks it; and a guest's reading of it. One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md). What is true now is
[`attention.md`](attention.md)'s.

Retired: WL-NOTE-01, WL-NOTE-03, WL-NOTE-04

## WL-NOTE-02 — The notices record keeps what Workshop said, at most its `keep`, the oldest let go

LAW — `zengine.notices` keeps each `NoticeSaid` from `zengine.workshop` alone, at most its role-wide `keep`, `kDefaultKept` to `kMostKept`, the oldest let go and counted, and says `NoticesMoved`.

MEANS
- each notice is kept as heard, after the newest, its words cut at `kMaxNoticeBytes`;
- a lower `keep` lets go at once; until the setting is handed in a life, nothing goes below `kMostKept`;
- it lasts the run and crosses a reload in place in its state; no file holds it.

WHY — `agents/decisions/what-workshop-said-is-kept.md`

## WL-NOTE-05 — What Workshop said is the desk's words to a guest

LAW — A guest observes `NoticeSaid` and `NoticesMoved`, and is answered `NoticesShown`, only with `capture`; a row that observes the record's `NoticesMoved` may ask it for a page.

MEANS
- the ask is a read of what the row may already follow, and grants no power over the record.

WHY — `agents/decisions/what-workshop-said-is-kept.md`

## WL-NOTE-06 — Workshop says only what helps the weaver understand what just happened

LAW — A notice says what changed for the weaver, in the weaver's words; a step an owner took for itself, or a motion still under way, is not said, and every refusal and every failure is.

MEANS
- a setting a role commits for itself is said only when it was not handed or not written; one another role commits for it is said as it landed;
- a hide says what was hidden and what went with it;
- a drag says its before and its after alone, its size too when that changed; a drop says what it carried, from where `->` to where, and nothing is said while it moves.

DOES NOT MEAN
- that a refusal or a failure goes unsaid;
- that the record or the pane chooses: each owner says less, and what is said is kept.

WHY — `agents/decisions/what-workshop-said-is-kept.md`

## Proven by

### WL-NOTE-02
`notices/vocabulary.hpp` kNoticesRole kSettingKeep kDefaultKept kMostKept kMaxNoticeBytes NoticesMoved NoticesState
`notices/notices.cpp` NoticesWeave kept_text let_go_past_keep on(NoticeSaid)
`tests/test_workshop_panes_notices.cpp` "the record keeps what Workshop said, the oldest let go past
keep, and a lower keep lets go at once" "the record keeps nothing a stranger says, cuts a long notice
and keeps what it holds across a reload"

### WL-NOTE-05
`workshop/guests.cpp` carries_the_desk follows_the_desk
`tests/test_workshop_guests.cpp` "a guest reads Workshop's notices through its row: with capture,
and a page of the record it observes"

### WL-NOTE-06
`workshop/weave_operation.cpp` move_value_drag drop_carry
`workshop/layouts/store.cpp` commit_role close
`workshop/layouts/owner.cpp` LayoutsOwner
`workshop/weave_desktop.cpp` judge_toggle
`tests/test_workshop_panes_first_run.cpp` "WL-NOTE-06: what the quick setup keeps for itself is kept
without a notice, and a keep that was not written is said"
`tests/test_workshop_panes_actions.cpp` "WL-DESK-12: a close is the layouts' owner's: the row stays in
the live layout, hidden, its settings and this layout's locks kept, and the presenter takes no close"
`tests/test_workshop_inventory_info.cpp` "a press on a canvas pane drags a value out as a prose press
does: the hold ends as lost when the carry begins, the value lands where the hand lets go, and a
click carries nothing" "while an entry is carried the wheel reaches the pane under the hand, past
Workshop's area in front, and walks its selection; the carry stands, and is dropped where it is
released"
