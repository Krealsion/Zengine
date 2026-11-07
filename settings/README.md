# Settings

**A theme is a named set of colours, fonts and sizes, settings and keys, kept in a JSON file of its
own, to be kept, shared and built on: one theme may include others, a setting may name every pane,
an office or one pane, and a theme is read each time it is applied, so editing its file and
applying it again changes the window with no build.**

Each theme is one file, `<name>.json`, format `zengine-theme` version 2, plain JSON you write by
hand. The weaver's own stand in `themes/` under Workshop's per-user configuration root; the shipped
ones, `night`, `day` and `block`, stand in `themes/` beside Workshop, and one of yours wins over a
shipped one of its name. A theme holds:

- **colors**: the shared pool's colours by name (`background`, `text`, `accent`...), each its value
  (`#rrggbb` or `#rrggbbaa`), what a terminal shows for it (`cells`: `terminal default`, or one of
  its sixteen inks, perhaps `bright`, perhaps `bold`) and the character a terminal fills a shape with
  in it (`fill`), any of the three left out leaving that part as it stands;
- **fonts** and **sizes**: the pool's fonts by name, each a face, and its text sizes, each in points;
- **slots**: each office's own colours and sizes, under the office that exposes them;
- **outline**: a terminal's corner, across and down;
- **settings**, each with a target: `*` (every pane that takes the key), an office such as
  `zengine.workshop` (an office-wide setting), or one pane, `<office>/<pane>`. A setting holds one
  value, a flag, a number or a text, or is a clear, which clears what an earlier theme set;
- **keys**, each an action and every gesture that asks for it, in the keymap's own spelling (`none`
  alone for no key);
- **includes**: other themes, applied first and in order;
- **on_failure**: what applying it does with an entry nothing takes, `refuse` (the default: the
  whole theme is refused) or `skip` (the entry is skipped and said).

```json
{ "format": "zengine-theme", "format_version": 2, "name": "evening",
  "summary": "light ink on a warm ground", "includes": ["night"], "on_failure": "skip",
  "colors": [ { "name": "background", "color": "#2e2a28", "cells": "terminal default" },
              { "name": "accent", "color": "#e0a050", "cells": "yellow" } ],
  "slots": [ { "office": "zengine.flow",
               "colors": [ { "name": "wire", "color": "#88c0d0", "cells": "cyan" } ] } ],
  "keys": [ { "action": "layout.rename", "gestures": ["ctrl+r", "alt+r"] } ] }
```

A member the file does not hold is refused by its name, so a misspelling such as `colours` is an
error, never a silence. A file of version 1, which held settings and keys alone in Loom's envelope,
is read as version 2 would hold it: `refuse` on a failure, each key one gesture. A theme is written
as version 2.

A theme resolves with its includes depth first: each include whole, in order, then the theme's own
entries, the later winning for one colour, font, size or slot by its name, for one setting under one
target, and for one action. Each resolved setting and key names the theme that gave it. A theme met
again on the path that includes it is a cycle, and a resolution never goes half done: a cycle, a
missing or broken include, or a bound passed refuses the whole of it, naming why. A theme included
twice by different paths is no cycle; its later application wins.

## Applying a theme

The `zengine.settings` office, the loadable weave `zengine-settings`, reads and applies themes
([its manual](docs/zengine.settings.md)). Each theme in the folders is offered as a palette under
its own name, its look over the pool's defaults, and the palette a layout's areas wear is the one its
baseline theme names, else the one Workshop's own areas wear (`workshop.theme`). Applying a theme to
a layout:

- **judges it whole first**: a setting against the declarations the layouts' owner shows, a key
  against the actions the keymap holds, a colour, font or size against the pool, a slot against its
  office's declaration. An entry nothing takes is skipped and said, or refuses the theme, as its
  `on_failure` says; a value of the wrong kind (`#12345g`, a number for a flag) refuses it whatever
  `on_failure` says. Nothing is written on a refusal;
- **merges or resets**: `merge` applies what the theme names and leaves the rest; `reset` returns
  the rest of each pane it names, every key the keymap file moves, and the colours of the palette the
  layout wears to their defaults, then applies the theme whole;
- **keeps locks as a final layer**: a locked setting stands, and the answer lists it. Colours carry
  no locks; your own colours are a theme of yours, included last;
- **waits for an office the load plan names**: its slots apply when it declares them, and an entry
  its declaration does not hold is then skipped and said on Workshop's band.

Its settings and baselines go through the layouts' owner at the revision it read, its keys through
the keymap's edit door, the first gesture of each set and the rest added, and its look as a new
revision of the palette the layout wears. Where no settings weave runs (the snake, `--isolated`),
Workshop paints with the three shipped palettes it was built with.

## Bounds

A file holds at most <!-- value kMaxThemeSettings -->512<!-- /value --> settings,
<!-- value kMaxThemeKeys -->128<!-- /value --> keys of at most
<!-- value kMaxThemeGestures -->8<!-- /value --> gestures each,
<!-- value kMaxThemeColors -->128<!-- /value --> colours,
<!-- value kMaxThemeFonts -->16<!-- /value --> fonts,
<!-- value kMaxThemeSizes -->32<!-- /value --> sizes, the slots of
<!-- value kMaxThemeSlotOffices -->32<!-- /value --> offices and
<!-- value kMaxThemeIncludes -->8<!-- /value --> includes in
<!-- value kMaxThemeBytes KiB -->64<!-- /value --> KiB; its name is 1 to
<!-- value kMaxThemeNameLen -->32<!-- /value --> bytes of `a`-`z`, `0`-`9` and `-`, and is the
file's name. One resolution reads at most <!-- value kMaxThemesResolved -->16<!-- /value -->
themes, nests at most <!-- value kMaxThemeDepth -->4<!-- /value --> includes below the theme asked
for, and resolves to at most <!-- value kMaxResolvedEntries -->1024<!-- /value --> entries.

A theme is written only by the safe write every durable file here shares (`persist/`). A theme
already on disk is overwritten only when asked to and, when the bytes it was read from are given,
only while it still holds them.

## What is in this folder

| | |
|---|---|
| `theme_persist.hpp` | the theme file: its shapes, its form, reading and writing it, resolving a theme with its includes, and listing a folder of themes |
| `apply.hpp` | how a theme applies, as values: each entry judged, the batch, the keymap's edits and the palette it gives |
| `slots.hpp` | the slots an office exposes, as it says them to the settings office |
| `vocabulary.hpp` | the `zengine.settings` office's messages: apply, save, list, resolve, what Workshop tells it and what it says later |
| `weave.cpp` | the `zengine-settings` weave, which holds the office |
| `docs/zengine.settings.md` | its manual |
| `CMakeLists.txt` | the build: `zengine-settings-vocabulary`, header-only, and the weave |

Working on it: [AGENTS.md](AGENTS.md).
