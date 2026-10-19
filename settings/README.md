# Settings

**A theme is a named set of settings and keys kept in a file of its own, to be kept, shared and
built on: one theme may include others, and a setting may name every pane, an office or one pane.**

Each theme is one file, `<name>.json` in a folder of themes, format `zengine-theme`. It holds:

- **settings**, each with a target: `*` (every pane that takes the key), an office such as
  `zengine.workshop` (an office-wide setting), or one pane, `<office>/<pane>`. A setting holds one
  value, a flag, a number or a text, or is a clear, which clears what an earlier theme set;
- **keys**, each an action and the gesture that asks for it, in the keymap's own spelling (`none`
  included);
- **includes**: other themes, applied first and in order.

A theme resolves with its includes depth first: each include whole, in order, then the theme's own
entries, the later winning for one setting under one target and for one action. Each resolved entry
names the theme that gave it. A theme met again on the path that includes it is a cycle, and a
resolution never goes half done: a cycle, a missing or broken include, or a bound passed refuses
the whole of it, naming why. A theme included twice by different paths is no cycle; its later
application wins.

A file holds at most <!-- value kMaxThemeSettings -->512<!-- /value --> settings,
<!-- value kMaxThemeKeys -->128<!-- /value --> keys and
<!-- value kMaxThemeIncludes -->8<!-- /value --> includes in
<!-- value kMaxThemeBytes KiB -->64<!-- /value --> KiB; its name is 1 to
<!-- value kMaxThemeNameLen -->32<!-- /value --> bytes of `a`-`z`, `0`-`9` and `-`, and is the
file's name. One resolution reads at most <!-- value kMaxThemesResolved -->16<!-- /value -->
themes, nests at most <!-- value kMaxThemeDepth -->4<!-- /value --> includes below the theme asked
for, and resolves to at most <!-- value kMaxResolvedEntries -->1024<!-- /value --> entries.

A theme is written only by the safe write every durable file here shares. A theme already on
disk is overwritten only when asked to and, when the bytes it was read from are given, only while
it still holds them.

The `zengine.settings` office's messages are declared in `vocabulary.hpp`; no weave holds the
office, so nothing answers them.

## What is in this folder

| | |
|---|---|
| `theme_persist.hpp` | the theme file: its shapes, its form, reading and writing it, resolving a theme with its includes, and listing a folder of themes |
| `vocabulary.hpp` | the `zengine.settings` office's messages: apply, save, snapshot, promote, list, resolve and provenance |
| `CMakeLists.txt` | the build: `zengine-settings-vocabulary`, header-only |

Working on it: [AGENTS.md](AGENTS.md).
