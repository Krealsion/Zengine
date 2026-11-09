# Agent law — Themes: the look alone, a stack, and the other medium's link

Routed detail behind [`theme/AGENTS.md`](../theme/AGENTS.md), for tasks touching `theme/` or a host
that names the stacks its areas wear. Public reference: [Themes](../theme/README.md); the names a
theme sets are graphics' ([graphics](graphics.md)).

## A theme is the look alone

- **A theme is a name with a window file, a terminal file or both**, paired by that name, each
  file's `format` saying its medium (`zengine-theme`, `zengine-terminal`;
  `theme/theme_persist.hpp`): the window file a window of light's colours, fonts, sizes, styles and
  each role's own values, the terminal file each colour's ink and fill and the border characters,
  and neither anything of the other's.
- **Nothing here reads or writes a setting or a key**: a version-1 theme, which held settings and
  keys, is refused in words naming where they live -- a pane's settings with the layouts' owner,
  keys in the keymap's own layers -- and nothing of it is read (`kVersionOneRefused`).
- **A file is read as it stands each time a stack is laid**, refused whole on any fault and named
  by it: an unknown member, a value of the wrong kind, a bound passed, a cycle of includes, an
  include with no file of that medium.

## A stack

- **The host keeps the stack its areas wear**, bottom first; the theme role keeps none of its own
  and names no host. A host says where the themes are (`ThemesPlaced`) and every stack worn
  (`ThemesWorn`), and chooses each area's palette by its stack's name (`stack_palette`, its themes
  joined by `+`; a stack of none wears the window's own).
- **One owner per palette name**: where the theme role runs it offers every stack's palette, the
  shipped looks among them, and the host offers none; a host without it offers its own.
- **A stack lays each theme over the pool's defaults and the terminal's own look and over the theme
  below it** (`lay_stack`), and a palette is offered again only where what was laid changed.
- **A theme of the other medium alone is a normal link**, applied in its place and converted as
  well as it can be -- a colour to its nearest ink (`nearest_terminal_ink`), an ink to the colour
  xterm shows it in (`kTerminalInkColors`, `ink_rgba`), `terminal default` falling through to the
  theme below -- and said with a warning as it is put in (`StackSaid`).
- **A pane's own value a theme sets is judged against what its role declared**
  (`OwnValuesDeclared`): one it does not declare is left out and said; a role that has not declared
  has its values laid as they are.

## A change by kind

- **A colour changed is the window's own repaint**: no owner is told. **A size, a font or a style
  changed is a new room** (`AreaRoom`, carrying the measures), which each owner of an area wearing
  the palette hears and lays its text out again by, in its own `on`, or ignores. Nothing keeps a
  dirty flag.
