# Notices

**What Workshop said, kept for the run and shown in a pane: each saved file, each refusal, each
thing a command of yours did, the newest at the bottom.** Workshop says each thing once as it
happens; the notices record keeps the last hundred (a setting keeps more), and the Notices pane,
on a fresh desk at the bottom left of Workshop's window, shows them. Turn the wheel over it, or
press into it and use `up` and `down`, to read older ones. Nothing here waits for you: a notice is
a moment that passed, and what is true now is [Attention](../attention/README.md)'s.

## Pages

- [`zengine.notices-pane`](docs/zengine.notices-pane.md): the pane's manual -- what it shows and
  its keys
- [`zengine.notices`](docs/zengine.notices.md): the record's manual -- what it keeps, for how long,
  its `keep` setting and the page a weave asks for
- [The notices' laws](docs/notices-laws.md): what is kept, who may read it, and what Workshop says

## Parts

| file | what it holds |
|---|---|
| `notices.cpp` | the record, `zengine-notices`, role `zengine.notices`: keeps `NoticeSaid`, says `NoticesMoved`, answers `NoticesRequested`, and declares its `keep` setting |
| `vocabulary.hpp` | the record's role, its stem, its setting and bounds, the page's shapes, and `NoticesState`, what a reload keeps |
| `notices-pane/pane.cpp` | the pane, `zengine-notices-pane`: offers `notices`, reads the record a page at a time, and moves with its two actions and the wheel |
| `notices-pane/vocabulary.hpp` | the pane's role, its key, its stem, its action ids and `NoticesPaneState` |
| `CMakeLists.txt`, `notices-pane/CMakeLists.txt` | the two loadable weaves, each with its manual compiled in; both need a Loom that can host loadable weaves (`loom::kernel`) |
| `docs/` | these pages |

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`notices/` adds to it.

**Law.** Three numbered laws, on [the notices' laws](docs/notices-laws.md): what the record keeps
and from whom (WL-NOTE-02), that a guest reads it only with `capture` (WL-NOTE-05), and what
Workshop says at all (WL-NOTE-06). Their proofs are listed in `tests/laws/notices.txt`. A rule
about one declaration is in that declaration's comment.

**Suites.** The folder has no suite of its own; its cases stand in suites Workshop's router
claims, each run through the official lane, `tests/verify.cmake`. `workshop_panes_inspect`
(`tests/test_workshop_panes_notices.cpp`) runs the real record and pane against what Workshop
really publishes, with the pane's census and presses; `workshop_screen`
(`tests/test_workshop_screen.cpp`) shows Workshop's band holds no notice; and `workshop_guests`
(`tests/test_workshop_guests.cpp`) a guest's reading of the record.

The rig in `tests/workshop_support.hpp` finds both weaves by their stems
(`WORKSHOP_SO_NOTICES`, `WORKSHOP_SO_NOTICES_PANE`), and the pane's rows are pinned in
`tests/actions/zengine.notices-pane.txt`.

**The host's side.**

- Workshop's plan, `workshop/default-load-plan.json`, loads both as optional rows, the record
  before the window so the launch's own notes are kept and the pane beside Attention;
  `workshop/CMakeLists.txt` stages both beside the host and lists the pane as a development pane.
- What is kept is `NoticeSaid` (`workshop/notice_seam_vocabulary.hpp`), which `WorkshopWeave::say`
  and `publish_notices` publish (`workshop/weave_document.cpp`); Workshop's grant to say it is in
  `workshop/grant.cpp`.
- The desk: `default_setup` (`workshop/setup.hpp`) seats the pane (`kNoticesPaneProvider`,
  `workshop/panes.hpp`), and `band_region` (`workshop/screen_compose.cpp`) draws the band without
  a notice.
- A guest's reading is judged in `workshop/guests.cpp` (`carries_the_desk`, `follows_the_desk`).
- The record's `keep` is one of the layouts' owner's role-wide settings (`workshop/layouts/`).
- Good practice here is [Notices](../docs/contributing/best-practices.md#notices-notices) in the
  best practices.
