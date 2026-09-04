# A layout is a lifted value

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [layouts](../workshop/layouts.md) and [session](../workshop/session.md).

**Context.** A maker could arrange one desk and name it; keeping a second meant a second file
and a second launch (`102017a`, "Keep several desks in one Workshop, and let a tab run say which
one you are on"). Then only the live layout came back after a restart (`a39795e`, "Bring every
layout back, and let the reader forget the shape that held one"). And one comparison copy —
`on_file` and `saved()` — spoke for every layout at once against a file only one of them had
anything to do with; naming meant accepting a write to a named artifact; `=` copied the desk you
were on; order could not be changed (`2dc7626`, "Let a maker treat layouts as tabs, and a Setup
file as one desk they chose to keep").

**Decision.** A layout is a desk plus its optional association with one Setup file.
`SetupState` holds the live desk every consumer still reads, its link, the shelf of inactive
layouts as values, the live position, and the rename editor; `Layout` is desk + link, one struct.
`link_status` decides `none`, `current` or `modified` about Workshop's knowledge, never the disk.
The run is the shelf with one element lifted out, and it leaves the state through one inverse
pair. Duplicate copies the desk and clears the association; renaming is a layout operation and
saving a file operation; the association follows a success; the shared-artifact law updates
every association to a path, compared by bytes. Per-layout is the value's own fields. One pane
in two layouts is one pane. `kMaxLayouts` refuses a ninth. The session holds the run with
associations, admitted by four questions plus the link.

**Alternatives considered.**
- *A swap when switching* — the shorter spelling, measured wrong: it reorders the run on its
  second hop; pinned by case `"WUX-9/SC-3: switching never reorders the run, and the live value
  never doubles"`.
- *`on_file` + `saved()`* — retired (`git log -S'on_file'` → `2dc7626`), and `UNSAVED` with
  them.
- *Two parallel vectors of desks and links* — rejected: the first `erase` that forgets one is a
  layout wearing another layout's association.
- *Index surgery on the shelf for move and duplicate* — rejected: a third spelling of the lift;
  the live position is computed, never searched, since two layouts may hold equal values.
- *Duplicate inheriting the association* — refused: the first `s` would overwrite the very file
  the maker duplicated in order not to touch; pinned by case `"WUX-11/SC-2: duplicate copies the
  desk exactly and always clears the association"`.
- *`s` opening the name editor and writing on commit* — retired: a typo fix forced a write to a
  named file.
- *The configured `--setup` path as a default association* — refused: the acquisition door.
- *Advancing only the acting layout's baseline on a shared artifact* — rejected; pinned by case
  `"WUX-11/SC-12: two layouts sharing one artifact cannot both claim `current`"`.
- *A digest instead of a whole desk in the association* — rejected: costs the ability to say
  why, and a restore must never re-read what it refers to.
- *Per-layout selection or keyboard focus* — rejected: a second store of a derived fact.
- *Dropping the ninth layout, or `x` for removal* — refused: refuse rather than drop; `x` once
  closed the Builder and a hand may still mean that.

**Consequences.** `kMaxSessionBytes` is `kMaxLayouts * (2 * kMaxSetupBytes + kMaxLinkPathBytes)`
— two desks and a path — so a session this build writes is never one it refuses. A pane in
both layouts at unchanged capacity hears nothing across four switches, and an inactive layout is
an unread value with no catalog fanout. A half-association is refused. `s` and `r` still act on
one desk, and a Setup file still means one desk.

**Laws supported.** [WL-LAYOUT-01](../workshop/layouts.md),
[WL-LAYOUT-02](../workshop/layouts.md), [WL-LAYOUT-03](../workshop/layouts.md),
[WL-LAYOUT-04](../workshop/layouts.md), [WL-LAYOUT-05](../workshop/layouts.md),
[WL-LAYOUT-06](../workshop/layouts.md), [WL-LAYOUT-07](../workshop/layouts.md),
[WL-LAYOUT-08](../workshop/layouts.md), [WL-LAYOUT-09](../workshop/layouts.md),
[WL-LAYOUT-10](../workshop/layouts.md), [WL-LAYOUT-11](../workshop/layouts.md),
[WL-LAYOUT-12](../workshop/layouts.md), [WL-LAYOUT-13](../workshop/layouts.md),
[WL-SESSION-05](../workshop/session-restore.md), [WL-SESSION-06](../workshop/session-restore.md).
