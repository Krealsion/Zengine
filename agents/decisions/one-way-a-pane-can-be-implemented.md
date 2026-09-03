# One way a pane can be implemented

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [maker-pane](../workshop/maker-pane.md).

**Context.** Every pane's interior was code, or a provider's behind its seam; a maker could not
make one (`df06ac5`, "Let a maker create a pane, because a pane can be data").

**Decision.** `PaneDefinition{name, regions[], next_id}` with `TextRegion{id, kind, x, y, w, h,
text}` — one admitted kind, ids minted and never reused, geometry in sub-units relative to the
pane's interior — is the first pane implementation whose interior is authored data, and one is
open. The identity is minted from the name under a Workshop-owned provider namespace no office
may offer a pane in. `kMakerPaneKind` is a handle, a third kind class. The pane on the desk is
the preview; there is no second renderer. Looking never authors. One owner door per fact. The
lifecycle is the source editor's, inherited whole. The definition loads before the session
restores. The pane file is version 1 and what it cannot say is the enforcement. The Pane Creator
is the maker-facing workflow. The code-backed answer is a capture.

**Alternatives considered.**
- *A singleton `Defined` ref whose meaning follows the open file* — rejected: a definition file
  moving changes nothing, and a definition not open leaves every row naming it retained and
  `unresolved`; pinned by case `"WUX-14/SC-4: a maker pane's identity is its name under
  Workshop's namespace -- not a singleton that follows the open file"`.
- *Converting built-ins and providers into this representation* — refused: this is one way a
  pane can be implemented, not the ontology of pane; a future tool asks a pane what it exposes.
- *A `CustomizablePane`, a widget set, controls, anchors, fill, nesting, a second renderer* —
  refused.
- *Resolving against the runtime catalog alone* — rejected: it would count a maker's pane
  unresolved beneath a pane they can see; the resolution table takes the whole `Panels`.
- *Rewriting an authored number to fit the face* — refused; pinned by case `"WUX-14/SC-8: a
  region too small for the face is the face's own answer, and the authored value is not
  rewritten to fit"`.
- *Loading the definition after the session* — rejected: the restore seats only what resolves at
  that moment, so the saved pane would come back `unresolved`.
- *The picker's refusal at the minimum composition* — rejected: a new pane lands `waiting`, says
  so, and stays the editable subject.
- *Decompiling or inferring controls for a code-backed subject* — none.

**Consequences.** The default region is 0, 0, 24 by 2 cells, two tall so the shipped face sets
one row of type. The file's 64 KiB ceiling is derived from the region bounds; both headers are
tripwired against every bus, kernel, grant, operator and keymap spelling, and an office in the
maker namespace hears nothing across a load, a press, a key and a wheel. The refused-file wall
compares two spellings of one path through one function, which the MSVC lane found was needed on
Windows. The session carries the row and not one byte of the interior.

**Laws supported.** [WL-MAKER-01](../workshop/maker-pane.md),
[WL-MAKER-03](../workshop/maker-pane.md), [WL-MAKER-04](../workshop/maker-pane.md),
[WL-MAKER-05](../workshop/maker-pane.md), [WL-MAKER-06](../workshop/maker-pane.md),
[WL-MAKER-07](../workshop/maker-pane.md), [WL-MAKER-08](../workshop/maker-pane.md),
[WL-MAKER-09](../workshop/maker-pane.md), [WL-MAKER-10](../workshop/maker-pane.md),
[WL-MAKER-11](../workshop/maker-pane.md), [WL-MAKER-12](../workshop/maker-pane.md).
