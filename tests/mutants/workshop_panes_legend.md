# The mutants `tests/test_workshop_panes_legend.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_legend.cpp`](../test_workshop_panes_legend.cpp) claims its cases catch,
the comment naming the entry by its path and heading. [The corpus's front page](README.md) says how
an entry reads and how `tests/mutants/run.py` runs it.

## picture-carries-the-panes-keys

The layouts' owner's picture of the desk carries each pane's own settings to Workshop.

In `workshop/layouts/store.cpp`:

```diff
-                      [](const PaneSetting& s) { return !is_desk_setting_key(s.key); });
+                      [](const PaneSetting& s) { return false && !is_desk_setting_key(s.key); });
```

Red in `zengine-workshop-panes-tests`:

- a layout keeps the Terminal's legend hidden while another shows it, written through Info's rows
