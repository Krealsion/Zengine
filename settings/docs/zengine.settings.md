# zengine.settings -- the theme reader

**Manual.** The theme reader: the weave that reads themes and applies them, answering to the
office `zengine.settings` (an office is the name a weave answers to). It is the loadable weave
`zengine-settings`, which Workshop's plan loads; it offers no pane. Other weaves reach it by the
messages below.

## About

A theme is a JSON file you write by hand: the colours, fonts and sizes it gives the shared pool,
the slots each office exposes, the settings and keys it writes, and the themes it includes first
([Settings](../README.md) says what a file holds). This office is the one place a theme file is
read. It reads the file as it stands each time it is asked, so a theme edited on disk and applied
again changes the window without a build.

Each theme in the folders Workshop names is offered to the graphics directory as a palette under
the theme's name: the pool's colours with the theme's over them. The palette a layout's areas wear
is the one named by the layout's baseline theme, else the theme Workshop's own areas wear. A theme
applied to a layout is judged whole, against what takes each entry, before anything is written:
a setting against the declarations the layouts' owner shows, a key against the actions the keymap
holds, a colour against the pool, a slot against its office's declaration. An entry nothing takes
is skipped and said, or refuses the whole theme, as the theme's `on_failure` says; a value of the
wrong kind refuses it whatever the theme says. Then its settings and baselines are written through
the layouts' owner, its keys through the keymap's door, and its look offered again as the palette
the layout wears, at a higher revision, so every window repaints and no pane is asked anything.

## Use

Apply a theme with [`ThemeApplyRequested`](#themeapplyrequested). With `merge` (the default) what
the theme does not name stays as it is: a terminal look such as `block` is worn over `night` or
`day` this way. With `reset` everything the theme touches returns to its default first, so the
theme applies whole. Settings locked on a layout are a final layer: the theme leaves them and the
answer lists them. Colours carry no locks.

A theme's slots for an office the load plan names that has not declared its slots yet wait for it,
and apply when it declares ([`SlotsDeclared`](#slotsdeclared)); an entry its declaration does not
hold is then skipped, and said in [`ThemeSkipsSaid`](#follow).

## Commands

### `ThemeApplyRequested`

`ThemeApplyRequested` v2 applies a theme to layouts, answered to you alone.

- **Takes:** `theme`, its name; `layouts`, the ids of the layouts it applies to, none for the live
  one, or `all_layouts`; `only_tags`, to apply only the settings whose row carries one of these;
  `mode`, `merge` or `reset`; `baseline`, to make it the layouts' baseline too, which chooses the
  palette their areas wear.
- **Answers:** `ThemeActed` v2: `written` and `unchanged`, the settings written and found already
  so; `skipped`, each entry nothing took, with why; `locked`, the settings locks kept; `keys`, how
  many actions' keys were set; `palette` and `revision`, the palette the live layout wears and the
  revision offered; `waiting`, each office whose slots wait for it to declare; `baseline`, whether
  the baseline moved.
- **Refuses:** a theme that will not resolve, naming why; an entry nothing takes, where the theme
  refuses on a failure, naming it; a value of the wrong kind or a colour that is no colour, always;
  a layout id no layout has; a mode other than `merge` or `reset`; more applies at once than
  `settings/vocabulary.hpp`'s `kMaxAppliesInFlight`. Nothing is written on a refusal.
- **Example:** `mail.send_to_role("zengine.settings", ThemeApplyRequested{"block", {}, false, {},
  "merge", false}, ask);` wears `block` over the live layout's palette.

### `ThemesRequested`

`ThemesRequested` v1 asks for every theme in the folders.

- **Takes:** nothing.
- **Answers:** `ThemesShown` v2: each theme by name, its summary, includes, and how many settings,
  keys and entries of its look it holds; a broken file by its name with its refusal. A theme of
  yours wins over a shipped one of its name.
- **Refuses:** nothing.
- **Example:** `mail.send_to_role("zengine.settings", ThemesRequested{}, ask);`

### `ThemeResolveRequested`

`ThemeResolveRequested` v1 resolves a theme with its includes and applies nothing.

- **Takes:** `theme`, its name.
- **Answers:** `ThemeResolved` v2: the chain in the order each theme applied, the theme's own
  `on_failure`, its look, its settings and keys, each naming the theme that gave it, and its
  fingerprint.
- **Refuses:** a cycle, a missing or broken include, or a bound passed, naming why.
- **Example:** `mail.send_to_role("zengine.settings", ThemeResolveRequested{"day"}, ask);`

### `ThemeSaveRequested`

`ThemeSaveRequested` v2 writes a theme among yours, by the safe write.

- **Takes:** `theme`, the whole theme; `replace`, to write over one of its name.
- **Answers:** `ThemeSaved` v1: the name and the file written.
- **Refuses:** a theme out of form; one of its name already there without `replace`; a Workshop
  that keeps none of yours, as an isolated launch keeps none.
- **Example:** `mail.send_to_role("zengine.settings", ThemeSaveRequested{theme, false}, ask);`

## Hears

### `ThemesPlaced`

`ThemesPlaced` v1, from `zengine.workshop` alone: the folder of your themes, the folder of the
shipped ones, the offices the load plan names, and the theme Workshop's own areas wear. Each theme
found that is not yet offered is offered then.

### `SlotsDeclared`

`SlotsDeclared` v1, from an office, as that office: the slots it exposes to themes, each colour
with its default and the pool colour a terminal shows it as, and each size with its points. Kept
for the office, and every theme's entries that waited for it judged then.

### `GraphicsReady`

`GraphicsReady` v1, from `zengine.graphics`: a directory started afresh, offered every palette
again.

### `ThemeSnapshotRequested`

`ThemeSnapshotRequested` v1, `ThemePromoteRequested` v1 and `ThemeProvenanceRequested` v1 are
refused, in the words `this office does not answer this door yet`.

## Follow

`PaletteOffered` v1 goes to `zengine.graphics` each time a palette changes: once for each theme
found, again whenever a theme applied changes the palette a layout wears, and again for each
palette that held entries waiting for an office once it declares. `ThemeSkipsSaid` v1 is published
to any once an office the plan named has declared: the theme, the palette, and each entry of it
that waited and that the declaration does not hold. Workshop says it on its band.

## Files

Each theme is `<name>.json` in the folder of yours, `themes/` under Workshop's per-user
configuration root, else in the folder shipped beside Workshop, which holds `night`, `day` and
`block`. A theme file of version 1 is read as version 2 would hold it. Themes are written only by
[`ThemeSaveRequested`](#themesaverequested), as version 2.

## See also

- [Settings](../README.md): what a theme file holds, how it resolves, and its bounds.
- [`settings/vocabulary.hpp`](../vocabulary.hpp): the office, its words and its shapes.
- [The layouts' owner](../../workshop/docs/zengine.layouts.md), whose doors write a theme's
  settings and baselines.
