# Themes

**A theme is a look, and only a look: colours, fonts, sizes and the styles made of them, a pane's
own values, and the terminal's inks and borders. It is a name with a window file, a terminal file
or both, plain JSON you write by hand, and it says nothing of settings or keys.**

A theme's two files are paired by its name, each saying its medium in its `format`:

- **`<name>.json`**, its window file (format `zengine-theme`, version 3), the look of a window of
  light:
  - **colors**: the shared pool's colours by name (`background`, `text`, `accent`...), each
    `#rrggbb` or `#rrggbbaa`;
  - **fonts**, **sizes** and **styles**: the pool's fonts by name, each a face; its sizes, `tiny` to
    `huge`, each in points; and its styles (`caption`, `body`, `heading`, `title`), each the font,
    size and colour it names and its weight, a field left out standing as it was;
  - **own**: each role's own values -- the colours, sizes and styles a pane declares where it draws
    -- under the role that declares them.
- **`<name>.terminal.json`**, its terminal file (format `zengine-terminal`, version 1), the
  terminal's look and nothing of a window's:
  - **inks**: the pool's colours by name, each the ink a terminal shows it in (`terminal default`,
    or one of its sixteen, perhaps `bright`, perhaps `bold`) and the character it fills a shape
    with;
  - **borders**: a corner, across and down.

Either file may have a **summary** and **includes**: other themes whose files of its medium it is
built from, applied first and in order.

```json
{ "format": "zengine-theme", "format_version": 3, "name": "evening",
  "summary": "light ink on a warm ground", "includes": ["night"],
  "colors": [ { "name": "background", "color": "#2e2a28" },
              { "name": "accent", "color": "#e0a050" } ],
  "styles": [ { "name": "heading", "color": "accent" } ],
  "own": [ { "role": "zengine.flow", "colors": [ { "name": "wire", "color": "#88c0d0" } ] } ] }
```

```json
{ "format": "zengine-terminal", "format_version": 1, "name": "evening",
  "borders": "+-|", "inks": [ { "name": "accent", "ink": "yellow", "fill": "O" } ] }
```

A member a file does not hold is refused by its name, so a misspelling such as `colours` is an
error, never a silence. A version-1 theme, which held settings and keys, is refused, saying where
they live: a pane's settings are kept by the layouts' owner, and keys are the keymap's own layers.
A version-2 window file, which gave each colour its terminal ink and named own values `slots`, is
refused saying what moved.

A theme resolves with its includes depth first, one medium at a time: each include whole, in order,
then the file's own entries, the later winning for one name. A theme met again on the path that
includes it is a cycle, and a resolution never goes half done: a cycle, a missing or broken
include, or a bound passed refuses the whole of it, naming why.

## A stack

A host's areas wear **stacks** of themes, bottom first, and the host keeps them: in Workshop the
layouts' owner keeps one a layout, and a pane's row may lay one more theme over its layout's for
that pane alone. The `zengine.theme` role, the loadable weave `zengine-theme`
([its manual](docs/zengine.theme.md)), lays each stack into one palette, named for its themes
joined by `+` (`night+block`), and offers it to graphics; a stack of none wears the pool's defaults,
night's look. Each theme is laid over the pool's defaults and the terminal's own look and over the
theme below it. A theme of the other medium alone is a normal link of the stack, applied in its
place and converted as well as it can be -- a colour to its nearest ink, an ink to the colour xterm
shows it in, `terminal default` falling through to the theme below -- and said with a warning as
it is put in. A pane's own value a theme sets that the pane's role does not declare is left out and
said. Where no theme weave runs (the snake, an app), the host offers its own palettes.

The shipped themes, `night`, `day` and `block`, stand in `themes/` beside the host, each with both
files; the weaver's own stand in `themes/` under the host's per-user configuration root, and one of
the weaver's wins over a shipped one of its name.

## Bounds

A window file holds at most <!-- value kMaxThemeColors -->128<!-- /value --> colours,
<!-- value kMaxThemeFonts -->16<!-- /value --> fonts,
<!-- value kMaxThemeSizes -->32<!-- /value --> sizes,
<!-- value kMaxThemeStyles -->32<!-- /value --> styles and the own values of
<!-- value kMaxThemeOwnRoles -->32<!-- /value --> roles,
<!-- value kMaxThemeOwnEntries -->64<!-- /value --> in all; a terminal file
<!-- value kMaxThemeInks -->128<!-- /value --> inks; each at most
<!-- value kMaxThemeIncludes -->8<!-- /value --> includes in
<!-- value kMaxThemeBytes KiB -->64<!-- /value --> KiB. A theme's name is 1 to
<!-- value kMaxThemeNameLen -->32<!-- /value --> bytes of `a`-`z`, `0`-`9` and `-`, and is its
files' name. One resolution reads at most <!-- value kMaxThemesResolved -->16<!-- /value -->
themes and nests at most <!-- value kMaxThemeDepth -->4<!-- /value --> includes below the theme
asked for. A stack lays at most <!-- value kMaxStackThemes -->8<!-- /value --> themes, and a host's
areas wear at most <!-- value kMaxStacksWorn -->48<!-- /value --> stacks.

A theme is written only by the safe write every durable file here shares (`persist/`), and a theme
already on disk is overwritten only when asked to.

## What is in this folder

| | |
|---|---|
| `theme_persist.hpp` | a theme's two files: their shapes, their form, reading and writing them, resolving a theme with its includes, and listing a folder of themes |
| `stack.hpp` | how a stack is laid, as values: each link over the one below, the other medium's converted |
| `stack_name.hpp` | the name of a stack's palette and a pane's theme laid over its layout's, and a stack's bounds, which a host names its areas' palettes by |
| `vocabulary.hpp` | the `zengine.theme` role's messages |
| `weave.cpp` | the `zengine-theme` weave, which holds the role |
| `docs/zengine.theme.md` | its manual |
| `CMakeLists.txt` | the build: `zengine-theme-vocabulary`, header-only, and the weave |

Working on it: [AGENTS.md](AGENTS.md).
