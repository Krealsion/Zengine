# `notices/` — Notices: working on it

The router for `notices/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `CMakeLists.txt` | target `zengine-notices`, a loadable weave; needs `loom::kernel` |
| `notices.cpp` | the record: keeps `NoticeSaid`, says `NoticesMoved`, answers `NoticesRequested`, declares `keep` |
| `vocabulary.hpp` | office `zengine.notices`, stem, setting, bounds, the page's shapes, `NoticesState` |
| `notices-pane/CMakeLists.txt` | target `zengine-notices-pane`, a loadable weave |
| `notices-pane/pane.cpp` | the pane: offers `notices`, walks the record a page at a time, its two actions and the wheel |
| `notices-pane/vocabulary.hpp` | office `zengine.notices-pane`, pane key, stem, action ids, `NoticesPaneState` |
| `docs/zengine.notices.md`, `docs/zengine.notices-pane.md` | the two manuals, each compiled into its weave |

## Law

- [notices](../agents/workshop/notices.md) `WL-NOTE` — what Workshop said: each notice published,
  the record and its `keep`, a page of it, the pane, a guest's reading; witnessed by
  `workshop_panes_inspect`, then `workshop_screen` and `workshop_guests`

## Suites

No entry of its own. Its cases in shared suites: `workshop_panes_inspect`
(`tests/test_workshop_panes_notices.cpp`, the real record and pane against Workshop's real
publication, with the pane's census and presses), `workshop_screen`
(`tests/test_workshop_screen.cpp`, the band without the notice) and `workshop_guests`
(`tests/test_workshop_guests.cpp`, a guest's reading). The rig in `tests/workshop_support.hpp`
resolves both stems to `WORKSHOP_SO_NOTICES` and `WORKSHOP_SO_NOTICES_PANE`. The pane's rows are
pinned in `tests/actions/zengine.notices-pane.txt`.

## Host side

- Plan rows: `workshop/default-load-plan.json` (optional; the record before the window, so the
  launch's notes are kept, the pane beside Attention); both staged beside the host in
  `workshop/CMakeLists.txt`, the pane listed as a development pane.
- What it keeps: `workshop/notice_seam_vocabulary.hpp` (`NoticeSaid`), published by
  `WorkshopWeave::say` and `publish_notices` in `workshop/weave_document.cpp`; Workshop's grant to
  say it, `workshop/grant.cpp`.
- The desk: `default_setup` in `workshop/setup.hpp` seats the pane (`kNoticesPaneProvider` in
  `workshop/panes.hpp`); the band without the notice, `band_region` in `workshop/screen_compose.cpp`.
- A guest's reading: `workshop/guests.cpp` (`carries_the_desk`, `follows_the_desk`, the page a row
  observing `NoticesMoved` may ask).
- The record's setting: the layouts' owner's office-wide settings (`workshop/layouts/`).

## Pages

- [`zengine.notices-pane`](docs/zengine.notices-pane.md), the Notices pane's manual
- [`zengine.notices`](docs/zengine.notices.md), the record's manual

## Practices

- [Notices](../docs/contributing/best-practices.md#notices-notices)
