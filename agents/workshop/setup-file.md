# Workshop law — the setup file

Register `WL-SETUP`: the setup file's shape, its three retained readers, and one spelling per fact.
One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

Retired: WL-SETUP-06.

## WL-SETUP-01 — A setup row is a reference plus the smallest authored difference

LAW — The setup file is format version <!-- value setup_persist::kFormatVersion -->6<!-- /value -->: each pane row carries a durable pane reference plus `place {mode,x,y}`, `width` and `height {mode,amount}`, `front`, its `settings`, and nothing else.

MEANS
- a fresh setup is sparse: the developer's defaults are absent, not written;
- an unresolved reference round-trips every authored field exactly;
- setup bytes carry no descriptor, room, handle or runtime fact.

WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-02 — An old setup opens where it stood in its room

LAW — An older setup's places and extents cross at the door: each edge floored to the pixel it painted, a place said from the room's top, two cells below the canvas's; other versions are refused.

MEANS
- version 4 reads with no settings, 3 at a floor of four sub-units, 2 times twelve; an extent is its span;
- a place above that room's top lands at the room's top, the nearest place the room has;
- an old `pixels` extent under one cell is raised to one cell; the shapes are three namespaces.

WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-03 — `default` is a value whose unused numbers are zero

LAW — Absent intent has one spelling: a `default` mode carrying a number is refused, naming the axis; so is any word carrying a number it does not read, missing one it reads, or past its range.

MEANS
- a place's and a size's fields are required, so omitting one spells nothing;
- a file's `right-column`, a centred axis, `fit` and `fill` read no number: one beside them is a second answer, a value a weaver could otherwise mean;
- a share is 1 to 10000 ten-thousandths of the room, an offset 0 to 65536 pixels, a pixel extent a cell to 65536.

WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-04 — A mode is a word from a closed set

LAW — A place's word is one of ui's ten and a size's one of its five, both lists spelled by one table; an unrecognised word refuses the whole candidate, naming what it found and what would have worked.

MEANS
- the in-memory numbers are arbitrary: a renumber would silently change every saved arrangement;
- a file's `right-column` reads as `top-right` at no offset and is written as that; offered to a size, it is a word that field's vocabulary does not have;
- adding a word is not a version: the shape is unchanged and an older build refuses out loud.

WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-05 — The format version and the envelope's version are one number

LAW — The format version and the envelope's shape version are one number, asserted: a wrong-version file is refused on its claim before a row is read, and the in-file version only catches forgery.

MEANS
- a version-1 file can never be reported as "a pane row is missing `place`";
- the refusal leaves the live setup and its on-file copy untouched.

WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-07 — `front` is a canonical rank, never a counter

LAW — `front` is a permutation of 0..n−1 over all authored rows, unresolved included; a gapped or duplicated rank is refused, and reset writes the bytes of a never-reordered setup.

MEANS
- there is no tie, so the resolved order needs no secondary key;
- the presented order restricts the permutation to what is seated: an absent pane keeps its rank.

DOES NOT MEAN
- that `max + 1` would do — it is an operation trace, and a legal gesture would eventually fail.

WHY — `agents/decisions/front-is-a-permutation.md`

## WL-SETUP-08 — The value doors are atomic

LAW — `author_pane_place` and `author_pane_size` write nothing when a value is refused, on either axis; an inverse edit that restores the bytes makes the setup match its file again.

MEANS
- `author_pane_setting` too, and it stores a value equal to the pane's default as its absence.

WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-09 — A setup name is four rules and a byte count

LAW — A setup name is present, more than spaces, free of control characters and at most `kMaxSetupNameLen` bytes; a typed name and a loaded one meet the one function, and the refusal says bytes.

MEANS
- spaces inside a name are legal; a control character is refused, never rendered safe;
- no Unicode policy: valid UTF-8 is the gate's answer, and nothing counts a code point or a cell.

WHY — `agents/decisions/a-name-is-judged-in-bytes.md`

## WL-SETUP-10 — A pane key is judged by shape and never by meaning

LAW — Either half of a `PaneRef` is present, at most `kMaxPaneKeyLen` bytes and free of whitespace and control bytes, judged as a view before anything owns a copy; whether it names anything is not an error.

MEANS
- the refusal says which half: `provider` and `pane key` are two fields a weaver tells apart;
- a descriptor's keys meet the same function, so no key a saved setup could not spell is admitted.

WHY — `agents/decisions/a-name-is-judged-in-bytes.md`

## WL-SETUP-11 — The setup file is written through the family's safe write

LAW — A setup is saved by `persist::write_file`, a whole candidate renamed over the destination, so a failed write leaves the last good setup file whole; crash durability is not claimed.

MEANS
- text past `kMaxSetupBytes`, which the reader would refuse, is refused at the save, nothing written.

WHY — `agents/decisions/setup-format-v3.md`

## Do not assume

- That the setup keeps no old reader — it keeps the v2, v3 and v4 ones, because a setup is a
  named artifact with no session to ride (WL-SETUP-02); the session reader keeps none (WL-MIG-01).

## Proven by

