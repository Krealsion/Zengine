# Workshop law — the setup file

Register `WL-SETUP`: the setup file's shape, its one legacy reader, and one spelling per fact.
One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-SETUP-01 — A setup row is a reference plus the smallest authored difference

LAW — The setup file is format version 3: each pane row carries a durable pane reference plus `place {mode,x,y}`, `width` and `height {mode,amount}` per axis, `front`, and nothing else.

MEANS
- a fresh setup is sparse: the developer's defaults are absent, not written;
- an unresolved reference round-trips every authored field exactly;
- setup bytes carry no descriptor, room, handle or runtime fact.

PROVEN BY — `workshop/setup_persist.hpp` `kFormatVersion`, `WorkshopSetup`, `to_text`,
`WorkshopPaneSize`, `WorkshopSetupPane`; `workshop/setup.hpp` `PaneRef`, `kMaxPaneKeyLen`,
`PaneSize`, `SetupPane`, `pane_ref_of`, `kNoPaneRow`, `kMaxSetupPanes`; `workshop/panel.hpp`
`kWorkshopProvider`, `PanelKind`, `every_kind_is_referable`;
`tests/test_workshop_panes_window.cpp` case `"WIND-2: a fresh setup is version 3, sparse, and
carries the identity ranks"`, case `"WIND-2: an unresolved reference round-trips every authored
field exactly"`; `tests/test_workshop_panes_seam.cpp` case `"setup bytes carry no descriptor, room
or handle"`.
WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-02 — A version-2 whole-cell setup still loads, and nothing else old does

LAW — A version-2 file is admitted against the retained v2 shapes and its cells are mapped exactly (×48) onto the fine lattice; every other version is refused by its number.

MEANS
- an old desk resolves to the identical pixels and characters; the next explicit save writes v3;
- this is one namespace and one multiply, not a migration framework.

PROVEN BY — `workshop/setup_persist.hpp` `v2`, `setup_in`, `from_text`, `setup_in_v2`;
`surface/vocabulary.hpp` `kCellSubs`; `workshop/session_history.hpp` `place_v2_to_v3`,
`desk_v2_to_v3`; `tests/test_workshop_screen.cpp` case `"WUX-2: a version-2 whole-cell setup loads
at exactly its old picture"`; `tests/test_workshop_panes_window.cpp` case `"WIND-2: a version-1
file is refused BY NUMBER, before its rows are judged"`.
WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-03 — `default` is a value whose unused numbers are zero

LAW — Absent intent has exactly one spelling: a `default` mode carrying a number is refused, naming the axis.

MEANS
- admission has no optional field, so absence cannot be spelled by omitting one;
- a magic coordinate is a value a maker could otherwise mean.

PROVEN BY — `workshop/setup.hpp` `check_pane_place`, `check_pane_size`,
`check_pane_place_coord`, `pane_unit::kDefault`, `PanePlace`;
`tests/test_workshop_panes_window.cpp` case `"WIND-2: a default mode carries no numbers, and that
is one canonical spelling"`.
WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-04 — A mode is a word from a closed set

LAW — A place has two mode words and a size three; an unrecognised word refuses the whole candidate, naming what it found and what would have worked.

MEANS
- the in-memory numbers are arbitrary: a renumber would silently change every saved arrangement;
- `pixels` offered to a place is a word that field's vocabulary does not have.

PROVEN BY — `workshop/setup_persist.hpp` `from_text`, `kUnitDefault`, `kUnitSubcells`,
`kPlaceWords`, `unit_word`; `workshop/setup.hpp` `pane_unit`, `pane_unit::kSubcells`;
`tests/test_workshop_panes_window.cpp` case `"WIND-2: an unknown mode word names what it found and
what would have worked"`, case `"WIND-2: every mode spelling round-trips, pixels included"`.
WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-05 — The format version and the envelope's version are one number

LAW — The format version and the envelope's shape version are one number, asserted: a wrong-version file is refused on its claim before a row is read, and the in-file version only catches forgery.

MEANS
- a version-1 file can never be reported as "a pane row is missing `place`";
- the refusal leaves the live setup and its on-file copy untouched.

PROVEN BY — `workshop/setup_persist.hpp` `kFormatVersion`, `WorkshopSetup`, `from_text`,
`format_version`, `wrong_version`; `tests/test_workshop_panes_window.cpp` case `"WIND-2: a
version-1 file is refused BY NUMBER, before its rows are judged"`, case `"WIND-2: a version-1 file
leaves the live setup and its on-file copy untouched"`.
WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-06 — `pixels` is declared, valid everywhere, and refused at projection

LAW — A pane with either axis in `pixels` is not presented on any medium, Info included; its bytes stay exact, reset and order still recover it, and there is no per-axis fallback.

MEANS
- no medium here publishes a trustworthy per-axis device-pixel scale for a canvas cell;
- a unit outranks a reservation in `arrange_geometry_ready`, as in `pane_state_of`.

DOES NOT MEAN
- that fixed placement is permission to present an unsupported unit as understood.

PROVEN BY — `workshop/screen.hpp` `pane_unit_projectable`, `pane_state_of`; `workshop/setup.hpp`
`pane_unit`, `kMaxPanePixels`, `kPixels`, `check_pane_size`; `workshop/weave.hpp`
`arrange_geometry_ready`; `tests/test_workshop_panes_window.cpp` case `"WIND-2: a pixel axis is
setup-valid, projection-refused, and never falls back"`; `tests/test_workshop_screen.cpp` case
`"WIND-2a: a pixel axis refuses every current pane projection, Info included"`.
WHY — `agents/decisions/setup-format-v3.md`

## WL-SETUP-07 — `front` is a canonical rank, never a counter

LAW — `front` is a permutation of 0..n−1 over all authored rows, unresolved included; a gapped or duplicated rank is refused, and reset writes the bytes of a never-reordered setup.

MEANS
- there is no tie, so the resolved order needs no secondary key;
- the presented order restricts the permutation to what is seated: an absent pane keeps its rank.

DOES NOT MEAN
- that `max + 1` would do — it is an operation trace, and a legal gesture would eventually fail.

PROVEN BY — `workshop/setup.hpp` `send_to_front`, `send_to_back`, `raise_one`, `lower_one`,
`reset_front`, `check_setup`, `add_pane`, `remove_pane`, `pane_at_front`, `SetupPane::front`,
`default_setup`; `tests/test_workshop_panes_window.cpp` case `"WIND-2: every ordering operation is
an exact permutation, ends included"`, case `"WIND-2: a gapped or duplicated rank is refused, and
a fresh one is not"`, case `"WIND-2: 10,000 alternating ordering operations stay inside 0..n-1"`.
WHY — `agents/decisions/front-is-a-permutation.md`

## WL-SETUP-08 — The value doors are atomic

LAW — `author_pane_place` and `author_pane_size` write nothing when a value is refused, on either axis; an inverse edit that restores the bytes makes the setup match its file again.

PROVEN BY — `workshop/setup.hpp` `author_pane_place`, `author_pane_size`;
`tests/test_workshop_panes_window.cpp` case `"WIND-2: a refused VALUE writes nothing, on either
axis"`, case `"WIND-2: dirty is structural -- an inverse edit makes a setup clean again"`.
WHY — `agents/decisions/setup-format-v3.md`

## Do not assume

- That the setup keeps no old reader — it keeps exactly the v2 one, because a setup is a named
  artifact with no session to ride (WL-SETUP-02); the session reader keeps none (WL-MIG-01).
