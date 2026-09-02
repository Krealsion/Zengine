# Setup format version 3

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [setup-file](../workshop/setup-file.md).

**Context.** The code authors a default, the maker authors an override, the host resolves the
room: version 2 was a clean break carrying each pane's durable reference plus the smallest
authored difference (`3ecaedd`, the commit that authored the window). Loom's admission refuses
an unknown field and has no optional, so absence cannot be spelled by omission. When the lattice
moved to sub-cells, version 3 followed and a version-2 whole-cell file had to keep loading
(`07eb620`).

**Decision.** Each row carries a `PaneRef` plus `place {mode, x, y}`, `width` and `height
{mode, amount}` per axis, and `front`, and nothing else. `default` is a value whose unused
numbers are zero. A mode is a word from a closed set — two for a place, three for a size. The
format version and the envelope's shape version are one number, asserted. `pixels` is declared,
valid on every medium, and refused at projection whole. The value doors are atomic. A version-2
file is admitted against the retained v2 shapes and its cells mapped exactly (×48) onto the fine
lattice; every other version is refused by its number. An authored place is absolute, and each
axis is independent.

**Alternatives considered.**
- *Spelling absence by omitting a field, or by a magic coordinate* — rejected: admission has no
  optional, and a magic coordinate is a value a maker could mean; the zeros give absent intent
  one canonical spelling, pinned by case `"WIND-2: a default mode carries no numbers, and that
  is one canonical spelling"`.
- *An integer mode in the file* — rejected: a renumber would silently change every saved
  arrangement; an unknown word refuses the whole candidate naming what would have worked.
- *Judging the `format_version` field first* — rejected: a version-1 file would be reported as
  "a pane row is missing `place`", a true sentence about a false cause; the envelope claim gates
  first, pinned by case `"WIND-2: a version-1 file is refused BY NUMBER, before its rows are
  judged"`.
- *A per-axis fallback for `pixels`* — refused as exactly the silent default; pinned by case
  `"WIND-2: a pixel axis is setup-valid, projection-refused, and never falls back"`.
- *A migration framework for version 2* — rejected: one namespace and one exact multiply; the
  clean-break stance stands for every other transition, and the session reader keeps no old
  shape at all ([yesterday-belongs-to-a-conversion](yesterday-belongs-to-a-conversion.md)).
- *An authored place as an offset from the default* — rejected; pinned by case `"WIND-2: an
  authored place is absolute canvas position, not an offset from the default"`.

**Consequences.** A fresh setup is sparse, an unresolved reference round-trips every authored
field, and setup bytes carry no descriptor, room, handle or runtime fact. A pane with a pixel
axis is not presented on any medium, Info included, reads `refused`, and its bytes stay exact
through the refusal. A unit outranks a reservation. The setup keeps exactly one old reader
because a setup is a maker's named artifact with no session to ride.

**Laws supported.** [WL-PANE-11](../workshop/panes-and-windows.md),
[WL-SETUP-01](../workshop/setup-file.md), [WL-SETUP-02](../workshop/setup-file.md),
[WL-SETUP-03](../workshop/setup-file.md), [WL-SETUP-04](../workshop/setup-file.md),
[WL-SETUP-05](../workshop/setup-file.md), [WL-SETUP-06](../workshop/setup-file.md),
[WL-SETUP-08](../workshop/setup-file.md).
