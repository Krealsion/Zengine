# Workshop law — what Workshop said

Register `WL-NOTE`: each thing Workshop says, published as said; the notices record that keeps it,
its bound and a page of it; the Notices pane that walks it; and a guest's reading of it. One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md). What is true now is
[`attention.md`](attention.md)'s.

## WL-NOTE-01 — Workshop publishes each thing it says once, numbered, at its delivery's end

LAW — Each `say` is a `NoticeSaid` numbered from 1 in the run, published to any as `zengine.workshop` once at the end of the delivery that said it, and no notice is said twice.

MEANS
- `Session::notice` is the last thing said, and the screen draws it nowhere: the band is the keys' legend alone;
- a refusal says so in `refusal`, and a statement is the rest;
- at most `kMaxNoticesOwed` wait for a delivery's end, the oldest let go past it.

DOES NOT MEAN
- that anyone keeps it: which weave keeps and shows it is the load plan's answer.

WHY — `agents/decisions/what-workshop-said-is-kept.md`

## WL-NOTE-02 — The notices record keeps what Workshop said, at most its `keep`, the oldest let go

LAW — `zengine.notices` keeps each `NoticeSaid` from `zengine.workshop` alone, at most its office-wide `keep`, `kDefaultKept` to `kMostKept`, the oldest let go and counted, and says `NoticesMoved`.

MEANS
- each notice is kept as heard, after the newest, its words cut at `kMaxNoticeBytes`;
- a lower `keep` lets go at once; until the setting is handed in a life, nothing goes below `kMostKept`;
- it lasts the run and crosses a reload in place in its state; no file holds it.

WHY — `agents/decisions/what-workshop-said-is-kept.md`

## WL-NOTE-03 — A page of the record is answered to its asker alone

LAW — `NoticesRequested` is answered with at most `kMaxNoticesPage` notices, oldest first, ending at the newest kept at or before `newest` (0: the newest), with how many stand before and after.

MEANS
- the page also says the next number after it, the oldest and newest kept, the `keep` and how many were let go;
- a `newest` older than everything kept answers no rows, and nothing is refused.

WHY — `agents/decisions/what-workshop-said-is-kept.md`

## WL-NOTE-04 — The Notices pane walks the record; a key and a wheel's notch are one action

LAW — `zengine.notices-pane` shows a page of the record, the newest at the bottom, a refusal in the alert's role, no row selected; `notices.up`, `notices.down` and each wheel notch move it a notice.

MEANS
- it asks again when the record moves, its room changes or the view moves, and reads only the answer to its last ask;
- a view that reaches the newest follows it; above, what is kept and not shown, or what was let go; below, what is newer;
- a notice said while the pane is closed is in the record, and in the pane when it opens.

DOES NOT MEAN
- that a notice waits, is unread or expires: nothing opens, counts or ends one;
- that the pane keeps a notice: where its view ends is all its state.

WHY — `agents/decisions/what-workshop-said-is-kept.md`

## WL-NOTE-05 — What Workshop said is the desk's words to a guest

LAW — A guest observes `NoticeSaid` and `NoticesMoved`, and is answered `NoticesShown`, only with `capture`; a row that observes the record's `NoticesMoved` may ask it for a page.

MEANS
- the ask is a read of what the row may already follow, and grants no power over the record.

WHY — `agents/decisions/what-workshop-said-is-kept.md`

## Proven by

### WL-NOTE-01
`workshop/notice_seam_vocabulary.hpp` NoticeSaid kMaxNoticesOwed
`workshop/weave_document.cpp` say publish_notices
`workshop/screen_compose.cpp` band_region
`workshop/screen.hpp` Session::notice
`tests/test_workshop_panes_notices.cpp` "each thing Workshop says is published once, numbered from
one, a refusal told apart"
`tests/test_workshop_screen.cpp` "the band is the keys' legend alone, and the room takes the row the
notice left"

### WL-NOTE-02
`notices/vocabulary.hpp` kNoticesRole kSettingKeep kDefaultKept kMostKept kMaxNoticeBytes NoticesMoved NoticesState
`notices/notices.cpp` NoticesWeave kept_text let_go_past_keep on(NoticeSaid)
`tests/test_workshop_panes_notices.cpp` "the record keeps what Workshop said, the oldest let go past
keep, and a lower keep lets go at once" "the record keeps nothing a stranger says, cuts a long notice
and keeps what it holds across a reload"

### WL-NOTE-03
`notices/vocabulary.hpp` NoticesRequested NoticesShown kMaxNoticesPage
`notices/notices.cpp` on(NoticesRequested)
`tests/test_workshop_panes_notices.cpp` "a page ends where it is asked to and says what stands
before and after it"

### WL-NOTE-04
`notices/notices-pane/vocabulary.hpp` kNoticesPaneRole kNoticesPane kActionUp kActionDown NoticesPaneState
`notices/notices-pane/pane.cpp` NoticesPaneWeave
`workshop/setup.hpp` default_setup
`workshop/panes.hpp` kNoticesPaneProvider kNoticesPaneKey
`tests/test_workshop_panes_notices.cpp` "the Notices pane shows the launch's notes and every notice
after them, the newest at the bottom" "a key and a wheel's notch move the Notices pane's rows the
same" "a notice said while the Notices pane is closed is there when it opens"

### WL-NOTE-05
`workshop/guests.cpp` carries_the_desk follows_the_desk
`tests/test_workshop_guests.cpp` "a guest reads Workshop's notices through its row: with capture,
and a page of the record it observes"