### WL-SETUP-01
`workshop/setup_persist.hpp` kFormatVersion WorkshopSetup to_text WorkshopPaneSize WorkshopSetupPane
WorkshopPanePlace
`workshop/setup.hpp` PaneRef kMaxPaneKeyLen PaneSize SetupPane pane_ref_of kNoPaneRow kMaxSetupPanes
pane_row
`workshop/panes.hpp` kWorkshopProvider BuiltinPane every_kind_is_referable
`tests/test_workshop_panes_window.cpp` "a fresh setup is the current version, sparse, and carries
the identity ranks" "an unresolved reference round-trips every authored field exactly"
`tests/test_workshop_panes_seam.cpp` "setup bytes carry no descriptor, room or handle"
`tests/test_workshop_persistence.cpp` "a setting rides its row through the setup file in its own
kind's field" "every place and size word survives the setup file and the session byte for byte"

### WL-SETUP-02
`workshop/setup_persist.hpp` v2 v3 v4 setup_in from_text setup_in_v2 v3::to_v4 v4::to_current
v3::kSubsPerPixel at_least_a_cell kCanvasRoomTopPx room_place_of_canvas px_of_saved_cells v5
to_current to_v5 lifted
`workshop/setup.hpp` kSavedCellPx
`workshop/layouts/session_history.hpp` place_v2_to_v3 desk_v2_to_v3 desk_v3_to_v4 desk_v4_to_v5
desk_v5_to_v6
`tests/test_workshop_screen.cpp` "a version-2 whole-cell setup loads at exactly its old picture" "a
fine setup opens where the window painted it, its place in the room" "a place in an older file keeps
its place in the room"
`tests/test_workshop_panes_window.cpp` "a version-1 file is refused BY NUMBER, before its rows are
judged"
`tests/test_workshop_persistence.cpp` "a version-4 setup file reads with no settings, and the next
save writes the current version"

### WL-SETUP-03
`workshop/setup.hpp` check_pane_place check_pane_size check_pane_place_coord pane_unit::kDefault
PanePlace kMaxPanePixels default_setup
`workshop/screen_chrome.cpp` bounds_of
`tests/test_workshop_panes_window.cpp` "a default mode carries no numbers, and that is one canonical
spelling" "a fresh setup is the current version, sparse, and carries the identity ranks" "a file's
`right-column` reads as `top-right` at no offset, and is written as that"
`tests/test_workshop_persistence.cpp` "a word's numbers out of its range are refused in the weaver's
words, by the file and by the owner"

### WL-SETUP-04
`workshop/setup_persist.hpp` from_text kUnitDefault kUnitPixels kUnitRightColumn place_word_of
size_word_of place_in WorkshopPaneSize WorkshopPanePlace size_in
`workshop/setup.hpp` pane_unit kPixels pane_place_words pane_size_words word_list
`ui/vocabulary.hpp` kPlaceWords kSizeWords place_named size_named place_word size_word
`tests/test_workshop_panes_window.cpp` "an unknown mode word names what it found and what would have
worked" "every mode spelling round-trips, pixels included" "a file's `right-column` reads as
`top-right` at no offset, and is written as that"
`tests/test_workshop_persistence.cpp` "every place and size word survives the setup file and the
session byte for byte"
`tests/test_ui.cpp` "each word is spelled by one table, and reads exactly its own numbers"

### WL-SETUP-05
`workshop/setup_persist.hpp` kFormatVersion WorkshopSetup from_text WorkshopSetup::format_version
wrong_version
`tests/test_workshop_panes_window.cpp` "a version-1 file is refused BY NUMBER, before its rows are
judged" "a version-1 file leaves the live setup and its on-file copy untouched"

### WL-SETUP-07
`workshop/setup.hpp` send_to_front send_to_back raise_one lower_one reset_front check_setup add_pane
remove_pane pane_at_front SetupPane::front default_setup desk_intent
`ui/vocabulary.hpp` problem
`tests/test_workshop_panes_window.cpp` "every ordering operation is an exact permutation, ends
included" "a gapped or duplicated rank is refused, and a fresh one is not" "10,000 alternating
ordering operations stay inside 0..n-1"

### WL-SETUP-08
`workshop/setup.hpp` author_pane_place author_pane_size author_pane_setting
`tests/test_workshop_panes_window.cpp` "a refused VALUE writes nothing, on either axis" "dirty is
structural -- an inverse edit makes a setup clean again"
`tests/test_workshop_panes_settings.cpp` "a pane's settings are part of what makes a linked layout
modified"

### WL-SETUP-09
`workshop/setup.hpp` check_setup_name kMaxSetupNameLen
`tests/test_workshop_persistence.cpp` "what this application accepts as a setup name" "the name and
key bounds are BYTES, and the refusal says bytes"

### WL-SETUP-10
`workshop/setup.hpp` check_pane_key check_pane_ref kMaxPaneKeyLen
`tests/test_workshop_persistence.cpp` "what this application accepts as either half of a reference"
`tests/test_workshop_panes_seam.cpp` "a descriptor's two keys are judged by the setup file's own
law" "an office longer than the key bound is delivered whole and admitted by nobody"

### WL-SETUP-11
`workshop/setup_persist.hpp` save_file load_file persist::write_file kMaxSetupBytes
`tests/test_workshop_persistence.cpp` "a detected setup write failure leaves the last good setup
file untouched" "a setup file on disk is the setup read back from it" "a setup past its byte ceiling
is refused at the save, and nothing is written"
