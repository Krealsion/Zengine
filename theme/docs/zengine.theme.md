# zengine.theme -- the theme reader

**Manual.** The theme reader: the weave that reads themes and lays the stacks a host's areas wear,
answering to the role `zengine.theme` (a role is the name a weave answers to). It is the loadable
weave `zengine-theme`; it offers no pane. Other weaves reach it by the messages below.

## About

A theme is the look alone: a name with a window file, a terminal file or both
([Themes](../README.md) says what each holds). This role is the one place a theme file is read, and
it reads the files as they stand each time a stack is laid, so a theme edited on disk and laid
again changes the window without a build.

A host's areas wear stacks of themes, bottom first, and the host keeps them; in Workshop the
layouts' owner keeps one stack a layout. The host says every stack its areas wear, and this role
lays each into one palette and offers it to the graphics directory under the stack's name, its
themes joined by `+` (`night+block`). Each theme is laid over the pool's defaults and the
terminal's own look and over the theme below it. A theme of the other medium alone is a normal link
of the stack, applied in its place and converted as well as it can be: a terminal file's ink is
shown in a window as the colour xterm shows it in, `terminal default` falling through to the theme
below; a window file's colour is shown in a terminal as the nearest of its sixteen inks. Such a
link is said with its warning as it is put in.

A pane's own values a theme sets are judged against what the pane's role declared: one it does
not declare is left out, and said. A role that has not declared yet has its entries laid as they
are.

## Use

Say where the themes are with [`ThemesPlaced`](#themesplaced), and what your areas wear with
[`ThemesWorn`](#themesworn); then choose each area's palette from the directory by the stack's
name. Read what was laid with [`StacksRequested`](#stacksrequested), the themes the folders hold
with [`ThemesRequested`](#themesrequested), and one theme resolved with
[`ThemeResolveRequested`](#themeresolverequested).

## Commands

### `StacksRequested`

`StacksRequested` v1 asks for every stack as it was last laid, answered to you alone.

- **Takes:** nothing.
- **Answers:** `StacksShown` v1: `stacks`, each a `StackLaid` -- its `palette`, the `revision` it was
  offered at, its `links` (each its `theme`, whether it has a `window` file and a `terminal` file,
  its `warning`, and its `refusal` where it was left out), and `skipped`, each own value or entry a
  theme gave that nothing takes, with why.
- **Refuses:** nothing.
- **Example:** `mail.send_to_role("zengine.theme", StacksRequested{}, ask);`

### `ThemesRequested`

`ThemesRequested` v1 asks for every theme the folders hold, answered to you alone.

- **Takes:** nothing.
- **Answers:** `ThemesShown` v1: `themes`, each a `ThemeSummary` -- its `name`, `summary`, whether
  it has a `window` file and a `terminal` file, the themes it `includes`, and the `refusal` of a
  file of it that will not load. A theme of yours wins over a shipped one of its name.
- **Refuses:** nothing.
- **Example:** `mail.send_to_role("zengine.theme", ThemesRequested{}, ask);`

### `ThemeResolveRequested`

`ThemeResolveRequested` v1 resolves one theme with its includes, laying nothing.

- **Takes:** `theme`, its name.
- **Answers:** `ThemeResolved` v1: whether it resolved, its window look (`colors`, `fonts`, `sizes`,
  `styles`, `own`) and its terminal look (`borders`, `inks`), each include applied first and then
  its own entries, the later winning for one name; or the `refusal`.
- **Refuses:** a theme with no file, a cycle of includes, an include with no file of that medium, a
  file that will not read -- a version-1 file, which held settings and keys, among them -- or a
  bound passed, each naming why.
- **Example:** `mail.send_to_role("zengine.theme", ThemeResolveRequested{"block"}, ask);`

### `ThemeSaveRequested`

`ThemeSaveRequested` v1 saves a theme among yours.

- **Takes:** `window` and `theme`, its window file; `terminal` and `terminal_file`, its terminal
  file; either or both; `replace`, to overwrite a theme of that name.
- **Answers:** `ThemeSaved` v1: `name` and `paths`, each file written, or the `refusal`. Every stack
  is laid again.
- **Refuses:** a file not in form, two files of different names, a theme already there without
  `replace`, or a host that names no folder of yours. Nothing is written on a refusal.
- **Example:** `mail.send_to_role("zengine.theme", ThemeSaveRequested{true, file, false, {},
  false}, ask);`

## Hears

### `ThemesPlaced`

`ThemesPlaced` v1, from the host: the folder of your themes ("" for none) and the folder of the
shipped ones.
Every stack worn is laid again from them.

### `ThemesWorn`

`ThemesWorn` v1, from the host: every stack its areas wear, whole, each a `ThemeStack` of its themes,
bottom first. Each is laid from the files as they stand, and offered where its palette changed; a
stack no longer worn is let go. At most `kMaxStacksWorn` stacks of `kMaxStackThemes` themes each
are laid (`theme/vocabulary.hpp`).

### `OwnValuesDeclared`

`OwnValuesDeclared` v1, from any pane, published as its role: the own values it declares, each its name,
its kind and its default. Kept for the role, and every stack laid again.

### `GraphicsReady`

`GraphicsReady` v1, from `zengine.graphics`: a directory started afresh, offered every palette
again.

## Follow

`PaletteOffered` v4 goes to `zengine.graphics` each time a stack's palette changes, at a higher
revision. `StackSaid` v1 is published to any when a stack is laid with something to say -- a link
of the other medium alone, converted; a link left out; an entry nothing takes -- once each time it
changes. On each activation the role publishes `ThemesPlacedRequested` v1, for whoever hosts it to
say where the themes are and what its areas wear, and `OwnValuesAsked` v1, for each pane to declare
its own values again.

## Files

A theme is `<name>.json`, its window file, and `<name>.terminal.json`, its terminal file, in the
folder of yours, else in the shipped folder. Themes are written only by
[`ThemeSaveRequested`](#themesaverequested).

## See also

- [Themes](../README.md): what a theme's files hold, how a theme resolves and a stack is laid, and
  the bounds.
- [`theme/vocabulary.hpp`](../vocabulary.hpp): the role, its words and its shapes.
- [The Graphics package](../../graphics/docs/graphics.md): colours, styles, own values and the
  terminal's look by name.
