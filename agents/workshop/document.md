# Workshop law — the typed rows, and the object document that retired

Register `WL-DOC`, the model half. It held the maker's object document — the prototype canvas's
authored rectangles and the operations on them — and that document retired with its canvas: a
maker arranges desks, layouts and panes now, and Info inspects a pane (WL-INFO-14). What outlived
it is the typed connection every editable row is built on (WL-DOC-02). The file half is
[`document-file.md`](document-file.md). One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

**What a retired id means here.** Each keeps its number and one line, so a citation elsewhere
still says what became of it; the reasoning stays in its decision record, and a maker's old
document file is said and left alone (WL-DOC-22).

## WL-DOC-01 — RETIRED: identity was the id, not the name, among the canvas's objects

WHY — `agents/decisions/the-document-model.md`

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

## WL-DOC-03 — RETIRED: an object's name was refused empty and past sixty-four bytes

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-04 — RETIRED: the extent text form was canonical out and typeable in

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-05 — RETIRED: an object's authored and resolved sizes were different facts

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-06 — RETIRED: a move was one authored change

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-07 — RETIRED: a resize was one authored change

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-08 — RETIRED: the authored minimum was the document's; a stop was no refusal

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-09 — RETIRED: a drag took hold of what the maker could see

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-10 — RETIRED: creating minted an identity, and deleting removed exactly one

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-11 — RETIRED: a context was authored by identity, a bad one refused by name

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-12 — RETIRED: composition re-resolved and rewrote nothing

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-17 — RETIRED: the workspace was the root frame

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-18 — RETIRED: the canvas, the object list and the inspector read one document

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-20 — RETIRED: the document crossed the pane seam as a picture (now WL-INFO-14)

WHY — `agents/decisions/a-presentation-owns-no-facts.md`

## WL-DOC-21 — RETIRED: a commit was written only to the subject named (now WL-INFO-15)

WHY — `agents/decisions/a-commit-names-its-subject.md`

## Do not assume

- That an old object document is read at launch: it is named and left alone (WL-DOC-22); the
  desk, the window, the keymap and the prefs are what a launch reads (WL-SESSION-01).
- That a retired law's discipline was dropped: a named subject, a refusal before any row is read
  and a picture said only when it changed are Info's, over a pane (WL-INFO-14, WL-INFO-15).
