# The mutants `tests/test_workshop_panes_guide.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_guide.cpp`](../test_workshop_panes_guide.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## stacks-never-sent

Workshop may not send the stacks its layouts wear, so the theme weave lays none and the desk wears
the pool's own look whatever its layout names.

In `workshop/grant.cpp`:

```diff
-    speak.allow_to_role(theme::ThemesWorn::zen_name, theme::ThemesWorn::zen_version, theme::kThemeRole);
+    (void)theme::ThemesWorn::zen_name;
```

Red in `zengine-workshop-panes-tests`:

- a mark in a terminal is its part's own cells in the guide colour: every row of the pane reads as
  before, and its label stands on a body row wholly free or is not drawn, the guest told which
