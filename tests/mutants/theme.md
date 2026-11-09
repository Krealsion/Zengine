# The mutants `tests/test_theme.cpp` claims

Each entry is a mutant that a comment in [`tests/test_theme.cpp`](../test_theme.cpp) claims its
cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## version-one-read

A version-1 file is read as version 3 would read it.

In `theme/theme_persist.hpp`:

```diff
-    if (c.version == 1) {
+    if (c.version == 1 && false) {
```

Red in `zengine-theme-tests`:

- a version-1 theme, which held settings and keys, is refused saying where they live; a
  version-2 one saying what moved; and a member no file holds by its name

## window-alone-unconverted

A window theme alone leaves a terminal's look as the theme below it.

In `theme/stack.hpp`:

```diff
-                const auto v = rgba_of(c.color);
+                const std::optional<std::int64_t> v = std::nullopt;
```

Red in `zengine-theme-tests`:

- a stack lays each theme over the one below; a theme of the other medium alone is a link
  of it, converted and said with a warning; a link that will not resolve is left out and said

## terminal-default-painted

`terminal default` is given a colour instead of falling through to the theme below.

In `graphics/terminal_inks.hpp`:

```diff
-    if (codes.empty() || codes == "39") {
+    if (codes.empty()) {
```

Red in `zengine-theme-tests`:

- a stack lays each theme over the one below; a theme of the other medium alone is a link
  of it, converted and said with a warning; a link that will not resolve is left out and said

## own-unjudged

An own value its role does not declare is laid, and nothing is said.

In `theme/stack.hpp`:

```diff
-                    if (known && detail::declared_value(d->second, entry, kind) == nullptr) {
+                    if (false && known && detail::declared_value(d->second, entry, kind) == nullptr) {
```

Red in `zengine-theme-tests`:

- a pane's own value a theme sets is laid for its role, and one its role does not declare
  is left out and said; a role not yet declared has its values laid as they are

## offer-unchanged

Every stack worn is offered again whatever changed.

In `theme/weave.cpp`:

```diff
-                offer = !(before == held.palette);
+                offer = true;
```

Red in `zengine-theme-tests`:

- the theme weave asks on its activation for where the themes are and for every own value,
  and offers each stack worn as the palette named for it, laid again only where it changed
