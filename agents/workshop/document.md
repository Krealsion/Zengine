# Workshop law — the typed rows, and the object document that retired

Register `WL-DOC`, the model half. It held the weaver's object document — the prototype canvas's
authored rectangles and the operations on them — and that document retired with its canvas: a
weaver arranges desks, layouts and panes now, and Info inspects a pane (WL-INFO-14). What outlived
it is the typed connection every editable row is built on (WL-DOC-02). The file half is
[`document-file.md`](document-file.md). One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

Retired: WL-DOC-01, WL-DOC-03, WL-DOC-04, WL-DOC-05, WL-DOC-06, WL-DOC-07, WL-DOC-08, WL-DOC-09, WL-DOC-10, WL-DOC-11, WL-DOC-12, WL-DOC-17, WL-DOC-18, WL-DOC-20, WL-DOC-21.

## WL-DOC-02 — A property is read through its semantic surface and written by commit

LAW — Unparseable text writes nothing, a refused value is a different outcome with its reason, a shown row takes no write, and two properties of one type share one conversion.

MEANS
- `TextForm<T>` is written once per type: canonical out, nothing but that type in;
- `Row::commit_text` is the one write door; a draft is the inspector's own (WL-INFO-15).

PROVEN BY — `workshop/property.hpp` `Row`, `Commit`, `Written`, `TextForm`, `Property`,
`TextForm::format`, `TextForm::expected`, `Row::show`, `Row::commit_text`;
`tests/test_workshop_document.cpp` case `"a successful commit writes through the semantic
setter"`, case `"unparseable text leaves the property untouched and says so"`, case `"a
parseable value the property refuses is a DIFFERENT outcome, with its reason"`, case
`"unparseable text writes nothing even where a default WOULD be accepted"`, case `"reuse: two
properties of one type share every line of conversion"`, case `"a shown row cannot be edited,
because it has nothing to write to"`, case `"the whole-number text form: canonical out, and
nothing but a whole number in"`.
WHY — `agents/decisions/the-document-model.md`

## Do not assume

- That an old object document is read at launch: it is named and left alone (WL-DOC-22); the
  desk, the window, the keymap and the prefs are what a launch reads (WL-SESSION-01).
- That nothing holds a commit to its subject: a named subject, a refusal before any row is read
  and a picture said only when it changed are Info's, over a pane (WL-INFO-14, WL-INFO-15).
