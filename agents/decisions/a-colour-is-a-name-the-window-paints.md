# A colour is a name the window paints

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [settings](../workshop/settings.md).

**Context.** A draw named a role, a medium chose an ink for it, and a skin -- a library holding a
painter and a table of colours -- painted each area. Themes, a pane's own colours and a terminal
that cannot show every colour all wanted colours as data, which no painter's table could be.

**Decision.** A draw carries a `Color`: an exact value, a pool name or an own value its pane
declares where it draws, by name with its default; a text carries a style the same way. The pool's
names are C++ names generated from JSON, so a name nobody declared does not build. The window turns
a name into a colour, a style into its fields, when it paints, from the palette the area wears, and
hands its painter exact colours; the painter knows none. A palette is offered whole at a revision, and a new one paints
every area wearing it again with nothing drawn on the bus. The skin seam retires: the painter is the
window's own, one of each medium behind one interface (`PaintTask` in, `PaintResult` out), and
another way of painting is another window. The skins become themes, generated at build time; which
ship is [its own record](night-day-and-block-ship.md).

**Alternatives considered.**
- *Argued: keep the skin seam, colour-free* -- a stranger could ship a painter without a window, at
  the cost of an ABI kept for no look to choose and a crossing a paint.
- *Argued: names checked at run time* -- a misspelt name would paint as `text` without a word.
- *Argued: the painter resolving names from a table it is handed* -- two places would know what a
  name looks like.

**Consequences.** Where the theme role runs it offers every palette, one a stack of themes a host's
areas wear, and the host names the one each area wears; the snake and an app offer their own. A
terminal's look is its own, beside a palette's colours. Drawings in the operands' first versions
are refused in words. A GPU painter is packaging behind the same interface, not a redesign.

**Laws supported.** [WL-SETTING-13](../workshop/settings.md).
