# The mutants `tests/test_settings_apply.cpp` claims

Each entry is a mutant that a comment in [`tests/test_settings_apply.cpp`](../test_settings_apply.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## bad-colour-taken

A theme's colour is judged by its length alone, so a value that is no colour is taken.

In `settings/theme_persist.hpp`:

```diff
-    if (!c.color.empty() && !rgba_of(c.color)) {
+    if (!c.color.empty() && c.color.size() != 7 && c.color.size() != 9) {
```

Red in `zengine-settings-themes-tests`:

- a value that is no colour, or a terminal word no terminal shows, refuses the theme whatever
  on_failure says

## refuse-read-as-skip

A theme's `on_failure` is unread: every entry nothing takes is skipped.

In `settings/apply.hpp`:

```diff
-    const bool skip = theme.on_failure == kOnFailureSkip;
+    const bool skip = true;
```

Red in `zengine-settings-themes-tests`:

- an entry nothing takes is skipped and said where the theme says skip, and refuses it whole where
  it says refuse

## reset-as-merge

A theme applied by `reset` is applied as by `merge`.

In `settings/apply.hpp`:

```diff
-    const bool reset = asked.mode == kApplyReset;
+    const bool reset = false;
```

Red in `zengine-settings-themes-tests`:

- merge writes what the theme names and leaves the rest; reset also returns every other key of
  each pane it names to its default

## planned-slots-refused

Slots of an office the plan names that has not declared are read as slots nothing exposes.

In `settings/apply.hpp`:

```diff
-            if (detail::holds(world.planned, s.office)) {
+            if (false) {
```

Red in `zengine-settings-themes-tests`:

- an office the plan names that has not declared its slots waits, and what it declares applies when
  it does

## sizes-left-out

A look over a palette sets the pool's sizes from none of a theme's.

In `settings/apply.hpp`:

```diff
-            if (into.name == z.name) into.points = z.points;
+            if (false && into.name == z.name) into.points = z.points;
```

Red in `zengine-settings-themes-tests`:

- a theme's fonts and sizes are judged against the pool's and set by name in the palette, a slot's
  size in its office's
