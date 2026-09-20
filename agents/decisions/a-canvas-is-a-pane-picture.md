# A canvas is a pane picture

**Decision record.** The host law is in [canvas](../workshop/canvas.md); the shared contract
is in [panes](../panes.md).

**Context.** A graph author needs nodes, ports and wires at positions within a pane. Prose
rows cannot express those relationships. Letting the provider publish a whole SurfaceCanvas
would make it a second screen owner and bypass Workshop's placement and front order.

**Decision.** Add an optional bounded local picture: rectangles and fixed-size labels, clipped
and translated by Workshop on the pane's existing plane. One room token binds the local extent
to its provider. Content and coordinate input echo it; held pointer gestures explicitly end,
retain their initiating picture, and target the original provider identity. The existing menu
presenter, keyboard router and medium picture fence remain the owners of their existing facts.

**Alternatives — argued.** A general scenegraph would add transforms, retained identities and
hit testing the first consumer can own itself. A new Surface renderer primitive would widen
every medium for wires expressible today as thin rectangles. Reusing prose row numbers as
coordinates would falsely claim the pane's drawing follows the active font metric. Retaining
room tokens across reload would let an image interpret a gesture aimed at its predecessor.

**Alternatives — tried.** None before this implementation; the alternatives are architectural
arguments rather than measured failed implementations.

**Limits.** Labels use fixed canvas lettering, with ASCII bytes and whole glyph edge clipping.
No paths, texture, font scaling or idle hover is added. The provider owns pan and zoom; Loom's
in-place reload still requires a fresh offer and the provider dropping transient grant state.
The fence proves dispatch ordering, not when a physical display presented a frame.

**Laws supported.** [WL-CANVAS-01](../workshop/canvas.md),
[WL-CANVAS-02](../workshop/canvas.md), [WL-CANVAS-03](../workshop/canvas.md).
