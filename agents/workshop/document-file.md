# Workshop law — the document file

Register `WL-DOC`, the file half: what the document file carries, how it refuses, and the two
gestures that write and read it. The model's laws stay in [`document.md`](document.md), and the
ids are one series. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-DOC-13 — The document file carries what a maker made, and no resolved geometry

LAW — `zengine-workshop` version 1 holds identity, version, objects and the mint, no resolved geometry; a round trip is byte-identical, and saving normalizes nothing and touches no document.

PROVEN BY — `workshop/persist.hpp` `kFormatVersion`, `to_text`, `from_text`, `WorkshopDocument`,
`WorkshopObject`, `to_document`, `mode_in`; `workshop/vocabulary.hpp` `WorkshopDoc`;
`tests/test_workshop_persistence.cpp` case `"save -> load -> save is byte-identical"`, case
`"saving does not touch the document"`, case `"the file a person can read: identity, version,
objects, and the mint"`, case `"the file carries no resolved geometry, and the scene is rebuilt
from what it does"`, case `"a save normalizes nothing: 60% is not written as the cells it happens
to be"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-14 — A malformed document never leaves Workshop halfway loaded

LAW — The candidate meets `check_document`, the refusal names the fact in a maker's words, and `restore` keeps the candidate's identities rather than minting new ones.

PROVEN BY — `workshop/document.hpp` `check_document`, `restore`; `workshop/persist.hpp`
`load_into`, `Loaded`; `tests/test_workshop_persistence.cpp` case `"a malformed document never
leaves Workshop halfway loaded"`, case `"a refusal says which fact was wrong, in words a maker can
act on"`, case `"restore keeps the candidate's identities rather than minting new ones"`;
`tests/test_workshop_screen.cpp` case `"a forged relationship never leaves Workshop halfway
loaded"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-15 — The file doors refuse before they harm

LAW — A missing file is an ordinary refusal, a failed write leaves the last good save readable, a save into a missing place refuses first, and a file over `kMaxDocumentBytes` is refused unread.

MEANS
- `pending_path` is the safe write's staging name; the family's other files share the door.

PROVEN BY — `workshop/persist.hpp` `read_file`, `write_file`, `pending_path`,
`kMaxDocumentBytes`, `save_file`, `load_file`; `tests/test_workshop_persistence.cpp` case `"a
missing file is an ordinary refusal, not a crash and not an empty document"`, case `"a detected
write failure leaves the last good save readable and unchanged"`, case `"a save into a place
that does not exist refuses before it writes anything"`, case `"a file too large to be a document
is refused before it is read"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-16 — `^s` saves and `^o` loads through the real message path

LAW — `^s` refuses while a row is being edited and writes nothing; a bare `s` or `o` is not a command; with no document file, save and open say so instead of guessing one.

MEANS
- a successful load cancels a drag, re-establishes the selection and continues no old resize;
- a failed load costs a maker nothing but the notice.

PROVEN BY — `workshop/weave.hpp` `save_document`, `load_document`; `workshop/keymap.hpp`
`document.save`, `document.open`; `tests/test_workshop_persistence.cpp` case `"^s saves and ^o
loads, through the real message path"`, case `"^s refuses while a row is being edited, and writes
nothing"`, case `"a bare s and a bare o are not commands, and Ctrl is what makes them one"`,
case `"a successful load cancels a drag and cannot continue an old resize"`, case `"a failed load
costs a maker nothing but the notice"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-19 — The status line says which file, and `saved` is computed by comparing

LAW — The status slot says how many objects, which is selected, which file, and `saved` or `UNSAVED` by comparing the document with its saved copy; with no document file it names none and claims nothing.

MEANS
- a fresh Workshop says `UNSAVED`: its opening document has never been written;
- a document edited and then edited back says `saved`, because it is;
- a setup save or restore moves the document's status not at all.

PROVEN BY — `workshop/weave.hpp` `status_line`; `surface/vocabulary.hpp` `kSlotStatus`;
`tests/test_workshop_persistence.cpp` case `"^s saves and ^o loads, through the real message
path"`, case `"with no document file, save and open say so instead of guessing one"`, case
`"saving and restoring a setup does not touch the document or its saved status"`.
WHY — `agents/decisions/the-document-model.md`
