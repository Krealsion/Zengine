# zengine.notices -- the notices record

**Manual.** The notices record: the weave that keeps what Workshop said during this run, answering
to the office `zengine.notices` (an office is the name a weave answers to). It is the loadable weave
`zengine-notices`, which Workshop's plan loads before anything is said; it offers no pane, and the
[Notices pane](zengine.notices-pane.md) walks it. Other weaves reach it by the messages below.

## About

Each time Workshop says something -- `saved setup "Working"`, a refused save, the size the last
session came back at -- it publishes the notice once, numbered from 1 in the run, with whether it
was a refusal ([`NoticeSaid`](#noticesaid)). The record keeps each one as it was said, the newest
last, until it holds as many as its `keep` setting, and then lets the oldest go. A notice longer
than [`notices/vocabulary.hpp`](../vocabulary.hpp)'s `kMaxNoticeBytes` is kept cut there, ending
`...`.

A notice is a moment that passed. The record is not a list of what is true now -- that is
[Attention](../../attention/docs/zengine.attention.md)'s -- and it is not a queue: nothing in it is
unread, nothing waits for you, and nothing expires with time. It lasts the run: a reload of the
weave in place keeps it, and a fresh Workshop begins it empty. No file holds it.

## Use

Ask for a page with [`NoticesRequested`](#noticesrequested), and ask again when
[`NoticesMoved`](#follow) says the record moved.

The record keeps one office-wide setting, `keep`: how many notices it keeps, from
`kDefaultKept`, its default, to `kMostKept`. It is kept in Workshop's settings file and written by
message, at the revision read
([`OfficeSettingCommitRequested`](../../workshop/docs/zengine.layouts.md#officesettingcommitrequested),
office `zengine.notices`); a value under what the record holds lets the oldest go at once.

## Commands

### `NoticesRequested`

`NoticesRequested` v1 asks for a page of the record, answered to you alone.

- **Takes:** `newest`, the number the page ends at, or the newest kept before it when that one is
  not kept -- 0 for the newest kept; `count`, how many notices, at most `kMaxNoticesPage`.
- **Answers:** `NoticesShown` v1: `rows`, the notices of the page, oldest first, each its
  `number`, `text` and `refusal`; `before` and `after`, how many kept notices stand before and
  after them, and `next`, the number of the first after them, or 0; `first` and `last`, the oldest
  and newest numbers kept, 0 when nothing is; `keep`, how many the record keeps; `let_go`, how many
  it has let go this run.
- **Refuses:** nothing; a `newest` older than everything kept answers no rows.
- **Example:** `mail.send_to_role("zengine.notices", NoticesRequested{0, 20}, ask);` reads the
  twenty newest.

## Hears

### `NoticeSaid`

`NoticeSaid` v1, published by `zengine.workshop` alone
([`workshop/notice_seam_vocabulary.hpp`](../../workshop/notice_seam_vocabulary.hpp)): each is kept as
heard, after the newest, since Workshop says each notice once.

### `OfficeSettings`

`OfficeSettings` v1, from `zengine.layouts` alone: the record's `keep`, a default kept as its
absence. The record declares the setting each time it is activated, and until it is handed back
in that life it lets nothing go below `kMostKept`.

## Follow

`NoticesMoved` v1, published to any as `zengine.notices` each time the record kept a notice,
replaced one or let some go, and once each time it is activated: `first`, `last`, `keep` and
`let_go`, as a page says them. It carries no words: a reader asks for the page it wants. A guest
follows it with `capture`, and observing it lets the guest ask for a page
([the guests file](../../external-host/docs/zengine.guests.md)).

## See also

- [The Notices pane](zengine.notices-pane.md), which shows the record.
- [`notices/vocabulary.hpp`](../vocabulary.hpp): the office, the setting, the bounds and the shapes.
