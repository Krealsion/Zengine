# The face reports the unit

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [geometry](../workshop/geometry.md).

**Context.** Workshop held one authored pane geometry on a medium-independent lattice and could
only spell it one way: `40+1/4` cells, an exact mixed number that is unreadable on a window and
says nothing about the pixels a hand there actually moved. It could not do better, and the
reason was custody rather than arithmetic: turning a cell into a device pixel needs a number
only a Skin knows, and an application holding one Skin's layout number is correct only for as
long as it has one medium (`35653ad`, "Let each face say a pane's geometry in the unit it can
actually see").

**Decision.** The medium says it. `SurfaceExtent` carries `cell_px` beside the text metric;
`adopt_screen` takes it and `Session::cell_px` holds it; zero means "my device unit IS the
cell", every terminal's permanent answer. Workshop derives no unit, and a change of unit alone
is a change. One derivation — five pure functions over `surface::device_of_subs`, the same
arithmetic the shipped face paints and hit-tests by — spells a pane's geometry in the face's
unit: no unit type, no registry, no per-medium table, no second conversion constant. A value not
exact in the face's unit wears `~`, and the line says `(~ projected)` once. Looking is not
authoring. The notice says where a pane the maker did not place actually is.

**Alternatives considered.**
- *The exact mixed number* (`subcell_text`) — retired: exact and unreadable on a window, where
  `126` is exact (`git log -S'subcell_text'` → `35653ad`).
- *Deriving the unit from one Skin's constant inside Workshop* — rejected, as the pointing rule
  forbids it; pinned by case `"WUX-6: the canvas's device unit is the medium's answer, never
  Workshop's"`.
- *A unit system* (a type, a registry, a per-medium table) — rejected: one derivation; pinned by
  case `"WUX-6: one authored value, spelled in whatever unit the active face reported"`.
- *Presenting a rounded value as the stored one* — refused, the lattice's own stop condition
  kept with a different spelling: the mark is the distinction.

**Consequences.** A maker reads `@77,53 417x233 px` on the shipped window and `@~6,~4 ~34x~19
cells (~ projected)` in a terminal, of the same desk. A session that crosses both media reading
a geometry no terminal can say writes the same file byte for byte, and the unit reaches no
durable file; a restore hands this run's unit straight back. An axis authored in `pixels` keeps
its own inline `px` (`483x220px px`, state `refused`). The mark is ASCII because the shipped
face's letterform is 0x20–0x7E. A partly reactive window is followed by ` -- now @x,y WxH
<unit>` from the unclipped ask, the rectangle a gesture measures from.

**Laws supported.** [WL-GEO-08](../workshop/geometry.md), [WL-GEO-09](../workshop/geometry.md),
[WL-GEO-10](../workshop/geometry.md), [WL-GEO-11](../workshop/geometry.md),
[WL-GEO-12](../workshop/geometry.md).
