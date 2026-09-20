# A canvas is a pane picture

**Decision record.** The host law is in [canvas](../workshop/canvas.md); the shared contract
is in [panes](../panes.md).

**Context.** A graph author needs nodes, ports and wires within a pane. Prose rows cannot
express their relationships. A whole SurfaceCanvas would give its provider screen authority.
Controls still need the medium's normal type, without fixed-cell lettering's opaque strips.

**Decision.** Publish a bounded local picture of rectangles, fixed labels and measured text
runs. Workshop clips and translates it on the pane's plane. Runs use the existing Surface text
renderer with the ground beneath; a shared helper resolves metric, clip, caret and selection
for both drawing and hit bounds. Room grants bind metrics and geometry to one provider. Held
gestures keep their initial picture and end explicitly. A resize may display the previous
image marked as updating, but clears its admission and input fence immediately.

**Alternatives — argued.** A scenegraph adds transforms and retained identities the provider
can own. A new Surface primitive is unnecessary for thin-rectangle wires or single-line type.
Shrinking a region to clip it changes fit and origin; whole glyph/row clipping instead keeps
its padded region inside the pane. Persisted grants could aim old gestures at a successor.

**Alternatives — tried.** Fixed labels served the first canvas but used bitmap lettering and
opaque strips in SDL. The measured-run witness now compares local hit fit with the existing
Surface projection and sweeps edge clipping in `tests/test_workshop_panes_canvas.cpp`.

**Limits.** Text stays ASCII and unscaled; partially visible glyphs and rows are omitted whole.
No paths, textures, idle hover or multiline clipping tree. The provider owns pan and zoom.
Metric changes renew the grant. Preview never crosses a close, zero room, re-offer, provider
or metric change. The fence orders dispatch; it does not prove physical presentation time.

**Laws supported.** [WL-CANVAS-01](../workshop/canvas.md),
[WL-CANVAS-02](../workshop/canvas.md), [WL-CANVAS-03](../workshop/canvas.md).
