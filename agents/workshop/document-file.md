# Workshop law — the file doors, and the document file that retired

Register `WL-DOC`, the file half: the doors every durable artifact reads and writes through,
what a weaver's old object document meets now, and the status line that used to name it. The
model's laws are in [`document.md`](document.md), and the ids are one series. One law per
heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

Retired: WL-DOC-13, WL-DOC-14, WL-DOC-16, WL-DOC-19.

## WL-DOC-15 — The file doors refuse before they harm

LAW — A missing file is an ordinary refusal, a failed write leaves the last good save readable, a save into a missing place refuses first, and a file past its reader's bound is refused unread.

MEANS
- `pending_path` is the safe write's staging name; every durable artifact shares the doors;
- the bound is the caller's (`read_file`'s `most`), and the refusal names what the file claims.

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-22 — An old object document is said once and left exactly as it is

LAW — A `--document` path, or `workshop.json` under the project, is named once at startup as left alone; no door reads, rewrites or deletes it, and its bytes are never taken for another file's.

MEANS
- `^s` and `^o` answer nothing; a keymap row naming either is kept and said at load (WL-KEY-06);
- an old document offered as a setup, a session or a desk is refused as the wrong kind of file.

DOES NOT MEAN
- a conversion: nothing of an object document becomes a pane, a desk or a layout.

WHY — `agents/decisions/the-document-model.md`

## WL-DOC-23 — The status line names the live layout and its panes, and claims no file

LAW — The status slot says the live layout's name and how many panes its desk names; it names no document file and claims no saved state, which is the layout band's to say.

WHY — `agents/decisions/the-document-model.md`

## Proven by

### WL-DOC-15
`workshop/persist.hpp` read_file write_file pending_path FileText
`tests/test_workshop_persistence.cpp` "a missing file is an ordinary refusal, not a crash and not an
empty file" "a detected write failure leaves the last good save readable and unchanged" "a save into
a place that does not exist refuses before it writes anything" "a file too large to be what it
claims is refused before it is read"

### WL-DOC-22
`workshop/weave.hpp` HostContext::retired_document
`workshop/persist.hpp` kRetiredDocumentName
`workshop/weave_handlers.cpp` retired_document
`workshop/workshop.cpp` retired_document kRetiredDocumentName
`tests/test_workshop_persistence.cpp` "an old object document is left exactly as it is: a launch
names it once, and nothing reads, rewrites or deletes it" "an old object document's file and the
setup's file cannot be mistaken for each other"

### WL-DOC-23
`workshop/weave_document.cpp` status_line
`surface/vocabulary.hpp` kSlotStatus
`tests/test_workshop_document.cpp` "the status line names the live layout and its panes, and claims
no file"
