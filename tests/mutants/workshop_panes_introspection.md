# The mutants `tests/test_workshop_panes_introspection.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_introspection.cpp`](../test_workshop_panes_introspection.cpp) claims its
cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## row-at-fixed-widths

Powers draws its chrome row at `kViewWidth` and `kGap` whatever the layout keeps.

In `introspection/powers.hpp`:

```diff
-        const std::string gap(static_cast<std::size_t>(ui.widths.gap), ' ');
+        const std::string gap(static_cast<std::size_t>(detail::kGap), ' ');
```

In `introspection/powers.hpp`:

```diff
-        column_of(text, 0, ui.widths.view);
+        column_of(text, 0, detail::kViewWidth);
```

In `introspection/powers.hpp`:

```diff
-            composite_at = static_cast<std::int64_t>(text.size()) + ui.widths.gap;
+            composite_at = static_cast<std::int64_t>(text.size()) + detail::kGap;
```

Red in `zengine-workshop-panes-tests`:

- Powers declares its chrome row's widths as settings, and a layout's widths pad its view control
  and space the row's parts

## paste-asked-elsewhere

Powers asks the graphics directory for a paste, not the window showing its area.

In `introspection/seats.hpp`:

```diff
-        const std::string office = room().clipboard_office();
+        const std::string office = "zengine.graphics";
```

Red in `zengine-workshop-panes-tests`:

- Powers asks the window that shows it for a paste into its query, the OS window or the terminal's
