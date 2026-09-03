# Pane boundary rungs

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [chrome](../workshop/chrome.md).

**Context.** Panes were rectangles with no boundary: their backdrop was `kMuted`, the workspace's
own role, so a pane was a hole in the dot field, and on the shipped face near-black on near-black
(`65cf9f1`, "Give the desk edges, and let the pane you are using come forward"). The first
boundary paid the smallest thing a terminal can draw — one whole cell — on both faces, because
one number was the only way to put it in the same place in each; on the shipped window that was
twelve pixels a side (`fd7b78a`, "Let the graphical desk frame its panes with a pixel, not a
cell"). Then the Layouts pane became two cells tall, and one cell a side left zero rows of the two
the band always had (`3bfc2fd`).

**Decision.** Every ordinary pane and framed transient surface draws a boundary of one unit of
the active face on every side, subtracted from the rectangle `bounds_of` answered; the pane
owns that it has chrome, the face owns how finely it can be drawn (`chrome_grain(sc)` from the
medium's reported `cell_px`). `pane_inside` tries the face's unit, then one cell, then no
boundary, and returns the first that leaves an interior the face can set, resolving the interior
and its presentation together. The backdrop is the border. Every body resolution goes through
`pane_inside`. A content-sized surface reserves the coarsest boundary. Selection changes the ink
and not one number. Three chrome roles, from the closed vocabulary.

**Alternatives considered.**
- *A frame grown outward around the authored rectangle* — rejected: the rectangle a maker
  authors is the rectangle the pane occupies; nothing grew outward (`65cf9f1`).
- *A border painter with a thickness argument* — rejected: the ring is the backdrop minus the
  interior, so there is no border arithmetic to drift; pinned by case `"WUX-8: the ring IS the
  backdrop the interior did not cover, on both faces"`.
- *One cell on every face* — the first implementation, superseded: measured on the shipped
  window the interior went from 552x84 to 574x106 pixels, four body rows of the face to five.
- *A pixel inset on a face that describes the interior in cells* — rejected: the inset is
  projected away and the body spills over its own left and top edge, leaving a ring on two
  sides; pinned by case `"WUX-8: a face that describes an interior in CELLS pays the cell"`.
- *A fifth chrome role or a per-medium palette* for a transient edge over bare workspace —
  refused: `surface/vocabulary.hpp` refuses a fifth role.
- *Sizing a content popup for pixels* — rejected: it would cut a row off itself the moment a
  terminal drew it, and its placement would depend on the face.

**Consequences.** Measured on a terminal: every stack pane's interior is 46x7 where the slot is
48x9, so the Builder drops its lowest-priority row, the picker windows two entries sooner, the
Info lists lose a row each, and the Compose form no longer fits the default slot — closed at the
desk by [the coarse step](why-the-coarse-step-is-four.md). On the shipped window a 576x108 slot
has a 574x106 interior, one pixel a side, and nothing pads it back. The rungs cannot oscillate
because each candidate is a larger interior than the last; a two-cell pane exists on every face,
wears no ring on a terminal and no selected ink on the last rung; `chrome_subs == 0` on the cell
medium.

**Laws supported.** [WL-CHROME-01](../workshop/chrome.md), [WL-CHROME-02](../workshop/chrome.md),
[WL-CHROME-03](../workshop/chrome.md), [WL-CHROME-04](../workshop/chrome.md),
[WL-CHROME-05](../workshop/chrome.md), [WL-CHROME-06](../workshop/chrome.md),
[WL-CHROME-07](../workshop/chrome.md), [WL-CHROME-08](../workshop/chrome.md).
