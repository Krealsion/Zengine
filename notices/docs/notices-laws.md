# The notices' laws (WL-NOTE)

**Laws.** The numbered rules kept about what Workshop said: by the notices record, by the Notices
pane and by Workshop, which says it. Each is a rule that more than one file relies on, and a
comment or a page that relies on one names it by its number. A rule about one declaration is in
that declaration's comment, not here. The record's messages are in [its
manual](zengine.notices.md), and the pane's in [the pane's](zengine.notices-pane.md). The files and
cases that prove each law are listed in
[`tests/laws/notices.txt`](../../tests/laws/notices.txt), which the `law_register` check reads.

## WL-NOTE-02 — The notices record keeps what Workshop said, at most its `keep`, the oldest let go

The notices record, `zengine.notices`, keeps each `NoticeSaid` that Workshop (`zengine.workshop`)
says, and nothing that another weave says. It keeps at most its role-wide `keep` setting, from
`kDefaultKept` to `kMostKept`; past that the oldest are let go and counted, and the record says
`NoticesMoved`.

What this means:

- Each notice is kept as it was heard, after the newest, its words cut at `kMaxNoticeBytes`.
- A lower `keep` lets the oldest go at once. Until the setting has been handed to the record in a
  life of it, nothing goes below `kMostKept`, so a reload keeps what the record held.
- The record lasts the run and crosses a reload in place in its state. No file holds it.

Why: [what Workshop said is kept](../../agents/decisions/what-workshop-said-is-kept.md).

## WL-NOTE-05 — A guest reads what Workshop said only with `capture`

A guest observes `NoticeSaid` and `NoticesMoved`, and is answered `NoticesShown`, only when its
row in [the guests file](../../workshop/docs/zengine.guests.md) grants `capture`. A row that
observes the record's `NoticesMoved` may ask the record for a page.

What this means: asking for a page reads what the row may already follow, and gives it no power
over the record.

Why: [what Workshop said is kept](../../agents/decisions/what-workshop-said-is-kept.md).

## WL-NOTE-06 — Workshop says only what helps the weaver understand what just happened

A notice says what changed for the weaver, in the weaver's words. A step that an owner took for
itself, or a motion still under way, is not said; every refusal and every failure is.

What this means:

- A setting that a role commits for itself is said only when it was not handed or not written. One
  that another role commits for it is said as it landed.
- A hide says what was hidden and what went with it.
- A drag says its before and its after alone, and its size too when that changed. A drop says what
  it carried, from where to where, and nothing is said while it moves.

What it does not mean: that a refusal or a failure goes unsaid, or that the record or the pane
chooses what is said. Each owner says less, and what is said is kept.

Why: [what Workshop said is kept](../../agents/decisions/what-workshop-said-is-kept.md).
